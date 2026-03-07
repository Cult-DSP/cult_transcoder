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
// transcoding/adm/adm_profile_resolver.cpp — ADM Profile Resolver (Phase 4)
//
// Implements resolveAdmProfile(). Read-only attribute-string pass over an
// already-parsed pugi::xml_document. No ADM semantic model used.
//
// Detection heuristics (AGENTS §11.3, priority order):
//   Pass 1: audioProgramme/@audioProgrammeName
//     - contains "atmos"  (case-insensitive) → DolbyAtmos
//     - contains "360ra"  (case-insensitive) → Sony360RA
//   Pass 2: audioPackFormat/@audioPackFormatName
//     - contains "atmos"  (case-insensitive) → DolbyAtmos
//     - contains "360ra"  (case-insensitive) → Sony360RA
//   Fallback: Unknown
//
// NOTE: bare "360" is intentionally NOT a signal — too broad. Only "360ra".
// ---------------------------------------------------------------------------

#include "adm_profile_resolver.hpp"
#include "pugixml.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace cult {

namespace {

// ---------------------------------------------------------------------------
// toLower() — ASCII-only lowercase for attribute value comparison
// ---------------------------------------------------------------------------
static std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

// ---------------------------------------------------------------------------
// checkAttribute() — inspect a single attribute value against known signals.
// Returns non-Unknown if a signal is found, and fills detectedFrom.
// ---------------------------------------------------------------------------
static AdmProfile checkAttribute(const std::string& attrValue,
                                  const std::string& attrName,
                                  std::string& detectedFrom) {
    const std::string lower = toLower(attrValue);

    // Priority: Atmos before 360RA within the same attribute
    if (lower.find("atmos") != std::string::npos) {
        detectedFrom = attrName + "=\"" + attrValue + "\"";
        return AdmProfile::DolbyAtmos;
    }
    if (lower.find("360ra") != std::string::npos) {
        detectedFrom = attrName + "=\"" + attrValue + "\"";
        return AdmProfile::Sony360RA;
    }
    return AdmProfile::Unknown;
}

// ---------------------------------------------------------------------------
// profileName() — human-readable profile name for warnings
// ---------------------------------------------------------------------------
static std::string profileName(AdmProfile p) {
    switch (p) {
        case AdmProfile::DolbyAtmos: return "DolbyAtmos";
        case AdmProfile::Sony360RA:  return "Sony360RA";
        case AdmProfile::Generic:    return "Generic";
        case AdmProfile::Unknown:    return "Unknown";
    }
    return "Unknown";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// resolveAdmProfile() — public implementation
// ---------------------------------------------------------------------------
ProfileResult resolveAdmProfile(const pugi::xml_document& doc) {
    ProfileResult result; // profile = Unknown, detectedFrom = "", warnings = {}

    // -------------------------------------------------------------------------
    // Pass 1 — audioProgramme/@audioProgrammeName
    // Traverse the entire document tree — handles both ebuCoreMain wrapper
    // and bare audioFormatExtended root (e.g. Sony 360RA files).
    // -------------------------------------------------------------------------
    for (const pugi::xpath_node& xnode :
         doc.select_nodes("//audioProgramme")) {
        const std::string attrVal =
            xnode.node().attribute("audioProgrammeName").as_string();
        if (attrVal.empty()) continue;

        AdmProfile detected = checkAttribute(
            attrVal, "audioProgrammeName", result.detectedFrom);
        if (detected != AdmProfile::Unknown) {
            result.profile = detected;
            result.warnings.push_back(
                "ADM profile detected: " + profileName(detected) +
                " (via " + result.detectedFrom + ")");
            return result;
        }
    }

    // -------------------------------------------------------------------------
    // Pass 2 — audioPackFormat/@audioPackFormatName
    // -------------------------------------------------------------------------
    for (const pugi::xpath_node& xnode :
         doc.select_nodes("//audioPackFormat")) {
        const std::string attrVal =
            xnode.node().attribute("audioPackFormatName").as_string();
        if (attrVal.empty()) continue;

        AdmProfile detected = checkAttribute(
            attrVal, "audioPackFormatName", result.detectedFrom);
        if (detected != AdmProfile::Unknown) {
            result.profile = detected;
            result.warnings.push_back(
                "ADM profile detected: " + profileName(detected) +
                " (via " + result.detectedFrom + ")");
            return result;
        }
    }

    // -------------------------------------------------------------------------
    // Fallback — Unknown (caller treats as Generic)
    // No warning emitted for Unknown; silence means standard EBU ADM.
    // -------------------------------------------------------------------------
    return result;
}

} // namespace cult

// Read AGENTS-CULT.md §11 in full before implementing. This file contains
// only design notes and is the authoritative stub. Do not implement anything
// here until the owner confirms Phase 3 pipeline testing is complete (§11.9).
// ═══════════════════════════════════════════════════════════════════════════
//
// ── Purpose ─────────────────────────────────────────────────────────────────
//
//   Inspect an already-parsed pugi::xml_document and detect which ADM profile
//   variant the file represents. Returns an AdmProfile enum + detection
//   provenance string. Caller (transcoder.cpp) uses this for report logging
//   and to inform LFE mode selection.
//
//   In Phase 4 the profile detection is READ-ONLY. It does not change the
//   ADM→LUSID conversion logic directly — that is done via the --lfe-mode
//   CLI flag. Full profile-driven conversion is deferred to Phase 5+.
//
// ── Public API (implement in adm_profile_resolver.hpp) ──────────────────────
//
//   namespace cult {
//
//   enum class AdmProfile {
//       Generic,     // Standard BS.2076, no tool-specific signals found
//       DolbyAtmos,  // "Atmos" signal detected
//       Sony360RA,   // "360RA" signal detected (NOT bare "360" — too broad)
//       Unknown,     // XML present but no profile signals — treat as Generic
//   };
//
//   struct ProfileResult {
//       AdmProfile profile = AdmProfile::Unknown;
//       std::string detectedFrom;          // e.g. "audioProgrammeName='Atmos_Master'"
//       std::vector<std::string> warnings; // logged to report warnings[]
//   };
//
//   ProfileResult resolveAdmProfile(const pugi::xml_document& doc);
//
//   } // namespace cult
//
// ── Detection heuristics (implement in priority order) ──────────────────────
//
//   STEP 1 — audioProgramme/@audioProgrammeName (highest confidence):
//     XPath-equivalent: //audioProgramme/@audioProgrammeName
//     - contains "Atmos" (case-insensitive)  → DolbyAtmos
//     - contains "360RA" or "360" (case-insensitive) → Sony360RA
//
//   STEP 2 — audioPackFormat/@audioPackFormatName (lower confidence):
//     XPath-equivalent: //audioPackFormat/@audioPackFormatName
//     - contains "Atmos" (case-insensitive)  → DolbyAtmos
//     - contains "360RA" (case-insensitive)  → Sony360RA
//
//   STEP 3 — fallthrough:
//     → Unknown (caller treats as Generic for all conversion purposes)
//
//   Implementation note: use pugi::xml_node::find_node() or manual child
//   iteration. Do NOT use libadm — this is pure attribute-string inspection.
//   Case-insensitive comparison: convert attribute value to lowercase before
//   checking (std::transform + ::tolower). Include <algorithm> and <cctype>.
//
//   The `detectedFrom` string should be human-readable for the report, e.g.:
//     "audioProgrammeName='Atmos_Master' (node: ituADM > audioProgramme)"
//
// ── LFE mode integration (adm_to_lusid.cpp, NOT this file) ──────────────────
//
//   This file does NOT implement LFE detection. LFE mode lives in
//   adm_to_lusid.cpp inside parseAdmDocument(). The new LfeMode enum belongs
//   in adm_to_lusid.hpp. See AGENTS-CULT §11.4 for the exact parameter
//   addition and the SpeakerLabel branch logic.
//
//   The --lfe-mode CLI flag is parsed in transcoder.cpp and passed through
//   TranscodeRequest. This file's ProfileResult is used only to emit a
//   warning to the report when a non-Generic profile is detected.
//
// ── CMake activation ─────────────────────────────────────────────────────────
//
//   When implementing Phase 4, add this file to CMakeLists.txt in both
//   the cult-transcoder and cult_tests source lists (same pattern as
//   adm_reader.cpp was added in Phase 3):
//
//     transcoding/adm/adm_profile_resolver.cpp
//
//   Also create transcoding/adm/adm_profile_resolver.hpp (public API header).
//   The transcoding/adm/ directory is already in include paths — no new
//   path entry needed.
//
// ── Open questions (must be answered by owner before implementing) ───────────
//
//   Q1: Does sourceData/sony360RA_example.xml use speakerLabel="LFE" or "LFE1"
//       or a different convention?  Run:
//         grep -i "speakerLabel\|LFE" sourceData/sony360RA_example.xml | head -20
//       The answer determines whether the SpeakerLabel branch in
//       parseAdmDocument() catches LFE correctly for 360RA files.
//
//   Q2: Should Phase 4 maintain oracle parity for the 360RA fixture, or is the
//       Phase 4 goal specifically to produce BETTER output than the oracle?
//       If parity is required: run python oracle first, commit reference LUSID,
//       then use it as the test baseline. If improvement is the goal: document
//       what the expected difference is before implementing.
//
//   Q3: Is profile-driven bed channel remapping (beyond LFE) required in Phase 4
//       or deferred to Phase 5? Only the owner can answer this.
//
//   Q4: Should --lfe-mode be forwarded from runRealtime.py to cult-transcoder?
//       If yes, realtime_runner.py RealtimeConfig needs a new field.
//       If no, the flag is CLI/testing only and the pipeline always uses default.
//
// ── Fixtures required before Phase 4 tests can be written ───────────────────
//
//   1. tests/parity/fixtures/sony_360ra_example.xml
//      Source: copy from spatialroot/sourceData/sony360RA_example.xml
//      (file confirmed present in spatialroot repo).
//      Do not modify the source file. Do not rename it in sourceData/.
//
//   2. tests/parity/fixtures/sony_360ra_reference.lusid.json
//      Generate with Python oracle BEFORE Phase 4 implementation:
//        cd /Users/lucian/projects/spatialroot && source activate.sh
//        python3 -c "
//          from LUSID.src.xml_etree_parser import parse_adm_xml_to_lusid_scene
//          import json
//          scene = parse_adm_xml_to_lusid_scene(
//              'sourceData/sony360RA_example.xml', contains_audio=None)
//          print(json.dumps(scene.to_dict(), indent=2))
//        " > cult_transcoder/tests/parity/fixtures/sony_360ra_reference.lusid.json
//      Commit this file before touching any C++ code.
//
// ── Dependencies ─────────────────────────────────────────────────────────────
//
//   - pugixml (already in build via FetchContent) — #include "pugixml.hpp"
//   - No new third-party libraries required for Phase 4.
//   - #include <string>, <vector>, <algorithm>, <cctype>
//
// ── References ───────────────────────────────────────────────────────────────
//
//   - AGENTS-CULT.md §11 (full Phase 4 pre-flight notes — read this first)
//   - AGENTS-CULT.md §4 (LFE behavior pinned decisions)
//   - AGENTS-CULT.md §10 (MPEG-H strategy for Phase 6)
//   - DEV-PLAN-CULT.md Phase 4 (work items table)
//   - DESIGN-DOC-V1-CULT.MD §ADM and Object Metadata (ADM background)
// ---------------------------------------------------------------------------
