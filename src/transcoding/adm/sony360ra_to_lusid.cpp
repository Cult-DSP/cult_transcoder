// Copyright 2026 Cult-DSP
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ---------------------------------------------------------------------------
// src/transcoding/adm/sony360ra_to_lusid.cpp — Sony 360RA ADM → LUSID converter
//
// Phase 6A implementation.  See sony360ra_to_lusid.hpp for design decisions.
//
// XML structure navigated:
//   conformance_point_document/File/aXML/format/audioFormatExtended
//       audioProgramme  (programme name — detection signal already consumed)
//       audioContent
//       audioObject[AO_8001]  ← container: has <audioObjectIDRef> — SKIP
//       audioObject[AO_1001..AO_100d]  ← leaf objects: have <audioTrackUIDRef>
//           audioPackFormatIDRef  → AP_0003XXXX (Objects type)
//           audioTrackUIDRef      → ATU_XXXXXXXX
//       audioPackFormat[AP_0003XXXX]
//           audioChannelFormatIDRef → AC_0003XXXX
//       audioChannelFormat[AC_0003XXXX]  typeDefinition="Objects"
//           audioBlockFormat  (one or more — gain-mute pattern)
//               cartesian       "0" (polar)
//               gain            linear — IGNORED (see §15.5)
//               position[@coordinate="azimuth"]
//               position[@coordinate="elevation"]
//               position[@coordinate="distance"]
//
// Note on <Technical> section: the conformance_point_document wrapper has a
// <Technical> child under <File> that carries SampleRate and Duration.
// This is used for scene.sampleRate and scene.duration.
// ---------------------------------------------------------------------------

#include "transcoding/adm/sony360ra_to_lusid.hpp"
#include "transcoding/adm/admHelper.hpp"
#include "transcoding/adm/adm_to_lusid.hpp"

#include <pugixml.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cult {

// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
namespace {

// Shared ADM helpers live in adm_helpers (admHelper.hpp).

// ---------------------------------------------------------------------------
// polarToCart()
//
// ADM polar → LUSID Cartesian (see AGENTS-CULT §15.7):
//   az_rad = azimuth  * π/180
//   el_rad = elevation * π/180
//   x = dist * sin(az_rad) * cos(el_rad)   right  = +x
//   y = dist * cos(az_rad) * cos(el_rad)   front  = +y
//   z = dist * sin(el_rad)                  up     = +z
// ---------------------------------------------------------------------------
struct Cart3 { double x = 0, y = 0, z = 0; };

static Cart3 polarToCart(double az_deg, double el_deg, double dist) {
    constexpr double kPi = 3.14159265358979323846;
    const double az_rad = az_deg * (kPi / 180.0);
    const double el_rad = el_deg * (kPi / 180.0);
    Cart3 c;
    c.x = dist * std::sin(az_rad) * std::cos(el_rad);
    c.y = dist * std::cos(az_rad) * std::cos(el_rad);
    c.z = dist * std::sin(el_rad);
    return c;
}

// ---------------------------------------------------------------------------
// isContainerObject()
//
// Returns true if this audioObject is a container (has <audioObjectIDRef>
// children). Container objects group leaf objects but carry no track data.
// Rule: skip any audioObject that has at least one <audioObjectIDRef> child.
// See AGENTS-CULT §15.6 for future container metadata consideration.
// ---------------------------------------------------------------------------
static bool isContainerObject(pugi::xml_node obj) {
    return !obj.child("audioObjectIDRef").empty();
}

// ---------------------------------------------------------------------------
// getFirstActiveBlockPosition()
//
// Given an audioChannelFormat node, finds the first audioBlockFormat with
// gain > 0 (or absent gain, which implies 1.0) and returns its polar coords.
// Returns false if no valid block is found (all blocks muted or channel empty).
//
// This implements Option A (single static frame) — AGENTS-CULT §15.4.
// The gain-mute pattern (0.01024s silence blocks at section boundaries) is
// discarded. The gain value itself is not stored (see §15.5).
// ---------------------------------------------------------------------------
static bool getFirstActiveBlockPosition(
    pugi::xml_node channelFmt,
    double& out_az, double& out_el, double& out_dist)
{
    for (auto block : channelFmt.children("audioBlockFormat")) {
        // Read gain (default 1.0 if absent)
        double gainVal = 1.0;
        auto gainElem = block.child("gain");
        if (!gainElem.empty() && gainElem.text()) {
            gainVal = gainElem.text().as_double(1.0);
        }

        // Skip muted blocks (gain == 0.0)
        if (gainVal == 0.0) continue;

        // Also skip zero-distance blocks (Sony uses dist=0 + az=0 + el=0
        // on silence blocks even when gain value might be ambiguous)
        double az = 0.0, el = 0.0, dist = 1.0;
        for (auto pos : block.children("position")) {
            const char* coord = pos.attribute("coordinate").as_string("");
            double val = pos.text().as_double(0.0);
            if (std::strcmp(coord, "azimuth") == 0)        az   = val;
            else if (std::strcmp(coord, "elevation") == 0)  el   = val;
            else if (std::strcmp(coord, "distance") == 0)   dist = val;
        }

        // Distance == 0 is the silence position marker; skip it.
        if (dist == 0.0) continue;

        out_az   = az;
        out_el   = el;
        out_dist = dist;
        return true;
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// convertSony360RaToLusid() — public API
// ---------------------------------------------------------------------------
ConversionResult convertSony360RaToLusid(
    const pugi::xml_document& doc,
    LfeMode lfeMode)
{
    ConversionResult result;

    // ------------------------------------------------------------------
    // 1. Warn if lfe-mode flag was supplied (it has no effect on 360RA)
    // ------------------------------------------------------------------
    if (lfeMode != LfeMode::Hardcoded) {
        result.warnings.push_back(
            "lfe-mode flag has no effect on Sony360RA input "
            "(360RA has no DirectSpeakers or LFE channels)");
    }
    result.warnings.push_back(
        "Sony360RA profile detected — using sony360ra_to_lusid converter "
        "(Phase 6A, ADM-variant mode)");

    // ------------------------------------------------------------------
    // 2. Navigate to <audioFormatExtended>
    // ------------------------------------------------------------------
    auto admRoot = adm_helpers::findAdmRoot(doc);
    if (admRoot.empty()) {
        result.errors.push_back(
            "Sony360RA converter: could not find <audioFormatExtended> in document. "
            "Expected conformance_point_document/File/aXML/format/audioFormatExtended.");
        return result;
    }

    // ------------------------------------------------------------------
    // 3. Extract sample rate and duration from <Technical>
    // ------------------------------------------------------------------
    int sampleRate = 48000;
    double durationSec = -1.0;
    std::string durationStr;

    auto technical = adm_helpers::findTechnicalNode(doc);
    if (!technical.empty()) {
        auto sr = technical.child("SampleRate");
        if (!sr.empty() && sr.text())
            sampleRate = sr.text().as_int(48000);

        auto dur = technical.child("Duration");
        if (!dur.empty() && dur.text()) {
            durationStr = dur.text().as_string("");
            durationSec = adm_helpers::parseTimecodeToSeconds(durationStr.c_str());
        }
    } else {
        result.warnings.push_back(
            "Sony360RA: no <Technical> section found — defaulting sampleRate=48000, duration unknown");
    }

    // ------------------------------------------------------------------
    // 4. Build a map: audioPackFormatID → audioChannelFormatID
    //    (each 360RA pack format has exactly one channel format ref)
    // ------------------------------------------------------------------
    //    AP_0003XXXX → AC_0003XXXX
    std::map<std::string, std::string> packToChannel;
    {
    auto packFormats = adm_helpers::findAllDescendants(admRoot, "audioPackFormat");
        for (auto& pf : packFormats) {
            // Only type 0003 (Objects) — 360RA leaf packs
            std::string typeLabel = pf.attribute("typeLabel").as_string("");
            if (typeLabel != "0003") continue;

            std::string pfID = pf.attribute("audioPackFormatID").as_string("");
            auto chRef = pf.child("audioChannelFormatIDRef");
            if (chRef.empty() || !chRef.text()) continue;
            packToChannel[pfID] = chRef.text().as_string("");
        }
    }

    // ------------------------------------------------------------------
    // 5. Build a map: audioChannelFormatID → audioChannelFormat node
    // ------------------------------------------------------------------
    std::map<std::string, pugi::xml_node> channelMap;
    {
    auto channelFormats = adm_helpers::findAllDescendants(admRoot, "audioChannelFormat");
        for (auto& cf : channelFormats) {
            std::string cfID = cf.attribute("audioChannelFormatID").as_string("");
            if (!cfID.empty())
                channelMap[cfID] = cf;
        }
    }

    // ------------------------------------------------------------------
    // 6. Iterate leaf audioObjects in document encounter order.
    //    Each leaf object has exactly one <audioTrackUIDRef> and one
    //    <audioPackFormatIDRef>. Container objects are skipped.
    // ------------------------------------------------------------------
    LusidFrame frame;
    frame.time = 0.0;

    int nodeCounter = 1;
    int leafObjectsFound = 0;
    int leafObjectsConverted = 0;

    auto allObjects = adm_helpers::findAllDescendants(admRoot, "audioObject");
    for (auto& obj : allObjects) {
        // Skip container objects
        if (isContainerObject(obj)) continue;

        leafObjectsFound++;

        // Resolve: audioObject → audioPackFormatIDRef → audioChannelFormatIDRef
        //                      → audioChannelFormat → audioBlockFormat
        auto packRefElem = obj.child("audioPackFormatIDRef");
        if (packRefElem.empty() || !packRefElem.text()) {
            result.warnings.push_back(
                std::string("Sony360RA: audioObject '") +
                obj.attribute("audioObjectID").as_string("?") +
                "' has no audioPackFormatIDRef — skipped");
            nodeCounter++;  // still advance counter to maintain encounter order
            continue;
        }
        std::string packID = packRefElem.text().as_string("");

        auto packIt = packToChannel.find(packID);
        if (packIt == packToChannel.end()) {
            result.warnings.push_back(
                std::string("Sony360RA: audioPackFormat '") + packID +
                "' not found (referenced by audioObject '" +
                obj.attribute("audioObjectID").as_string("?") + "') — skipped");
            nodeCounter++;
            continue;
        }
        std::string channelID = packIt->second;

        auto chIt = channelMap.find(channelID);
        if (chIt == channelMap.end()) {
            result.warnings.push_back(
                std::string("Sony360RA: audioChannelFormat '") + channelID +
                "' not found — skipped");
            nodeCounter++;
            continue;
        }
        auto channelFmt = chIt->second;

        // Extract first active (non-muted, non-zero-distance) block position
        double az = 0.0, el = 0.0, dist = 1.0;
        if (!getFirstActiveBlockPosition(channelFmt, az, el, dist)) {
            result.warnings.push_back(
                std::string("Sony360RA: audioChannelFormat '") + channelID +
                "' has no active audioBlockFormat (all blocks muted or zero-distance) — "
                "node added at origin");
            az = 0.0; el = 0.0; dist = 1.0;
        }

        // Convert polar → Cartesian
        Cart3 cart = polarToCart(az, el, dist);

        // Build LUSID node
        LusidNode node;
        node.id   = std::to_string(nodeCounter) + ".1";
        node.type = "audio_object";
        node.cart[0] = cart.x;
        node.cart[1] = cart.y;
        node.cart[2] = cart.z;
        node.hasCart = true;
        // NOTE: gain is intentionally NOT written — see AGENTS-CULT §15.5

        frame.nodes.push_back(std::move(node));
        nodeCounter++;
        leafObjectsConverted++;
    }

    if (leafObjectsFound == 0) {
        result.errors.push_back(
            "Sony360RA: no leaf audioObjects found in document "
            "(all objects are containers or document has no audioObjects)");
        return result;
    }

    if (leafObjectsConverted == 0) {
        result.errors.push_back(
            "Sony360RA: found " + std::to_string(leafObjectsFound) +
            " leaf audioObjects but none could be converted "
            "(missing pack/channel refs — check warnings)");
        return result;
    }

    // ------------------------------------------------------------------
    // 7. Build LusidScene — single frame at t=0
    // ------------------------------------------------------------------
    result.scene.version  = "1.0";
    result.scene.timeUnit = "seconds";
    result.scene.sampleRate = sampleRate;
    if (durationSec >= 0.0)
        result.scene.duration = durationSec;

    result.scene.metadata["sourceFormat"] = "Sony360RA-ADM";
    if (!durationStr.empty())
        result.scene.metadata["duration"] = durationStr;

    result.scene.frames.push_back(std::move(frame));

    // ------------------------------------------------------------------
    // 8. Summary stats
    // ------------------------------------------------------------------
    result.numFrames = 1;
    result.countsByNodeType["audio_object"] = leafObjectsConverted;

    result.success = true;
    return result;
}

} // namespace cult
