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

#include "cult_transcoder.hpp"
#include "parsing/lusid_reader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

namespace cult {
namespace authoring_helpers {

inline std::string resolveScenePath(const AdmAuthorRequest& req) {
    if (!req.lusidPackage.empty()) {
        std::filesystem::path pkg(req.lusidPackage);
        return (pkg / "scene.lusid.json").string();
    }
    return req.lusidPath;
}

inline std::string resolveWavDir(const AdmAuthorRequest& req) {
    if (!req.lusidPackage.empty()) {
        return req.lusidPackage;
    }
    return req.wavDir;
}

inline std::vector<std::string> collectNodeIds(const LusidScene& scene) {
    std::vector<std::string> ids;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (node.type == "direct_speaker" || node.type == "audio_object" || node.type == "LFE") {
                ids.push_back(node.id);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

inline std::map<std::string, int> countUniqueSupportedNodeTypes(const LusidScene& scene) {
    std::map<std::string, std::string> typeById;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (node.type == "direct_speaker" || node.type == "audio_object" || node.type == "LFE") {
                typeById.emplace(node.id, node.type);
            }
        }
    }

    std::map<std::string, int> counts;
    for (const auto& entry : typeById) {
        counts[entry.second] += 1;
    }
    return counts;
}

inline uint32_t readU32(std::ifstream& in) {
    std::array<unsigned char, 4> b{};
    in.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

inline bool readTag(std::ifstream& in, char out[4]) {
    in.read(out, 4);
    return in.gcount() == 4;
}

inline bool extractDbmdFromRiff(const std::string& path, std::vector<char>& data, std::string& error) {
    data.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        error = "failed to open dbmd source: " + path;
        return false;
    }

    char riff[4];
    if (!readTag(in, riff)) {
        error = "dbmd source is empty";
        return false;
    }
    const std::string riffId(riff, 4);
    if (riffId != "RIFF" && riffId != "BW64" && riffId != "RF64") {
        in.clear();
        in.seekg(0, std::ios::beg);
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (data.empty()) {
            error = "raw dbmd source is empty";
            return false;
        }
        return true;
    }

    (void)readU32(in);
    char wave[4];
    if (!readTag(in, wave) || std::string(wave, 4) != "WAVE") {
        error = "dbmd source is RIFF-like but not WAVE";
        return false;
    }

    while (in) {
        char chunkId[4];
        if (!readTag(in, chunkId)) break;
        const uint32_t chunkSize = readU32(in);
        const std::string id(chunkId, 4);
        if (id == "dbmd") {
            data.resize(chunkSize);
            if (chunkSize > 0) {
                in.read(data.data(), static_cast<std::streamsize>(chunkSize));
            }
            if (!in) {
                error = "failed to read dbmd chunk from source";
                data.clear();
                return false;
            }
            return true;
        }
        in.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        if (chunkSize % 2 == 1) {
            in.seekg(1, std::ios::cur);
        }
    }

    error = "dbmd chunk not found in source: " + path;
    return false;
}

} // namespace authoring_helpers
} // namespace cult
