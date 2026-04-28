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
// test_adm_author_integration.cpp - End-to-end adm-author tests
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "audio/wav_io.hpp"

#include <bw64/bw64.hpp>
#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

#include <cstdint>
#include <array>
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
        path = fs::temp_directory_path() / ("cult_adm_author_integration_" + std::to_string(++counter));
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

uint32_t readU32(std::ifstream& in) {
    std::array<unsigned char, 4> b{};
    in.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

bool outputHasChunk(const fs::path& path, const std::string& wantedId) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    char tag[4];
    in.read(tag, 4);
    if (std::string(tag, 4) != "RIFF" && std::string(tag, 4) != "BW64" && std::string(tag, 4) != "RF64") {
        return false;
    }
    (void)readU32(in);
    in.read(tag, 4);
    if (std::string(tag, 4) != "WAVE") {
        return false;
    }

    while (in) {
        in.read(tag, 4);
        if (in.gcount() != 4) break;
        const uint32_t size = readU32(in);
        if (std::string(tag, 4) == wantedId) {
            return true;
        }
        in.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        if (size % 2 == 1) {
            in.seekg(1, std::ios::cur);
        }
    }
    return false;
}

std::vector<std::string> chunkOrder(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    char tag[4];
    in.read(tag, 4);
    REQUIRE((std::string(tag, 4) == "RIFF" || std::string(tag, 4) == "BW64" || std::string(tag, 4) == "RF64"));
    (void)readU32(in);
    in.read(tag, 4);
    REQUIRE(std::string(tag, 4) == "WAVE");

    std::vector<std::string> ids;
    while (in) {
        in.read(tag, 4);
        if (in.gcount() != 4) break;
        const uint32_t size = readU32(in);
        ids.emplace_back(tag, 4);
        in.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        if (size % 2 == 1) {
            in.seekg(1, std::ios::cur);
        }
    }
    return ids;
}

} // namespace

