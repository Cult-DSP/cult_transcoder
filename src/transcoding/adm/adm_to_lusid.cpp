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
// adm_to_lusid.cpp — ADM XML → LUSID Scene JSON conversion
//
// Preserves the original ADM encounter-order conversion.
//
// Key parity rules (AGENTS §3, §5):
//   - DirectSpeakers appear in t=0 frame, in encounter order.
//   - Objects appear after DirectSpeakers, also in encounter order.
//   - Frames are in ascending time order.
//   - LFE is channel 4 (1-based) when _DEV_LFE_HARDCODED = True.
//   - containsAudio is NOT used — all channels assumed active.
//   - timeUnit is always "seconds".
//   - IDs are "{group_counter}.1" — group_counter starts at 1.
// ---------------------------------------------------------------------------

#include "transcoding/adm/adm_to_lusid.hpp"
#include "transcoding/adm/admHelper.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace cult {

// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
namespace {

// ---------------------------------------------------------------------------
// Developer flag — matches Python's _DEV_LFE_HARDCODED = True
// ---------------------------------------------------------------------------
constexpr bool kLfeHardcoded = true;

// Helper implementations live in adm_helpers (admHelper.hpp).

// ---------------------------------------------------------------------------
// Position extraction — mirrors _get_position_coords()
// ---------------------------------------------------------------------------
struct Vec3 { double x = 0, y = 0, z = 0; };

Vec3 getPositionCoords(pugi::xml_node blockElem) {
    Vec3 v;
    for (auto pos : blockElem.children("position")) {
        const char* coord = pos.attribute("coordinate").as_string("");
        double val = 0.0;
        if (pos.text()) val = pos.text().as_double(0.0);
        if (std::strcmp(coord, "X") == 0)      v.x = val;
        else if (std::strcmp(coord, "Y") == 0)  v.y = val;
        else if (std::strcmp(coord, "Z") == 0)  v.z = val;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Find the ebuCoreMain element — mirrors _find_ebu_root()
// ---------------------------------------------------------------------------
pugi::xml_node findEbuRoot(pugi::xml_document& doc) {
    auto root = doc.document_element();

    // Case 1: root IS ebuCoreMain
    std::string rootName = root.name();
    if (rootName == "ebuCoreMain") return root;

    // Case 2: bwfmetaedit wrapper — find <aXML> then ebuCoreMain inside
    auto axml = root.child("aXML");
    if (!axml.empty()) {
        // Try without namespace (pugixml default mode strips ns prefixes)
        auto ebu = axml.child("ebuCoreMain");
        if (!ebu.empty()) return ebu;
    }

    // Fallback: search entire tree
    auto ebu = root.find_node([](pugi::xml_node n) {
        return std::string(n.name()) == "ebuCoreMain";
    });
    return ebu;
}

// ---------------------------------------------------------------------------
// JSON helpers — manual serializer matching Python json.dump(indent=2)
// ---------------------------------------------------------------------------

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Format a double the way Python does: 0.0 stays 0.0, others use
// enough precision to be exact.  Python json.dump uses repr-style.
std::string fmtDouble(double v) {
    // If it's an integer value, print with .0
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        return buf;
    }
    // Otherwise, use enough digits
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    // Python's json.dump outputs minimal repr. We need to match.
    // Try progressively fewer digits until round-trip breaks.
    for (int prec = 6; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        double roundtrip = 0;
#ifdef _MSC_VER
#pragma warning(suppress: 4996)
#endif
        std::sscanf(buf, "%lf", &roundtrip);
        if (roundtrip == v) return buf;
    }
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

std::string indent(int level) {
    return std::string(static_cast<size_t>(level) * 2, ' ');
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// convertAdmDocumentToLusid() — shared conversion for an already-loaded ADM XML
// document. Used by the public file/buffer APIs and by transcode() after ADM
// profile detection has already parsed the XML.
// ---------------------------------------------------------------------------
ConversionResult convertAdmDocumentToLusid(pugi::xml_document& doc,
                                            LfeMode lfeMode) {
    ConversionResult result;

    // -- Find EBU root --
    auto ebuRoot = findEbuRoot(doc);
    if (ebuRoot.empty()) {
        result.warnings.push_back(
            "Could not find ebuCoreMain element in XML — returning empty scene");
        result.scene.sampleRate = 48000;
        result.success = true;
        result.numFrames = 0;
        return result;
    }

    // -- Extract global data (from <Technical> section) --
    // Python: extract_global_data(tree) — root.find(".//Technical")
    int sampleRate = 48000;
    std::string durationStr;
    auto docRoot = doc.document_element();
    // Search descendants (Python uses .//Technical which is a recursive search)
    pugi::xml_node technical;
    auto techNodes = adm_helpers::findAllDescendants(docRoot, "Technical");
    if (!techNodes.empty()) {
        technical = techNodes[0];
    }
    if (!technical.empty()) {
        auto sr = technical.child("SampleRate");
        if (!sr.empty() && sr.text())
            sampleRate = sr.text().as_int(48000);
        auto dur = technical.child("Duration");
        if (!dur.empty() && dur.text())
            durationStr = dur.text().as_string("");
    } else {
        result.warnings.push_back("No <Technical> section found in XML");
    }

    // -- Parse duration --
    double durationSeconds = -1.0;
    if (!durationStr.empty()) {
    durationSeconds = adm_helpers::parseTimecodeToSeconds(durationStr);
    }

    // -- Metadata --
    // Python: metadata = {"sourceFormat": "ADM"}
    //         if duration_str: metadata["duration"] = duration_str
    //         if global_data.get("Format"): metadata["format"] = global_data["Format"]
    result.scene.metadata["sourceFormat"] = "ADM";
    if (!durationStr.empty())
        result.scene.metadata["duration"] = durationStr;
    if (!technical.empty()) {
        auto fmt = technical.child("Format");
        if (!fmt.empty() && fmt.text())
            result.scene.metadata["format"] = fmt.text().as_string("");
    }

    // =====================================================================
    // DIRECT SPEAKERS → single frame at t=0 (static position)
    // Mirrors Python: extract_direct_speaker_data() + loop in
    //   parse_adm_xml_to_lusid_scene()
    // =====================================================================
    struct SpeakerInfo {
        std::string channelID;
        std::string channelName;
        double x = 0, y = 0, z = 0;
        std::string speakerLabel;
    };

    std::vector<SpeakerInfo> directSpeakers;

    // Iterate all audioChannelFormat elements in document order
    // Python uses ebu_root.iter(_ebu("audioChannelFormat")) which is a
    // recursive descendant search in encounter order.
    auto channelFormats = adm_helpers::findAllDescendants(ebuRoot, "audioChannelFormat");
    for (auto& channel : channelFormats) {
        std::string typeDef = channel.attribute("typeDefinition").as_string("");
        if (typeDef != "DirectSpeakers") continue;

        SpeakerInfo si;
        si.channelName = channel.attribute("audioChannelFormatName").as_string("Unnamed");
        si.channelID = channel.attribute("audioChannelFormatID").as_string("");

        // Find first audioBlockFormat child
        auto block = channel.child("audioBlockFormat");
        if (block.empty()) continue;

        auto pos = getPositionCoords(block);
        si.x = pos.x;
        si.y = pos.y;
        si.z = pos.z;

        auto labelElem = block.child("speakerLabel");
        if (!labelElem.empty() && labelElem.text())
            si.speakerLabel = labelElem.text().as_string("");

        directSpeakers.push_back(si);
    }

    // =====================================================================
    // BUILD LUSID SCENE — mirrors parse_adm_xml_to_lusid_scene() body
    // =====================================================================
    int groupCounter = 1;
    int numDirectSpeakers = static_cast<int>(directSpeakers.size());

    // time → nodes map (Python: time_to_nodes dict)
    std::map<double, std::vector<LusidNode>> timeToNodes;

    // -- Direct Speakers at t=0 --
    int channel1based = 0;
    for (auto& si : directSpeakers) {
        channel1based++;
        int groupId = channel1based;

        // LFE detection — controlled by lfeMode (AGENTS §11.2, §11.4)
        bool isLfe = false;
        if (lfeMode == LfeMode::Hardcoded) {
            // Phase 2 default: LFE = 4th DirectSpeaker encountered (1-based).
            // Mirrors Python _is_lfe_channel() with _DEV_LFE_HARDCODED = True.
            isLfe = kLfeHardcoded && (channel1based == 4);
        } else {
            // SpeakerLabel mode (Phase 4 opt-in): LFE = speakerLabel is
            // "LFE" or "LFE1" (case-insensitive). Channel-4 rule not applied.
            std::string lbl = si.speakerLabel;
            std::transform(lbl.begin(), lbl.end(), lbl.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            isLfe = (lbl == "lfe" || lbl == "lfe1");
        }

        if (isLfe) {
            LusidNode node;
            node.id = std::to_string(groupId) + ".1";
            node.type = "LFE";
            node.hasCart = false;
            timeToNodes[0.0].push_back(node);
            continue;
        }

        // Normal DirectSpeaker
        LusidNode node;
        node.id = std::to_string(groupId) + ".1";
        node.type = "direct_speaker";
        node.cart[0] = si.x;
        node.cart[1] = si.y;
        node.cart[2] = si.z;
        node.hasCart = true;
        node.speakerLabel = si.speakerLabel;
        node.channelID = si.channelID;
        timeToNodes[0.0].push_back(node);
    }

    groupCounter = numDirectSpeakers + 1;

    // -- Audio Objects across frames --
    // Reuse the channelFormats encounter-order list, but write object block
    // nodes directly instead of staging every block in an intermediate vector.
    for (auto& channel : channelFormats) {
        std::string typeDef = channel.attribute("typeDefinition").as_string("");
        if (typeDef != "Objects") continue;

        auto firstBlock = channel.child("audioBlockFormat");
        if (firstBlock.empty()) continue;

        int objGroup = groupCounter;
        groupCounter++;

        for (auto block : channel.children("audioBlockFormat")) {
            double timeSec = adm_helpers::parseTimecodeToSeconds(
                block.attribute("rtime").as_string("00:00:00.00000"));
            auto pos = getPositionCoords(block);

            LusidNode node;
            node.id = std::to_string(objGroup) + ".1";
            node.type = "audio_object";
            node.cart[0] = pos.x;
            node.cart[1] = pos.y;
            node.cart[2] = pos.z;
            node.hasCart = true;
            timeToNodes[timeSec].push_back(node);
        }
    }

    // -- Build frames sorted by time --
    // Python: for time_val in sorted(time_to_nodes.keys())
    for (auto& [timeVal, nodes] : timeToNodes) {
        if (nodes.empty()) continue;
        LusidFrame frame;
        frame.time = std::round(timeVal * 1e6) / 1e6; // round(time_val, 6)
        frame.nodes = std::move(nodes);
        result.scene.frames.push_back(std::move(frame));
    }

    // -- Fill scene metadata --
    result.scene.version = "1.0";
    result.scene.timeUnit = "seconds";
    result.scene.sampleRate = sampleRate;
    if (durationSeconds >= 0.0)
        result.scene.duration = durationSeconds;

    // -- Summary stats --
    result.numFrames = static_cast<int>(result.scene.frames.size());
    for (auto& frame : result.scene.frames) {
        for (auto& node : frame.nodes) {
            result.countsByNodeType[node.type]++;
        }
    }

    result.success = true;
    return result;
}

// ---------------------------------------------------------------------------
// convertAdmToLusid() — public API, parses from file path
// ---------------------------------------------------------------------------
ConversionResult convertAdmToLusid(const std::string& xmlPath, LfeMode lfeMode) {
    ConversionResult result;

    pugi::xml_document doc;
    // Parse without namespace processing so element names appear without
    // the {ns} prefix, matching Python's ElementTree iter() behavior when
    // we search by local name.  However, EBU ADM XML typically has xmlns on
    // the root element which means child names are namespace-qualified in
    // pugixml's default mode.  We need to handle both cases.
    auto parseResult = doc.load_file(xmlPath.c_str());
    if (!parseResult) {
        result.errors.push_back(
            "Failed to parse XML: " + std::string(parseResult.description()) +
            " at offset " + std::to_string(parseResult.offset));
        return result;
    }

    return convertAdmDocumentToLusid(doc, lfeMode);
}

// ---------------------------------------------------------------------------
// convertAdmToLusidFromBuffer() — public API, parses from in-memory string
//
// Phase 3: used when the XML was extracted from a BW64 WAV in-memory by
// extractAxmlFromWav(). Avoids a disk write+re-read cycle. All parity rules
// from AGENTS-CULT §3/§5 apply identically — same document conversion call.
// ---------------------------------------------------------------------------
ConversionResult convertAdmToLusidFromBuffer(const std::string& xmlBuffer,
                                              LfeMode lfeMode) {
    ConversionResult result;

    pugi::xml_document doc;
    auto parseResult = doc.load_string(xmlBuffer.c_str());
    if (!parseResult) {
        result.errors.push_back(
            "Failed to parse XML buffer: " + std::string(parseResult.description()) +
            " at offset " + std::to_string(parseResult.offset));
        return result;
    }

    return convertAdmDocumentToLusid(doc, lfeMode);
}

// ---------------------------------------------------------------------------
// lusidSceneToJson — serialize to stable pretty-printed LUSID Scene JSON
// ---------------------------------------------------------------------------
std::string lusidSceneToJson(const LusidScene& scene) {
    std::ostringstream o;
    o << "{\n";
    o << indent(1) << "\"version\": \"" << jsonEscape(scene.version) << "\",\n";
    o << indent(1) << "\"timeUnit\": \"" << jsonEscape(scene.timeUnit) << "\",\n";

    // "frames" array
    o << indent(1) << "\"frames\": [\n";
    for (size_t fi = 0; fi < scene.frames.size(); ++fi) {
        auto& frame = scene.frames[fi];
        o << indent(2) << "{\n";
        o << indent(3) << "\"time\": " << fmtDouble(frame.time) << ",\n";
        o << indent(3) << "\"nodes\": [\n";

        for (size_t ni = 0; ni < frame.nodes.size(); ++ni) {
            auto& node = frame.nodes[ni];
            o << indent(4) << "{\n";
            o << indent(5) << "\"id\": \"" << jsonEscape(node.id) << "\",\n";
            o << indent(5) << "\"type\": \"" << jsonEscape(node.type) << "\"";

            if (node.hasCart) {
                o << ",\n";
                o << indent(5) << "\"cart\": [\n";
                o << indent(6) << fmtDouble(node.cart[0]) << ",\n";
                o << indent(6) << fmtDouble(node.cart[1]) << ",\n";
                o << indent(6) << fmtDouble(node.cart[2]) << "\n";
                o << indent(5) << "]";
            }

            // speakerLabel and channelID for direct_speaker nodes
            if (node.type == "direct_speaker") {
                if (!node.speakerLabel.empty()) {
                    o << ",\n";
                    o << indent(5) << "\"speakerLabel\": \"" << jsonEscape(node.speakerLabel) << "\"";
                }
                if (!node.channelID.empty()) {
                    o << ",\n";
                    o << indent(5) << "\"channelID\": \"" << jsonEscape(node.channelID) << "\"";
                }
            }

            o << "\n" << indent(4) << "}";
            if (ni + 1 < frame.nodes.size()) o << ",";
            o << "\n";
        }

        o << indent(3) << "]\n";
        o << indent(2) << "}";
        if (fi + 1 < scene.frames.size()) o << ",";
        o << "\n";
    }
    o << indent(1) << "]";

    // Optional: duration
    if (scene.duration >= 0.0) {
        o << ",\n";
        o << indent(1) << "\"duration\": " << fmtDouble(scene.duration);
    }

    // Optional: sampleRate
    if (scene.sampleRate > 0) {
        o << ",\n";
        o << indent(1) << "\"sampleRate\": " << scene.sampleRate;
    }

    // Optional: metadata
    if (!scene.metadata.empty()) {
        o << ",\n";
        o << indent(1) << "\"metadata\": {\n";
        size_t mi = 0;
        for (auto& [k, v] : scene.metadata) {
            o << indent(2) << "\"" << jsonEscape(k) << "\": \"" << jsonEscape(v) << "\"";
            if (mi + 1 < scene.metadata.size()) o << ",";
            o << "\n";
            mi++;
        }
        o << indent(1) << "}";
    }

    o << "\n}";
    return o.str();
}

// ---------------------------------------------------------------------------
// writeLusidScene — write JSON to file
// ---------------------------------------------------------------------------
bool writeLusidScene(const LusidScene& scene, const std::string& outPath) {
    std::string json = lusidSceneToJson(scene);
    std::ofstream f(outPath);
    if (!f.is_open()) return false;
    f << json;
    f.close();
    return f.good();
}

} // namespace cult
