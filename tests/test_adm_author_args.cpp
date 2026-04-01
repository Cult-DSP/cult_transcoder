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
// test_adm_author_args.cpp — Unit tests for admAuthor() validation
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"

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
        path = fs::temp_directory_path() / ("cult_adm_author_" + std::to_string(
            std::hash<std::string>{}(__FILE__)));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void writeTextFile(const fs::path& path, const std::string& text) {
    std::ofstream f(path);
    f << text;
}

void writeWavFile(const fs::path& path, uint32_t sampleRate, uint16_t channels,
                  uint16_t bitsPerSample, uint32_t frames) {
    const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
    const uint32_t dataSize = frames * blockAlign;
    const uint32_t byteRate = sampleRate * blockAlign;

    std::ofstream f(path, std::ios::binary);
    f.write("RIFF", 4);
    const uint32_t riffSize = 36 + dataSize;
    f.write(reinterpret_cast<const char*>(&riffSize), 4);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    const uint32_t fmtSize = 16;
    f.write(reinterpret_cast<const char*>(&fmtSize), 4);
    const uint16_t audioFormat = 1;
    f.write(reinterpret_cast<const char*>(&audioFormat), 2);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    f.write(reinterpret_cast<const char*>(&sampleRate), 4);
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    f.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataSize), 4);

    std::vector<char> zeros(dataSize, 0);
    f.write(zeros.data(), zeros.size());
}

bool containsError(const cult::Report& report, const std::string& needle) {
    for (const auto& e : report.errors) {
        if (e.find(needle) != std::string::npos) return true;
    }
    return false;
}

} // namespace

TEST_CASE("admAuthor: rejects mixed package and explicit inputs", "[adm-author][validation]") {
    cult::AdmAuthorRequest req;
    req.lusidPackage = "/tmp/pkg";
    req.lusidPath = "/tmp/scene.lusid.json";
    req.wavDir = "/tmp/wavs";
    req.outXmlPath = "/tmp/out.adm.xml";
    req.outWavPath = "/tmp/out.adm.wav";
    req.reportPath = "/tmp/out.report.json";

    auto result = cult::admAuthor(req);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "mutually exclusive"));
}

TEST_CASE("admAuthor: missing out paths returns fail", "[adm-author][validation]") {
    cult::AdmAuthorRequest req;
    req.lusidPath = "/tmp/scene.lusid.json";
    req.wavDir = "/tmp/wavs";

    auto result = cult::admAuthor(req);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "--out-xml"));
    REQUIRE(containsError(result.report, "--out-wav"));
}

TEST_CASE("admAuthor: validates required scene + wav files", "[adm-author][validation]") {
    TempDir temp;
    const fs::path scenePath = temp.path / "scene.lusid.json";
    const fs::path wavDir = temp.path;

    writeTextFile(scenePath, R"JSON({
  "version": "0.5",
  "timeUnit": "seconds",
  "frames": [
    {
      "time": 0.0,
      "nodes": [
        {"id": "1.1", "type": "direct_speaker", "cart": [0.0, 0.0, 0.0]},
        {"id": "11.1", "type": "audio_object", "cart": [0.0, 0.0, 0.0]}
      ]
    }
  ]
})JSON");

    writeWavFile(wavDir / "1.1.wav", 48000, 1, 16, 480);
    writeWavFile(wavDir / "11.1.wav", 48000, 1, 16, 480);

    cult::AdmAuthorRequest req;
    req.lusidPath = scenePath.string();
    req.wavDir = wavDir.string();
    req.outXmlPath = (temp.path / "out.adm.xml").string();
    req.outWavPath = (temp.path / "out.adm.wav").string();
    req.reportPath = (temp.path / "out.report.json").string();

    auto result = cult::admAuthor(req);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "not implemented"));
}
