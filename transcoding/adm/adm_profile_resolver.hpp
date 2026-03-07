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

#pragma once

// ---------------------------------------------------------------------------
// transcoding/adm/adm_profile_resolver.hpp — ADM Profile Resolver (Phase 4)
//
// Inspects an already-parsed pugi::xml_document and returns the detected
// ADM profile variant (DolbyAtmos, Sony360RA, Generic, or Unknown).
//
// Detection is a READ-ONLY attribute-string pass — no ADM semantic model.
// Does not modify the document. Called by transcoder.cpp after XML is loaded,
// before any conversion begins.
//
// Detection heuristics (priority order — AGENTS §11.3):
//   Pass 1: audioProgramme/@audioProgrammeName
//     - contains "Atmos"  (case-insensitive) → DolbyAtmos
//     - contains "360RA"  (case-insensitive) → Sony360RA
//   Pass 2: audioPackFormat/@audioPackFormatName
//     - contains "Atmos"  (case-insensitive) → DolbyAtmos
//     - contains "360RA"  (case-insensitive) → Sony360RA
//   Fallback: Unknown (caller treats as Generic)
//
// NOTE: bare "360" is intentionally NOT a detection signal — too broad.
//       Only "360RA" is used. (AGENTS §11.3)
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

// Forward-declare pugixml to avoid pulling the full header into every TU
// that includes this header. The .cpp implementation includes pugixml directly.
namespace pugi { class xml_document; }

namespace cult {

// ---------------------------------------------------------------------------
// AdmProfile — detected profile variant
// ---------------------------------------------------------------------------
enum class AdmProfile {
    Generic,     // Standard EBU BS.2076, no tool-specific signals found
    DolbyAtmos,  // "Atmos" signal detected in audioProgrammeName or audioPackFormatName
    Sony360RA,   // "360RA" signal detected in audioProgrammeName or audioPackFormatName
    Unknown,     // XML present but no profile signals found — treat as Generic
};

// ---------------------------------------------------------------------------
// ProfileResult — returned by resolveAdmProfile()
// ---------------------------------------------------------------------------
struct ProfileResult {
    AdmProfile profile = AdmProfile::Unknown;

    /// Human-readable description of which attribute triggered detection.
    /// Empty if profile == Unknown.
    /// Example: "audioProgrammeName=\"Gem_OM_360RA_3\""
    std::string detectedFrom;

    /// Informational warnings to forward into the report warnings[] block.
    /// Always populated when a profile is detected (even if it doesn't change
    /// conversion behaviour) — the report is a full transparency record.
    std::vector<std::string> warnings;
};

// ---------------------------------------------------------------------------
// resolveAdmProfile() — main entry point
//
// Inspect an already-parsed pugi::xml_document and return the detected
// profile. This is a read-only pass — does not modify the document.
//
// Called by transcoder.cpp after extractAxmlFromWav() or after
// doc.load_file(), before convertAdmToLusid[FromBuffer]() is called.
// ---------------------------------------------------------------------------
ProfileResult resolveAdmProfile(const pugi::xml_document& doc);

} // namespace cult
