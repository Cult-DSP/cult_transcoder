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
// adm_to_lusid.hpp — ADM XML → LUSID Scene JSON conversion
//
// Preserves the established ADM encounter-order mapping:
//   - Iterates audioChannelFormat elements in document (encounter) order.
//   - DirectSpeakers go in the t=0 frame; channel 4 (1-based) is LFE.
//   - Objects are grouped by rtime into time-keyed frames.
//   - IDs are "{group_counter}.1" in encounter order.
//   - containsAudio is NOT used (all channels assumed active, AGENTS §4).
//   - timeUnit is always "seconds" (AGENTS §4).
//
// The output JSON follows LUSID Scene v1.0 while preserving the original
// parity-critical ordering and node semantics for the same input XML.
// ---------------------------------------------------------------------------

#include "lusid_scene.hpp"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace pugi {
class xml_document;
}

namespace cult {

// ---------------------------------------------------------------------------
// LfeMode — controls how the LFE channel is identified (AGENTS §11.2, §11.4)
// ---------------------------------------------------------------------------
enum class LfeMode {
    /// Phase 2 default (forever): LFE = 4th DirectSpeaker encountered (1-based).
    Hardcoded,
    /// Opt-in (Phase 4+): LFE = any DirectSpeaker whose speakerLabel child
    /// element text is "LFE" or "LFE1" (case-insensitive).
    SpeakerLabel,
};

// ---------------------------------------------------------------------------
// Conversion result
// ---------------------------------------------------------------------------

struct ConversionResult {
    bool success = false;
    LusidScene scene;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    // Summary stats for the report
    int numFrames = 0;
    std::map<std::string, int> countsByNodeType;
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Parse an ADM XML file and produce a LUSID scene.
/// This preserves the original ADM XML encounter-order conversion.
ConversionResult convertAdmToLusid(const std::string& xmlPath,
                                    LfeMode lfeMode = LfeMode::Hardcoded);

/// Parse an ADM XML string (in-memory buffer) and produce a LUSID scene.
/// Phase 3: used when the XML was extracted from a BW64 WAV in-memory by
/// extractAxmlFromWav() — avoids writing then re-reading from disk.
/// Semantically identical to convertAdmToLusid(); all parity rules apply.
ConversionResult convertAdmToLusidFromBuffer(const std::string& xmlBuffer,
                                              LfeMode lfeMode = LfeMode::Hardcoded);

/// Convert an already-parsed ADM XML document.
/// Internal orchestration path used when profile detection has already loaded
/// the XML. Callers must keep the document alive for the duration of the call.
ConversionResult convertAdmDocumentToLusid(pugi::xml_document& doc,
                                            LfeMode lfeMode = LfeMode::Hardcoded);

/// Serialize a LusidScene to a JSON string.
/// Output is stable, pretty-printed LUSID Scene JSON.
/// Compatibility wrapper: delegates to writeLusidSceneJson() via an
/// std::ostringstream.
std::string lusidSceneToJson(const LusidScene& scene);

/// Stream a LusidScene as stable, pretty-printed LUSID Scene JSON.
void writeLusidSceneJson(std::ostream& out, const LusidScene& scene);

/// Write a LusidScene to a file.  Returns true on success.
/// File-writing API: delegates to writeLusidSceneJson() on the destination
/// std::ofstream.
bool writeLusidScene(const LusidScene& scene, const std::string& outPath);

} // namespace cult