TEST_CASE("admAuthor writes ADM XML and BW64 with matching embedded axml",
          "[adm-author][integration]") {
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
        {"id": "4.1", "type": "LFE"},
        {"id": "11.1", "type": "audio_object", "cart": [0.0, 0.0, 0.0]}
      ]
    },
    {
      "time": 0.005,
      "nodes": [
        {"id": "11.1", "type": "audio_object", "cart": [0.5, 0.0, 0.25]}
      ]
    }
  ]
})JSON");

    writeFloatWav(temp.path / "1.1.wav", 0.1f);
    writeFloatWav(temp.path / "LFE.wav", 0.2f);
    writeFloatWav(temp.path / "11.1.wav", 0.3f);

    const fs::path outXml = temp.path / "export.adm.xml";
    const fs::path outWav = temp.path / "export.wav";
    const fs::path report = temp.path / "export.report.json";

    cult::AdmAuthorRequest req;
    req.lusidPath = scenePath.string();
    req.wavDir = temp.path.string();
    req.outXmlPath = outXml.string();
    req.outWavPath = outWav.string();
    req.reportPath = report.string();

    auto result = cult::admAuthor(req);
    INFO(result.report.toJson());
    REQUIRE(result.success);
    REQUIRE(result.report.status == "pass");
    REQUIRE(result.report.summary.durationSec == 0.01);
    REQUIRE(result.report.summary.sampleRate == 48000);
    REQUIRE(result.report.summary.timeUnit == "seconds");
    REQUIRE(result.report.summary.numFrames == 2);
    REQUIRE(result.report.summary.countsByNodeType["direct_speaker"] == 1);
    REQUIRE(result.report.summary.countsByNodeType["LFE"] == 1);
    REQUIRE(result.report.summary.countsByNodeType["audio_object"] == 1);
    REQUIRE(result.report.hasAuthoringValidation);
    REQUIRE(result.report.authoringValidation.expectedFrames == 480);
    REQUIRE(result.report.authoringValidation.files.size() == 3);
    for (const auto& f : result.report.authoringValidation.files) {
        REQUIRE(f.framesUsed == 480);
        REQUIRE_FALSE(f.truncatedToExpected);
        REQUIRE(f.ok);
    }

    REQUIRE(fs::exists(outXml));
    REQUIRE(fs::exists(outWav));

    const std::string xml = readTextFile(outXml);
    REQUIRE(xml.find("<audioFormatExtended") != std::string::npos);
    REQUIRE(xml.find("start=\"00:00:00.00000\"") != std::string::npos);
    REQUIRE(xml.find("end=\"00:00:00.01000\"") != std::string::npos);
    REQUIRE(xml.find("duration=\"00:00:00.01000\"") != std::string::npos);
    REQUIRE(xml.find("audioObjectName=\"11.1\"") != std::string::npos);
    REQUIRE(xml.find("audioObjectName=\"Master\"") != std::string::npos);
    REQUIRE(xml.find("audioChannelFormatName=\"RoomCentricLFE\"") != std::string::npos);
    REQUIRE(xml.find("S48000") == std::string::npos);
    REQUIRE(xml.find("<dialogue mixedContentKind=\"0\">2</dialogue>") != std::string::npos);
    REQUIRE(xml.find("<jumpPosition interpolationLength=\"0\">1</jumpPosition>") != std::string::npos);

    pugi::xml_document doc;
    REQUIRE(doc.load_file(outXml.string().c_str()));

    auto bw64File = bw64::readFile(outWav.string());
    REQUIRE(bw64File->formatChunk());
    REQUIRE(bw64File->formatChunk()->channelCount() == 3);
    REQUIRE(bw64File->formatChunk()->sampleRate() == 48000);
    REQUIRE(bw64File->formatChunk()->bitsPerSample() == 24);
    REQUIRE(bw64File->axmlChunk());
    REQUIRE(bw64File->axmlChunk()->data() == xml);
    REQUIRE(bw64File->chnaChunk());
    REQUIRE(bw64File->chnaChunk()->numTracks() == 3);
    REQUIRE(bw64File->chnaChunk()->numUids() == 3);

    const auto audioIds = bw64File->chnaChunk()->audioIds();
    REQUIRE(audioIds.size() == 3);
    REQUIRE(audioIds[0].trackIndex() == 1);
    REQUIRE(audioIds[0].uid() == "ATU_00000001");
    REQUIRE(audioIds[0].trackRef() == "AT_00011001_01");
    REQUIRE(audioIds[0].packRef() == "AP_00011001");
    REQUIRE(audioIds[1].trackIndex() == 2);
    REQUIRE(audioIds[1].uid() == "ATU_00000002");
    REQUIRE(audioIds[1].trackRef() == "AT_00011004_01");
    REQUIRE(audioIds[1].packRef() == "AP_00011001");
    REQUIRE(audioIds[2].trackIndex() == 3);
    REQUIRE(audioIds[2].uid() == "ATU_00000003");
    REQUIRE(audioIds[2].trackRef() == "AT_00031003_01");
    REQUIRE(audioIds[2].packRef() == "AP_00031002");
}

TEST_CASE("admAuthor accepts LUSID v1 sample timestamps by converting them to seconds",
          "[adm-author][integration]") {
    TempDir temp;

    const fs::path scenePath = temp.path / "scene.lusid.json";
    writeTextFile(scenePath, R"JSON({
  "version": "1.0",
  "timeUnit": "samples",
  "sampleRate": 48000,
  "duration": 0.01,
  "frames": [
    {
      "time": 0,
      "nodes": [
        {"id": "11.1", "type": "audio_object", "cart": [0.0, 0.0, 0.0]},
        {"id": "11.2", "type": "spectral_features", "centroid": 5000.0}
      ]
    },
    {
      "time": 240,
      "nodes": [
        {"id": "11.1", "type": "audio_object", "cart": [0.5, 0.0, 0.25]}
      ]
    }
  ]
})JSON");

    writeFloatWav(temp.path / "11.1.wav", 0.3f);

    const fs::path outXml = temp.path / "export.adm.xml";
    const fs::path outWav = temp.path / "export.wav";

    cult::AdmAuthorRequest req;
    req.lusidPath = scenePath.string();
    req.wavDir = temp.path.string();
    req.outXmlPath = outXml.string();
    req.outWavPath = outWav.string();

    auto result = cult::admAuthor(req);
    INFO(result.report.toJson());
    REQUIRE(result.success);
    REQUIRE(result.report.summary.timeUnit == "seconds");
    REQUIRE(result.report.summary.numFrames == 2);
    REQUIRE(result.report.summary.countsByNodeType["audio_object"] == 1);
    REQUIRE(result.report.summary.countsByNodeType.count("spectral_features") == 0);

    const std::string xml = readTextFile(outXml);
    REQUIRE(xml.find("rtime=\"00:00:00.00500\"") != std::string::npos);
    REQUIRE(xml.find("duration=\"00:00:00.00500\"") != std::string::npos);
}

