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

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace cult {
namespace packaging_helpers {

struct WavSourceInfo {
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;
    uint64_t frameCount = 0;
};

inline uint16_t readU16(std::ifstream& f) {
    std::array<unsigned char, 2> b{};
    f.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint16_t>(b[0] | (b[1] << 8));
}

inline uint32_t readU32(std::ifstream& f) {
    std::array<unsigned char, 4> b{};
    f.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

inline bool readTag(std::ifstream& f, char out[4]) {
    f.read(out, 4);
    return f.gcount() == 4;
}

inline bool readWavSourceInfo(const std::string& path, WavSourceInfo& info, std::string& error) {
    info = WavSourceInfo{};
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        error = "Failed to open WAV: '" + path + "'";
        return false;
    }

    char riff[4];
    if (!readTag(f, riff)) {
        error = "Invalid WAV: missing RIFF/BW64 header";
        return false;
    }
    const std::string riffId(riff, 4);
    if (riffId != "RIFF" && riffId != "BW64" && riffId != "RF64") {
        error = "Invalid WAV: missing RIFF/BW64/RF64 header";
        return false;
    }
    (void)readU32(f);
    char wave[4];
    if (!readTag(f, wave) || std::string(wave, 4) != "WAVE") {
        error = "Invalid WAV: missing WAVE header";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    while (f && !(foundFmt && foundData)) {
        char chunkId[4];
        if (!readTag(f, chunkId)) break;
        const uint32_t chunkSize = readU32(f);
        const std::string id(chunkId, 4);
        const std::streamoff payloadOffset = f.tellg();

        if (id == "fmt ") {
            info.audioFormat = readU16(f);
            info.channels = readU16(f);
            info.sampleRate = readU32(f);
            (void)readU32(f);
            info.blockAlign = readU16(f);
            info.bitsPerSample = readU16(f);
            if (chunkSize > 16) {
                f.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
            }
            foundFmt = true;
        } else if (id == "data") {
            info.dataOffset = static_cast<uint64_t>(payloadOffset);
            info.dataSize = chunkSize;
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
            foundData = true;
        } else {
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }

        if (chunkSize % 2 == 1) {
            f.seekg(1, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData) {
        error = "Invalid WAV: missing fmt or data chunk";
        return false;
    }
    if (info.channels == 0 || info.blockAlign == 0) {
        error = "Invalid WAV: bad format data";
        return false;
    }
    if (info.audioFormat != 1 && info.audioFormat != 3) {
        error = "Unsupported WAV format for package splitting";
        return false;
    }
    if (info.bitsPerSample != 16 && info.bitsPerSample != 24 && info.bitsPerSample != 32) {
        error = "Unsupported WAV bit depth for package splitting";
        return false;
    }
    info.frameCount = info.dataSize / info.blockAlign;
    return true;
}

inline std::vector<std::string> collectUniqueNodeIds(const LusidScene& scene) {
    std::vector<std::string> ids;
    std::set<std::string> seen;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (seen.insert(node.id).second) {
                ids.push_back(node.id);
            }
        }
    }
    return ids;
}

inline int bedIndex(const std::string& id) {
    if (id.size() > 2 && id.substr(id.size() - 2) == ".1") {
        try {
            const int parsed = std::stoi(id.substr(0, id.size() - 2));
            if (parsed > 0 && parsed <= 10) return parsed;
        } catch (...) {
        }
    }
    return 999999;
}

inline std::vector<std::string> canonicalNodeOrder(const LusidScene& scene) {
    auto ids = collectUniqueNodeIds(scene);
    std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
        const int bedA = bedIndex(a);
        const int bedB = bedIndex(b);
        if (bedA != bedB) return bedA < bedB;
        return a < b;
    });
    return ids;
}

inline std::string stemNameForNode(const std::string& id) {
    return id == "4.1" ? "LFE.wav" : id + ".wav";
}

inline void writeFloat32MonoHeader(std::ofstream& f, uint32_t sampleRate, uint64_t frames) {
    const uint32_t dataBytes = static_cast<uint32_t>(frames * sizeof(float));
    const uint32_t riffSize = 36u + dataBytes;
    const uint16_t audioFormat = 3;
    const uint16_t channels = 1;
    const uint16_t bits = 32;
    const uint16_t blockAlign = channels * (bits / 8);
    const uint32_t byteRate = sampleRate * blockAlign;

    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&riffSize), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    const uint32_t fmtSize = 16;
    f.write(reinterpret_cast<const char*>(&fmtSize), 4);
    f.write(reinterpret_cast<const char*>(&audioFormat), 2);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    f.write(reinterpret_cast<const char*>(&sampleRate), 4);
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    f.write(reinterpret_cast<const char*>(&bits), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataBytes), 4);
}

inline float decodeSample(const char* p, uint16_t audioFormat, uint16_t bitsPerSample) {
    if (audioFormat == 3 && bitsPerSample == 32) {
        float value = 0.0f;
        std::memcpy(&value, p, sizeof(float));
        return value;
    }
    if (audioFormat == 1 && bitsPerSample == 16) {
        int16_t value = 0;
        std::memcpy(&value, p, sizeof(int16_t));
        return static_cast<float>(value) / 32768.0f;
    }
    if (audioFormat == 1 && bitsPerSample == 24) {
        const auto b0 = static_cast<unsigned char>(p[0]);
        const auto b1 = static_cast<unsigned char>(p[1]);
        const auto b2 = static_cast<unsigned char>(p[2]);
        int32_t value = (static_cast<int32_t>(b2) << 24) |
                        (static_cast<int32_t>(b1) << 16) |
                        (static_cast<int32_t>(b0) << 8);
        value >>= 8;
        return static_cast<float>(value) / 8388608.0f;
    }
    if (audioFormat == 1 && bitsPerSample == 32) {
        int32_t value = 0;
        std::memcpy(&value, p, sizeof(int32_t));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    return 0.0f;
}

} // namespace packaging_helpers
} // namespace cult
