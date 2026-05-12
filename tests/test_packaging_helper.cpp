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

#include "packaging/packagingHelper.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static uint32_t counter = 0;
        path = fs::temp_directory_path() / ("cult_packaging_helper_" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void appendU16(std::vector<char>& out, uint16_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(std::vector<char>& out, uint32_t value) {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
    out.push_back(static_cast<char>((value >> 16) & 0xff));
    out.push_back(static_cast<char>((value >> 24) & 0xff));
}

void appendU64(std::vector<char>& out, uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<char>((value >> shift) & 0xff));
    }
}

void overwriteU64(std::vector<char>& out, size_t offset, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out[offset + static_cast<size_t>(i)] = static_cast<char>((value >> (i * 8)) & 0xff);
    }
}

void writeRf64Fixture(const fs::path& path) {
    std::vector<char> bytes;
    bytes.insert(bytes.end(), {'R', 'F', '6', '4'});
    appendU32(bytes, 0xffffffffu);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});

    bytes.insert(bytes.end(), {'d', 's', '6', '4'});
    appendU32(bytes, 28u);
    const size_t riffSize64Offset = bytes.size();
    appendU64(bytes, 0u);   // patched later
    appendU64(bytes, 16u);  // dataSize64: 4 stereo PCM16 frames
    appendU64(bytes, 4u);   // sampleCount64 (frame count)
    appendU32(bytes, 0u);   // table length

    bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
    appendU32(bytes, 16u);
    appendU16(bytes, 1u);       // PCM
    appendU16(bytes, 2u);       // channels
    appendU32(bytes, 48000u);   // sample rate
    appendU32(bytes, 192000u);  // byte rate
    appendU16(bytes, 4u);       // block align
    appendU16(bytes, 16u);      // bits per sample

    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendU32(bytes, 0xffffffffu);
    for (uint16_t sample = 0; sample < 8; ++sample) {
        appendU16(bytes, sample);
    }

    overwriteU64(bytes, riffSize64Offset, static_cast<uint64_t>(bytes.size() - 8));

    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.is_open());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

}  // namespace

TEST_CASE("readWavSourceInfo uses ds64 data size for RF64 package input",
          "[package-adm-wav][rf64]") {
    TempDir temp;
    const fs::path wavPath = temp.path / "rf64_fixture.wav";
    writeRf64Fixture(wavPath);

    cult::packaging_helpers::WavSourceInfo info;
    std::string error;
    REQUIRE(cult::packaging_helpers::readWavSourceInfo(wavPath.string(), info, error));
    REQUIRE(info.audioFormat == 1);
    REQUIRE(info.channels == 2);
    REQUIRE(info.sampleRate == 48000);
    REQUIRE(info.blockAlign == 4);
    REQUIRE(info.bitsPerSample == 16);
    REQUIRE(info.dataSize == 16);
    REQUIRE(info.frameCount == 4);
}
