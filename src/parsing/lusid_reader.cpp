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
// parsing/lusid_reader.cpp — minimal LUSID Scene v1.0 reader for authoring
// ---------------------------------------------------------------------------

#include "parsing/lusid_reader.hpp"
#include "parsing/parsingHelper.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <string>

namespace cult {

// Helpers live in parsing_helpers (parsingHelper.hpp).

LusidSceneLoadResult readLusidScene(const std::string& path) {
    LusidSceneLoadResult result;
    nlohmann::json root;
    std::string error;

    if (!parsing_helpers::readJsonFile(path, root, error)) {
        result.errors.push_back(error);
        return result;
    }

    if (!root.is_object()) {
        result.errors.push_back("LUSID scene root must be an object");
        return result;
    }

    if (!root.contains("version") || !root["version"].is_string()) {
        result.errors.push_back("LUSID scene missing string 'version'");
        return result;
    }
    result.scene.version = root["version"].get<std::string>();
    if (result.scene.version != "1.0") {
        result.errors.push_back("LUSID scene version must be '1.0'");
        return result;
    }
    if (root.contains("timeUnit") && !root["timeUnit"].is_string()) {
        result.errors.push_back("LUSID scene timeUnit must be a string");
        return result;
    }
    if (root.contains("timeUnit")) {
        result.scene.timeUnit = root["timeUnit"].get<std::string>();
    }
    if (root.contains("duration") && !root["duration"].is_number()) {
        result.errors.push_back("LUSID scene duration must be numeric");
        return result;
    }
    if (root.contains("duration")) {
        result.scene.duration = root["duration"].get<double>();
        if (result.scene.duration < 0.0) {
            result.errors.push_back("LUSID scene duration must be non-negative");
            return result;
        }
    }
    if (root.contains("sampleRate") && !root["sampleRate"].is_number_integer()) {
        result.errors.push_back("LUSID scene sampleRate must be an integer");
        return result;
    }
    if (root.contains("sampleRate")) {
        result.scene.sampleRate = root["sampleRate"].get<int>();
        if (result.scene.sampleRate <= 0) {
            result.errors.push_back("LUSID scene sampleRate must be positive");
            return result;
        }
    }

    if (root.contains("metadata") && root["metadata"].is_object()) {
        for (auto it = root["metadata"].begin(); it != root["metadata"].end(); ++it) {
            if (it.value().is_string()) {
                result.scene.metadata[it.key()] = it.value().get<std::string>();
            }
        }
    }

    if (!root.contains("frames") || !root["frames"].is_array()) {
        result.errors.push_back("LUSID scene missing 'frames' array");
        return result;
    }

    for (const auto& frameJson : root["frames"]) {
        if (!frameJson.is_object()) {
            result.errors.push_back("LUSID frame is not an object");
            continue;
        }

        LusidFrame frame;
        if (!frameJson.contains("time") || !frameJson["time"].is_number()) {
            result.errors.push_back("LUSID frame missing numeric 'time'");
            continue;
        }
        frame.time = frameJson["time"].get<double>();

        if (!frameJson.contains("nodes") || !frameJson["nodes"].is_array()) {
            result.errors.push_back("LUSID frame missing 'nodes' array");
            continue;
        }

        for (const auto& nodeJson : frameJson["nodes"]) {
            if (!nodeJson.is_object()) {
                result.errors.push_back("LUSID node is not an object");
                continue;
            }
            if (!nodeJson.contains("id") || !nodeJson["id"].is_string()) {
                result.errors.push_back("LUSID node missing string 'id'");
                continue;
            }
            if (!nodeJson.contains("type") || !nodeJson["type"].is_string()) {
                result.errors.push_back("LUSID node missing string 'type'");
                continue;
            }

            LusidNode node;
            node.id = nodeJson["id"].get<std::string>();
            node.type = nodeJson["type"].get<std::string>();
            if (!parsing_helpers::isValidNodeId(node.id)) {
                result.errors.push_back("LUSID node id '" + node.id + "' must match X.Y");
                continue;
            }
            if (!parsing_helpers::isKnownNodeType(node.type)) {
                result.errors.push_back("LUSID node '" + node.id + "' has unknown type '" + node.type + "'");
                continue;
            }

            if (!parsing_helpers::isSupportedAudioNodeType(node.type)) {
                frame.nodes.push_back(node);
                continue;
            }

            if ((node.type == "audio_object" || node.type == "direct_speaker") &&
                (!nodeJson.contains("cart") || !nodeJson["cart"].is_array() || nodeJson["cart"].size() != 3)) {
                result.errors.push_back("LUSID " + node.type + " node '" + node.id + "' missing 3-value 'cart'");
                continue;
            }

            if (nodeJson.contains("cart") && nodeJson["cart"].is_array() && nodeJson["cart"].size() == 3) {
                if (!nodeJson["cart"][0].is_number() ||
                    !nodeJson["cart"][1].is_number() ||
                    !nodeJson["cart"][2].is_number()) {
                    result.errors.push_back("LUSID node '" + node.id + "' has non-numeric 'cart'");
                    continue;
                }
                node.cart[0] = nodeJson["cart"][0].get<double>();
                node.cart[1] = nodeJson["cart"][1].get<double>();
                node.cart[2] = nodeJson["cart"][2].get<double>();
                if (!std::isfinite(node.cart[0]) ||
                    !std::isfinite(node.cart[1]) ||
                    !std::isfinite(node.cart[2])) {
                    result.errors.push_back("LUSID node '" + node.id + "' has non-finite 'cart'");
                    continue;
                }
                node.hasCart = true;
            }
            if (node.type == "LFE" && node.hasCart) {
                result.errors.push_back("LUSID LFE node '" + node.id + "' must not include 'cart'");
                continue;
            }
            if (nodeJson.contains("speakerLabel") && nodeJson["speakerLabel"].is_string()) {
                node.speakerLabel = nodeJson["speakerLabel"].get<std::string>();
            }
            if (nodeJson.contains("channelID") && nodeJson["channelID"].is_string()) {
                node.channelID = nodeJson["channelID"].get<std::string>();
            }

            frame.nodes.push_back(node);
        }

        result.scene.frames.push_back(frame);
    }

    std::sort(result.scene.frames.begin(), result.scene.frames.end(), [](const LusidFrame& a, const LusidFrame& b) {
        return a.time < b.time;
    });

    std::string timeError;
    if (!parsing_helpers::convertSceneTimeToSeconds(result.scene, timeError)) {
        result.errors.push_back(timeError);
        return result;
    }

    if (result.errors.empty()) {
        result.success = true;
    }

    return result;
}

} // namespace cult
