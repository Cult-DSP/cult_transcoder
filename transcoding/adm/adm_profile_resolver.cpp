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

// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
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
