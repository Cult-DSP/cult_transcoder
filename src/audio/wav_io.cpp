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
// wav_io.cpp — minimal WAV inspection helpers for authoring
// ---------------------------------------------------------------------------

#include "audio/wav_io.hpp"

#include <fstream>
#include <string>

namespace cult {

namespace {

uint32_t readU32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

uint16_t readU16(std::ifstream& f) {
    uint8_t b[2];
    f.read(reinterpret_cast<char*>(b), 2);
    return static_cast<uint16_t>(b[0]) | (static_cast<uint16_t>(b[1]) << 8);
}

bool readTag(std::ifstream& f, char out[4]) {
    f.read(out, 4);
    return f.gcount() == 4;
}

} // namespace

bool readWavInfo(const std::string& path, WavFileInfo& info, std::string& error) {
    info = WavFileInfo{};
    info.path = path;

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        error = "Failed to open WAV: '" + path + "'";
        return false;
    }

    char riff[4];
    if (!readTag(f, riff) || std::string(riff, 4) != "RIFF") {
        error = "Invalid WAV: missing RIFF header";
        return false;
    }

    (void)readU32(f); // chunk size

    char wave[4];
    if (!readTag(f, wave) || std::string(wave, 4) != "WAVE") {
        error = "Invalid WAV: missing WAVE header";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    uint32_t dataSize = 0;
    uint16_t blockAlign = 0;

    while (f && !(foundFmt && foundData)) {
        char chunkId[4];
        if (!readTag(f, chunkId)) break;
        uint32_t chunkSize = readU32(f);
        std::string id(chunkId, 4);

        if (id == "fmt ") {
            info.audioFormat = readU16(f);
            info.channels = readU16(f);
            info.sampleRate = readU32(f);
            (void)readU32(f); // byteRate
            blockAlign = readU16(f);
            info.bitsPerSample = readU16(f);

            if (chunkSize > 16) {
                f.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
            }
            foundFmt = true;
        } else if (id == "data") {
            dataSize = chunkSize;
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
            foundData = true;
        } else {
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }

        if (chunkSize % 2 == 1) {
            f.seekg(1, std::ios::cur);
        }
    }

    if (!foundFmt) {
        error = "Invalid WAV: missing fmt chunk";
        return false;
    }
    if (!foundData) {
        error = "Invalid WAV: missing data chunk";
        return false;
    }

    if (blockAlign == 0 || info.channels == 0) {
        error = "Invalid WAV: bad format data";
        return false;
    }

    info.frameCount = dataSize / blockAlign;

    if (info.audioFormat != 1 && info.audioFormat != 3) {
        error = "Unsupported WAV format (only PCM or IEEE float supported)";
        return false;
    }

    return true;
}

} // namespace cult
