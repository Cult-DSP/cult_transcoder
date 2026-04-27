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
// test_lusid_to_adm_mapping.cpp - Authoring mapping tests
// ---------------------------------------------------------------------------

#include "authoring/adm_writer.hpp"
#include "audio/wav_io.hpp"

#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        static uint32_t counter = 0;
        path = fs::temp_directory_path() / ("cult_adm_mapping_" + std::to_string(++counter));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void writeFloatWav(const fs::path& path, float value) {
    std::vector<float> samples(480, value);
    std::string error;
    REQUIRE(cult::writeFloat32MonoWav(path.string(), 48000, samples, error));
}

cult::LusidNode makeNode(const std::string& id, const std::string& type, double x, double y, double z) {
    cult::LusidNode node;
    node.id = id;
    node.type = type;
    node.cart[0] = x;
    node.cart[1] = y;
    node.cart[2] = z;
    node.hasCart = true;
    return node;
}

cult::LusidNode makeLfeNode() {
    cult::LusidNode node;
    node.id = "4.1";
    node.type = "LFE";
    node.hasCart = false;
    return node;
}

void collectChildrenByName(pugi::xml_node node,
                           const std::string& name,
                           std::vector<pugi::xml_node>& out) {
    for (auto child : node.children()) {
        if (std::string(child.name()) == name) {
            out.push_back(child);
        }
        collectChildrenByName(child, name, out);
    }
}

std::vector<pugi::xml_node> childrenByName(pugi::xml_node node, const std::string& name) {
    std::vector<pugi::xml_node> out;
    collectChildrenByName(node, name, out);
    return out;
}

} // namespace

TEST_CASE("AdmWriter maps beds before objects with deterministic IDs and motion blocks",
          "[adm-author][mapping]") {
    TempDir temp;

    writeFloatWav(temp.path / "1.1.wav", 0.1f);
    writeFloatWav(temp.path / "2.1.wav", 0.2f);
    writeFloatWav(temp.path / "LFE.wav", 0.3f);
    writeFloatWav(temp.path / "11.1.wav", 0.4f);

    std::vector<cult::WavFileInfo> wavs;
    for (const auto& name : {"1.1.wav", "2.1.wav", "LFE.wav", "11.1.wav"}) {
        cult::WavFileInfo info;
        std::string error;
        REQUIRE(cult::readWavInfo((temp.path / name).string(), info, error));
        wavs.push_back(info);
    }

    cult::LusidScene scene;
    scene.duration = 0.01;
    scene.frames.push_back(cult::LusidFrame{
        0.0,
        {
            makeNode("11.1", "audio_object", 0.0, 0.0, 0.0),
            makeNode("2.1", "direct_speaker", 1.0, 0.0, 0.0),
            makeLfeNode(),
            makeNode("1.1", "direct_speaker", -1.0, 0.0, 0.0),
        }
    });
    scene.frames.push_back(cult::LusidFrame{
        0.005,
        {
            makeNode("11.1", "audio_object", 0.25, 0.5, 0.75),
        }
    });
    scene.frames.push_back(cult::LusidFrame{
        0.011,
        {
            makeNode("11.1", "audio_object", 1.0, 1.0, 1.0),
        }
    });

    cult::AdmWriter writer;
    const fs::path outXml = temp.path / "out.adm.xml";
    const fs::path outWav = temp.path / "out.wav";
    auto result = writer.writeAdmBw64(outXml.string(), outWav.string(), scene, wavs, 48000, 480);

    REQUIRE(result.success);
    REQUIRE(result.channelCount == 4);
    REQUIRE(result.objectCount == 1);

    pugi::xml_document doc;
    REQUIRE(doc.load_file(outXml.string().c_str()));

    const auto audioObjects = childrenByName(doc, "audioObject");
    REQUIRE(audioObjects.size() == 2);
    REQUIRE(std::string(audioObjects[0].attribute("audioObjectName").value()) == "Master");
    REQUIRE(std::string(audioObjects[1].attribute("audioObjectName").value()) == "11.1");
    REQUIRE(std::string(audioObjects[0].attribute("audioObjectID").value()) == "AO_1001");
    REQUIRE(std::string(audioObjects[1].attribute("audioObjectID").value()) == "AO_100B");

    const auto bedTrackRefs = childrenByName(audioObjects[0], "audioTrackUIDRef");
    REQUIRE(bedTrackRefs.size() == 3);
    REQUIRE(std::string(bedTrackRefs[0].text().get()) == "ATU_00000001");
    REQUIRE(std::string(bedTrackRefs[1].text().get()) == "ATU_00000002");
    REQUIRE(std::string(bedTrackRefs[2].text().get()) == "ATU_00000003");

    const auto channels = childrenByName(doc, "audioChannelFormat");
    REQUIRE(channels.size() == 4);

    uint32_t directSpeakerCount = 0;
    uint32_t objectCount = 0;
    pugi::xml_node objectChannel;
    pugi::xml_node lfeChannel;
    for (auto channel : channels) {
        const std::string typeDefinition = channel.attribute("typeDefinition").value();
        const std::string name = channel.attribute("audioChannelFormatName").value();
        if (typeDefinition == "DirectSpeakers") {
            ++directSpeakerCount;
        }
        if (typeDefinition == "Objects") {
            ++objectCount;
        }
        if (name == "11.1") {
            objectChannel = channel;
        }
        if (name == "RoomCentricLFE") {
            lfeChannel = channel;
        }
    }

    REQUIRE(directSpeakerCount == 3);
    REQUIRE(objectCount == 1);
    REQUIRE(objectChannel);
    REQUIRE(lfeChannel);
    REQUIRE(std::string(objectChannel.attribute("audioChannelFormatID").value()) == "AC_00031004");
    REQUIRE(std::string(lfeChannel.attribute("audioChannelFormatID").value()) == "AC_00011004");

    const auto objectBlocks = childrenByName(objectChannel, "audioBlockFormat");
    REQUIRE(objectBlocks.size() == 2);
    REQUIRE(std::string(objectBlocks[0].attribute("rtime").value()) == "00:00:00.00000S48000");
    REQUIRE(std::string(objectBlocks[1].attribute("rtime").value()) == "00:00:00.00500S48000");
    REQUIRE(std::string(objectBlocks[0].attribute("duration").value()) == "00:00:00.00500S48000");
    REQUIRE(std::string(objectBlocks[1].attribute("duration").value()) == "00:00:00.00500S48000");

    const auto lfeLabels = childrenByName(lfeChannel, "speakerLabel");
    REQUIRE(lfeLabels.size() == 1);
    REQUIRE(std::string(lfeLabels[0].text().get()) == "RC_LFE");
}
