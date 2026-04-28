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
// lusid_scene.hpp — LUSID scene data model
//
// Defines the in-memory representation of a LUSID Scene v1.0 document.
// Used by both the ADM ingest path (adm_to_lusid) and the LUSID JSON reader
// (lusid_reader).  No ADM-specific types here.
// ---------------------------------------------------------------------------

#include <map>
#include <string>
#include <vector>

namespace cult {

struct LusidNode {
    std::string id;             // "X.1"
    std::string type;           // "audio_object", "direct_speaker", "LFE"
    double cart[3] = {0,0,0};
    std::string speakerLabel;   // direct_speaker only
    std::string channelID;      // direct_speaker only
    bool hasCart = false;        // false for LFE nodes
};

struct LusidFrame {
    double time = 0.0;
    std::vector<LusidNode> nodes;
};

struct LusidScene {
    std::string version  = "1.0";
    std::string timeUnit = "seconds";
    double duration   = -1.0;   // <0 means not set
    int    sampleRate =  0;     // 0 means not set
    std::map<std::string, std::string> metadata;
    std::vector<LusidFrame> frames;
};

} // namespace cult
