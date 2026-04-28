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

#include "parsing/lusid_reader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <string>

namespace cult {
namespace parsing_helpers {

inline bool isSupportedAudioNodeType(const std::string& type) {
    return type == "direct_speaker" || type == "audio_object" || type == "LFE";
}

inline bool isKnownNodeType(const std::string& type) {
    return isSupportedAudioNodeType(type) ||
           type == "spectral_features" ||
           type == "agent_state";
}

inline bool isValidNodeId(const std::string& id) {
    const auto dot = id.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size()) {
        return false;
    }
    if (id.find('.', dot + 1) != std::string::npos) {
        return false;
    }
    return std::all_of(id.begin(), id.begin() + static_cast<std::ptrdiff_t>(dot), [](unsigned char c) {
               return std::isdigit(c);
           }) &&
           std::all_of(id.begin() + static_cast<std::ptrdiff_t>(dot + 1), id.end(), [](unsigned char c) {
               return std::isdigit(c);
           });
}

inline std::string normalizeTimeUnit(const std::string& raw) {
    std::string value = raw;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (value == "s") return "seconds";
    if (value == "samp") return "samples";
    if (value == "ms") return "milliseconds";
    return value;
}

inline bool convertSceneTimeToSeconds(LusidScene& scene, std::string& error) {
    const std::string unit = normalizeTimeUnit(scene.timeUnit);
    double scale = 1.0;
    if (unit == "seconds") {
        scale = 1.0;
    } else if (unit == "milliseconds") {
        scale = 0.001;
    } else if (unit == "samples") {
        if (scene.sampleRate <= 0) {
            error = "LUSID scene timeUnit is 'samples' but sampleRate is missing or invalid";
            return false;
        }
        scale = 1.0 / static_cast<double>(scene.sampleRate);
    } else {
        error = "LUSID scene has unsupported timeUnit: '" + scene.timeUnit + "'";
        return false;
    }

    for (auto& frame : scene.frames) {
        frame.time *= scale;
    }
    scene.timeUnit = "seconds";
    return true;
}

inline bool readJsonFile(const std::string& path, nlohmann::json& out, std::string& error) {
    std::ifstream f(path);
    if (!f.is_open()) {
        error = "Failed to open LUSID scene: '" + path + "'";
        return false;
    }
    try {
        f >> out;
    } catch (const std::exception& e) {
        error = std::string("Failed to parse LUSID scene JSON: ") + e.what();
        return false;
    }
    return true;
}

} // namespace parsing_helpers
} // namespace cult
