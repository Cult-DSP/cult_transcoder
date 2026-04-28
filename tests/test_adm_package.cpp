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

#include "cult_transcoder.hpp"
#include "audio/wav_io.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static uint32_t counter = 0;
        path = fs::temp_directory_path() / ("cult_adm_package_" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void writeTextFile(const fs::path& path, const std::string& text) {
    std::ofstream f(path);
    REQUIRE(f.is_open());
    f << text;
}

void writeFloatWav(const fs::path& path, float value, size_t frameCount = 480) {
    std::vector<float> samples(frameCount, value);
    std::string error;
    REQUIRE(cult::writeFloat32MonoWav(path.string(), 48000, samples, error));
}

std::string readTextFile(const fs::path& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}

} // namespace

TEST_CASE("packageAdmWav creates a self-contained LUSID package from authored ADM WAV",
          "[package-adm-wav][integration]") {
    TempDir temp;

    const fs::path scenePath = temp.path / "scene.lusid.json";
    writeTextFile(scenePath, R"JSON({
  "version": "1.0",
  "timeUnit": "seconds",
  "duration": 0.01,
  "frames": [
    {
      "time": 0.0,
      "nodes": [
        {"id": "1.1", "type": "direct_speaker", "cart": [-1.0, 0.0, 0.0]},
        {"id": "2.1", "type": "direct_speaker", "cart": [1.0, 0.0, 0.0]},
        {"id": "3.1", "type": "direct_speaker", "cart": [0.0, 1.0, 0.0]},
        {"id": "4.1", "type": "LFE"},
        {"id": "11.1", "type": "audio_object", "cart": [0.0, 0.0, 0.0]}
      ]
    }
  ]
})JSON");

    writeFloatWav(temp.path / "1.1.wav", 0.1f);
    writeFloatWav(temp.path / "2.1.wav", 0.15f);
    writeFloatWav(temp.path / "3.1.wav", 0.18f);
    writeFloatWav(temp.path / "LFE.wav", 0.2f);
    writeFloatWav(temp.path / "11.1.wav", 0.3f);

    const fs::path authoredXml = temp.path / "authored.adm.xml";
    const fs::path authoredWav = temp.path / "authored.wav";

    cult::AdmAuthorRequest authorReq;
    authorReq.lusidPath = scenePath.string();
    authorReq.wavDir = temp.path.string();
    authorReq.outXmlPath = authoredXml.string();
    authorReq.outWavPath = authoredWav.string();

    auto authorResult = cult::admAuthor(authorReq);
    INFO(authorResult.report.toJson());
    REQUIRE(authorResult.success);

    const fs::path packageDir = temp.path / "package";
    std::vector<cult::ProgressEvent> events;
    cult::PackageAdmWavRequest packageReq;
    packageReq.inWavPath = authoredWav.string();
    packageReq.outPackageDir = packageDir.string();
    packageReq.onProgress = [&](const cult::ProgressEvent& event) {
        events.push_back(event);
    };

    auto packageResult = cult::packageAdmWav(packageReq);
    INFO(packageResult.report.toJson());
    REQUIRE(packageResult.success);
    REQUIRE(packageResult.report.status == "success");
    REQUIRE(packageResult.report.summary.sampleRate == 48000);
    REQUIRE(packageResult.report.summary.durationSec == 0.01);
    REQUIRE(packageResult.report.summary.countsByNodeType["direct_speaker"] == 3);
    REQUIRE(packageResult.report.summary.countsByNodeType["LFE"] == 1);
    REQUIRE(packageResult.report.summary.countsByNodeType["audio_object"] == 1);

    REQUIRE(fs::exists(packageDir / "scene.lusid.json"));
    REQUIRE(fs::exists(packageDir / "scene_report.json"));
    REQUIRE(fs::exists(packageDir / "channel_order.txt"));
    REQUIRE(fs::exists(packageDir / "1.1.wav"));
    REQUIRE(fs::exists(packageDir / "2.1.wav"));
    REQUIRE(fs::exists(packageDir / "3.1.wav"));
    REQUIRE(fs::exists(packageDir / "LFE.wav"));
    REQUIRE(fs::exists(packageDir / "5.1.wav"));

    const std::string order = readTextFile(packageDir / "channel_order.txt");
    REQUIRE(order == "1.1\n2.1\n3.1\n4.1\n5.1\n");

    cult::WavFileInfo stemInfo;
    std::string error;
    REQUIRE(cult::readWavInfo((packageDir / "5.1.wav").string(), stemInfo, error));
    REQUIRE(stemInfo.channels == 1);
    REQUIRE(stemInfo.sampleRate == 48000);
    REQUIRE(stemInfo.audioFormat == 3);
    REQUIRE(stemInfo.bitsPerSample == 32);
    REQUIRE(stemInfo.frameCount == 480);

    bool sawSplit = false;
    bool sawSplitComplete = false;
    for (const auto& event : events) {
        if (event.phase == "split") {
            sawSplit = true;
            sawSplitComplete = sawSplitComplete || (event.total > 0 && event.current == event.total);
        }
    }
    REQUIRE(sawSplit);
    REQUIRE(sawSplitComplete);
}
