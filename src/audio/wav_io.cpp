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
#include <vector>

namespace cult {

// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
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

struct WavHeader {
    WavFileInfo info;
    uint32_t dataSize = 0;
    std::streamoff dataOffset = 0;
};

bool readWavHeader(std::ifstream& f, WavHeader& header, std::string& error) {
    header = WavHeader{};

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
    uint16_t blockAlign = 0;

    while (f && !(foundFmt && foundData)) {
        char chunkId[4];
        if (!readTag(f, chunkId)) break;
        uint32_t chunkSize = readU32(f);
        std::string id(chunkId, 4);

        if (id == "fmt ") {
            header.info.audioFormat = readU16(f);
            header.info.channels = readU16(f);
            header.info.sampleRate = readU32(f);
            (void)readU32(f); // byteRate
            blockAlign = readU16(f);
            header.info.bitsPerSample = readU16(f);

            if (chunkSize > 16) {
                f.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
            }
            foundFmt = true;
        } else if (id == "data") {
            header.dataSize = chunkSize;
            header.dataOffset = f.tellg();
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

    if (blockAlign == 0 || header.info.channels == 0) {
        error = "Invalid WAV: bad format data";
        return false;
    }

    header.info.frameCount = header.dataSize / blockAlign;

    if (header.info.audioFormat != 1 && header.info.audioFormat != 3) {
        error = "Unsupported WAV format (only PCM or IEEE float supported)";
        return false;
    }

    return true;
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

    WavHeader header;
    if (!readWavHeader(f, header, error)) {
        return false;
    }

    info = header.info;
    info.path = path;
    return true;
}

bool readWavMonoSamples(const std::string& path,
                         WavFileInfo& info,
                         std::vector<float>& samples,
                         std::string& error) {
    samples.clear();

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        error = "Failed to open WAV: '" + path + "'";
        return false;
    }

    WavHeader header;
    if (!readWavHeader(f, header, error)) {
        return false;
    }

    info = header.info;
    info.path = path;

    if (info.channels != 1) {
        error = "Unsupported WAV: only mono files are supported";
        return false;
    }

    const uint32_t bytesPerSample = info.bitsPerSample / 8;
    if (bytesPerSample == 0) {
        error = "Unsupported WAV: invalid bits-per-sample";
        return false;
    }

    samples.resize(static_cast<size_t>(info.frameCount));
    f.seekg(header.dataOffset, std::ios::beg);

    if (info.audioFormat == 3 && info.bitsPerSample == 32) {
        for (uint64_t i = 0; i < info.frameCount; ++i) {
            float v = 0.0f;
            f.read(reinterpret_cast<char*>(&v), sizeof(float));
            if (!f) {
                error = "Failed to read WAV samples";
                return false;
            }
            samples[static_cast<size_t>(i)] = v;
        }
        return true;
    }

    if (info.audioFormat == 1) {
        if (info.bitsPerSample == 16) {
            for (uint64_t i = 0; i < info.frameCount; ++i) {
                int16_t v = 0;
                f.read(reinterpret_cast<char*>(&v), sizeof(int16_t));
                if (!f) {
                    error = "Failed to read WAV samples";
                    return false;
                }
                samples[static_cast<size_t>(i)] = static_cast<float>(v) / 32768.0f;
            }
            return true;
        }
        if (info.bitsPerSample == 24) {
            for (uint64_t i = 0; i < info.frameCount; ++i) {
                uint8_t b[3] = {0, 0, 0};
                f.read(reinterpret_cast<char*>(b), 3);
                if (!f) {
                    error = "Failed to read WAV samples";
                    return false;
                }
                int32_t v = (static_cast<int32_t>(b[2]) << 24) |
                            (static_cast<int32_t>(b[1]) << 16) |
                            (static_cast<int32_t>(b[0]) << 8);
                v >>= 8;
                samples[static_cast<size_t>(i)] = static_cast<float>(v) / 8388608.0f;
            }
            return true;
        }
        if (info.bitsPerSample == 32) {
            for (uint64_t i = 0; i < info.frameCount; ++i) {
                int32_t v = 0;
                f.read(reinterpret_cast<char*>(&v), sizeof(int32_t));
                if (!f) {
                    error = "Failed to read WAV samples";
                    return false;
                }
                samples[static_cast<size_t>(i)] = static_cast<float>(v) / 2147483648.0f;
            }
            return true;
        }
    }

    error = "Unsupported WAV format (expected PCM16/24/32 or float32)";
    return false;
}

bool writeFloat32MonoWav(const std::string& path,
                         uint32_t sampleRate,
                         const std::vector<float>& samples,
                         std::string& error) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) {
        error = "Failed to write WAV: '" + path + "'";
        return false;
    }

    const uint16_t channels = 1;
    const uint16_t bitsPerSample = 32;
    const uint16_t audioFormat = 3;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(float));
    const uint32_t riffSize = 36 + dataSize;

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
    f.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataSize), 4);
    if (!samples.empty()) {
        f.write(reinterpret_cast<const char*>(samples.data()),
                static_cast<std::streamsize>(samples.size() * sizeof(float)));
    }

    if (!f.good()) {
        error = "Failed to write WAV data";
        return false;
    }
    return true;
}

} // namespace cult
