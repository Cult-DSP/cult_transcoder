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
// Mirrors the Python oracle (LUSID/src/xml_etree_parser.py) exactly:
//   - Iterates audioChannelFormat elements in document (encounter) order.
//   - DirectSpeakers go in the t=0 frame; channel 4 (1-based) is LFE.
//   - Objects are grouped by rtime into time-keyed frames.
//   - IDs are "{group_counter}.1" in encounter order.
//   - containsAudio is NOT used (all channels assumed active, AGENTS §4).
//   - timeUnit is always "seconds" (AGENTS §4).
//
// The output JSON is semantically identical to the Python oracle's output
// for the same input XML.  Parity is verified by deep-compare of parsed
// JSON objects (not byte-identical string comparison).
// ---------------------------------------------------------------------------

#include <string>
#include <vector>
#include <map>

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
// LUSID scene data model (mirrors LUSID/src/scene.py)
// ---------------------------------------------------------------------------

struct LusidNode {
    std::string id;        // "X.1"
    std::string type;      // "audio_object", "direct_speaker", "LFE"
    double cart[3] = {0,0,0};
    std::string speakerLabel;  // direct_speaker only
    std::string channelID;     // direct_speaker only
    bool hasCart = false;       // false for LFE nodes
};

struct LusidFrame {
    double time = 0.0;
    std::vector<LusidNode> nodes;
};

struct LusidScene {
    std::string version = "0.5";
    std::string timeUnit = "seconds";
    double duration = -1.0;     // <0 means not set
    int sampleRate = 0;         // 0 means not set
    std::map<std::string, std::string> metadata;
    std::vector<LusidFrame> frames;
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
/// This mirrors parse_adm_xml_to_lusid_scene(xml_path, contains_audio=None).
ConversionResult convertAdmToLusid(const std::string& xmlPath,
                                    LfeMode lfeMode = LfeMode::Hardcoded);

/// Parse an ADM XML string (in-memory buffer) and produce a LUSID scene.
/// Phase 3: used when the XML was extracted from a BW64 WAV in-memory by
/// extractAxmlFromWav() — avoids writing then re-reading from disk.
/// Semantically identical to convertAdmToLusid(); all parity rules apply.
ConversionResult convertAdmToLusidFromBuffer(const std::string& xmlBuffer,
                                              LfeMode lfeMode = LfeMode::Hardcoded);

/// Serialize a LusidScene to a JSON string.
/// Output matches Python's json.dump(scene.to_dict(), indent=2) format.
std::string lusidSceneToJson(const LusidScene& scene);

/// Write a LusidScene to a file.  Returns true on success.
bool writeLusidScene(const LusidScene& scene, const std::string& outPath);

} // namespace cult