TEST_CASE("admAuthor can copy an experimental dbmd chunk into authored output",
          "[adm-author][integration]") {
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
        {"id": "1.1", "type": "direct_speaker", "cart": [-1.0, 0.0, 0.0]}
      ]
    }
  ]
})JSON");

    writeFloatWav(temp.path / "1.1.wav", 0.1f);
    const fs::path dbmd = temp.path / "source.dbmd.bin";
    writeTextFile(dbmd, "CULT_DBMD_TEST");

    const fs::path outXml = temp.path / "export.adm.xml";
    const fs::path outWav = temp.path / "export.wav";

    cult::AdmAuthorRequest req;
    req.lusidPath = scenePath.string();
    req.wavDir = temp.path.string();
    req.outXmlPath = outXml.string();
    req.outWavPath = outWav.string();
    req.dbmdSourcePath = dbmd.string();
    req.metadataPostData = true;

    auto result = cult::admAuthor(req);
    INFO(result.report.toJson());
    REQUIRE(result.success);
    REQUIRE(outputHasChunk(outWav, "dbmd"));
    REQUIRE(result.report.warnings.size() == 2);
    REQUIRE(chunkOrder(outWav) == std::vector<std::string>{"JUNK", "fmt ", "data", "axml", "chna", "dbmd"});
}

TEST_CASE("admAuthor tolerates one trailing sample mismatch by truncating to shortest length",
          "[adm-author][integration]") {
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
        {"id": "11.1", "type": "audio_object", "cart": [0.0, 0.0, 0.0]}
      ]
    }
  ]
})JSON");

    writeFloatWav(temp.path / "1.1.wav", 0.1f, 480);
    writeFloatWav(temp.path / "11.1.wav", 0.3f, 481);

    const fs::path outXml = temp.path / "export.adm.xml";
    const fs::path outWav = temp.path / "export.wav";

    cult::AdmAuthorRequest req;
    req.lusidPath = scenePath.string();
    req.wavDir = temp.path.string();
    req.outXmlPath = outXml.string();
    req.outWavPath = outWav.string();

    auto result = cult::admAuthor(req);
    INFO(result.report.toJson());
    REQUIRE(result.success);
    REQUIRE(result.report.status == "pass");
    REQUIRE(result.report.authoringValidation.expectedFrames == 480);
    REQUIRE(result.report.warnings.size() == 1);
    REQUIRE(result.report.lossLedger.size() == 1);
    REQUIRE(result.report.lossLedger.front().kind == "truncated");
    REQUIRE(result.report.lossLedger.front().count == 1);

    bool sawTruncated = false;
    for (const auto& f : result.report.authoringValidation.files) {
        REQUIRE(f.ok);
        REQUIRE(f.framesUsed == 480);
        if (f.frames == 481) {
            sawTruncated = true;
            REQUIRE(f.truncatedToExpected);
        }
    }
    REQUIRE(sawTruncated);

    auto bw64File = bw64::readFile(outWav.string());
    REQUIRE(bw64File->formatChunk());
    REQUIRE(bw64File->formatChunk()->channelCount() == 2);
    REQUIRE(bw64File->chnaChunk());
    REQUIRE(bw64File->chnaChunk()->numTracks() == 2);
    REQUIRE(bw64File->chnaChunk()->numUids() == 2);
}
