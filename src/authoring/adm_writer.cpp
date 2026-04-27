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
// adm_writer.cpp - Mapping implementation for ADM authoring
// ---------------------------------------------------------------------------

#include "adm_writer.hpp"
#include <pugixml.hpp>
#include <bw64/bw64.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

namespace cult {

namespace {

std::string to_string_fixed(double val, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << val;
    return out.str();
}

struct NodeTimeline {
    std::string id;
    std::string type;
    std::vector<const LusidNode*> nodes;
    std::vector<double> times;
};

struct AdmIds {
    std::string audioObjectId;
    std::string packId;
    std::string channelId;
    std::string streamId;
    std::string trackFormatId;
    std::string trackUid;
    std::string elementValue;
    std::string typeDescriptor;
};

std::string decimalPadded(size_t value, size_t width) {
    std::ostringstream out;
    out << std::setfill('0') << std::setw(static_cast<int>(width)) << value;
    return out.str();
}

std::string hexPadded(uint32_t value, size_t width) {
    std::ostringstream out;
    out << std::nouppercase << std::hex << std::setfill('0')
        << std::setw(static_cast<int>(width)) << value;
    return out.str();
}

std::string admTypeDefinition(const std::string& type) {
    return type == "audio_object" ? "Objects" : "DirectSpeakers";
}

std::string admTypeLabel(const std::string& type) {
    return type == "audio_object" ? "0003" : "0001";
}

uint32_t directSpeakerElementValue(const std::string& id, size_t sortedIndex) {
    const auto dot = id.find('.');
    if (dot != std::string::npos) {
        try {
            const int parsed = std::stoi(id.substr(0, dot));
            if (parsed > 0) {
                return static_cast<uint32_t>(parsed);
            }
        } catch (...) {
        }
    }
    return static_cast<uint32_t>(sortedIndex + 1);
}

AdmIds makeAdmIds(size_t sortedIndex, size_t objectIndex, const std::string& id, const std::string& type) {
    const bool isObject = type == "audio_object";
    const std::string typeDescriptor = admTypeLabel(type);
    const uint32_t elementValue = isObject
        ? static_cast<uint32_t>(0x1001 + objectIndex)
        : directSpeakerElementValue(id, sortedIndex);
    const std::string value = hexPadded(elementValue, 4);

    AdmIds ids;
    ids.typeDescriptor = typeDescriptor;
    ids.elementValue = value;
    ids.audioObjectId = "AO_" + hexPadded(static_cast<uint32_t>(0x1001 + sortedIndex), 4);
    ids.packId = "AP_" + typeDescriptor + value;
    ids.channelId = "AC_" + typeDescriptor + value;
    ids.streamId = "AS_" + typeDescriptor + value;
    ids.trackFormatId = "AT_" + typeDescriptor + value + "_01";
    ids.trackUid = "ATU_" + decimalPadded(sortedIndex + 1, 8);
    return ids;
}

void appendText(pugi::xml_node parent, const char* name, const std::string& value) {
    parent.append_child(name).text().set(value.c_str());
}

void appendCoordinate(pugi::xml_node parent, const char* coordinate, double value) {
    auto node = parent.append_child("position");
    node.append_attribute("coordinate") = coordinate;
    node.text().set(to_string_fixed(value, 6).c_str());
}

std::string saveXmlToString(const pugi::xml_document& doc) {
    std::ostringstream out;
    doc.save(out, "  ", pugi::format_default, pugi::encoding_utf8);
    return out.str();
}

} // namespace

std::string AdmWriter::formatAdmTime(double timeSeconds, uint32_t sampleRate) const {
    // Basic format: HH:MM:SS.fffffS<sampleRate>
    uint64_t totalSeconds = static_cast<uint64_t>(timeSeconds);
    uint32_t hours = totalSeconds / 3600;
    uint32_t minutes = (totalSeconds % 3600) / 60;
    uint32_t seconds = totalSeconds % 60;

    double frac = timeSeconds - static_cast<double>(totalSeconds);
    uint32_t fracFrames = static_cast<uint32_t>(std::round(frac * 100000.0));

    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "."
        << std::setw(5) << fracFrames << "S" << sampleRate;

    return out.str();
}

std::vector<std::string> AdmWriter::sortNodeIds(const std::vector<std::string>& ids) const {
    std::vector<std::string> sorted = ids;
    std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
        // Beds vs Objects: if an ID ends in ".1" and parses cleanly up to 10, it's a bed.
        // Actually, deterministic sort logic from AGENTS §3:
        // "beds (1.1,2.1,3.1,4.1,5.1..10.1), then objects by id."

        auto parseBedIndex = [](const std::string& s) -> int {
            if (s.size() > 2 && s.substr(s.size() - 2) == ".1") {
                try {
                    int val = std::stoi(s.substr(0, s.size() - 2));
                    if (val > 0 && val <= 10) return val;
                } catch(...) {}
            }
            return 999999;
        };

        int bedA = parseBedIndex(a);
        int bedB = parseBedIndex(b);

        if (bedA != bedB) {
            return bedA < bedB;
        }

        return a < b;
    });
    return sorted;
}

std::string AdmWriter::generateAdmXml(const LusidScene& scene, const std::vector<WavFileInfo>& monoWavs, uint32_t targetSampleRate, uint64_t expectedFrames, uint32_t& outChannelCount, uint32_t& outObjectCount) {
    (void)monoWavs;

    std::vector<std::string> ids;
    std::map<std::string, NodeTimeline> timelines;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (node.type != "direct_speaker" && node.type != "audio_object" && node.type != "LFE") {
                continue;
            }
            auto& timeline = timelines[node.id];
            if (timeline.id.empty()) {
                timeline.id = node.id;
                timeline.type = node.type;
                ids.push_back(node.id);
            }
            timeline.nodes.push_back(&node);
            timeline.times.push_back(frame.time);
        }
    }

    for (auto& entry : timelines) {
        auto& timeline = entry.second;
        std::vector<size_t> order(timeline.nodes.size());
        for (size_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return timeline.times[a] < timeline.times[b];
        });

        std::vector<const LusidNode*> sortedNodes;
        std::vector<double> sortedTimes;
        sortedNodes.reserve(order.size());
        sortedTimes.reserve(order.size());
        for (const auto index : order) {
            sortedNodes.push_back(timeline.nodes[index]);
            sortedTimes.push_back(timeline.times[index]);
        }
        timeline.nodes = sortedNodes;
        timeline.times = sortedTimes;
    }

    const auto sortedIds = sortNodeIds(ids);
    outChannelCount = static_cast<uint32_t>(sortedIds.size());
    outObjectCount = 0;

    const double totalDuration = static_cast<double>(expectedFrames) / static_cast<double>(targetSampleRate);

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto root = doc.append_child("ebuCoreMain");
    root.append_attribute("xmlns") = "urn:ebu:metadata-schema:ebuCore_2014";
    root.append_attribute("xmlns:dc") = "http://purl.org/dc/elements/1.1/";
    root.append_attribute("version") = "1.10";

    auto coreMetadata = root.append_child("coreMetadata");
    auto format = coreMetadata.append_child("format");
    auto admFormat = format.append_child("audioFormatExtended");
    admFormat.append_attribute("version") = "ITU-R_BS.2076-2";

    // Create base Programme and Content
    auto programme = admFormat.append_child("audioProgramme");
    programme.append_attribute("audioProgrammeID") = "APR_1001";
    programme.append_attribute("audioProgrammeName") = "CULT Authoring Programme";
    programme.append_attribute("start") = formatAdmTime(0.0, targetSampleRate).c_str();
    programme.append_attribute("duration") = formatAdmTime(totalDuration, targetSampleRate).c_str();

    auto content = admFormat.append_child("audioContent");
    content.append_attribute("audioContentID") = "ACO_1001";
    content.append_attribute("audioContentName") = "CULT Authoring Content";
    programme.append_child("audioContentIDRef").text().set("ACO_1001");

    for (size_t i = 0; i < sortedIds.size(); ++i) {
        const std::string& id = sortedIds[i];
        const auto timelineIt = timelines.find(id);
        if (timelineIt == timelines.end()) {
            continue;
        }

        const auto& timeline = timelineIt->second;
        const bool isObject = timeline.type == "audio_object";
        const size_t objectIndex = outObjectCount;
        if (isObject) {
            ++outObjectCount;
        }

        const AdmIds ids = makeAdmIds(i, objectIndex, id, timeline.type);
        const std::string typeDefinition = admTypeDefinition(timeline.type);
        const std::string typeLabel = admTypeLabel(timeline.type);

        auto audioObject = admFormat.append_child("audioObject");
        audioObject.append_attribute("audioObjectID") = ids.audioObjectId.c_str();
        audioObject.append_attribute("audioObjectName") = id.c_str();
        audioObject.append_attribute("start") = formatAdmTime(0.0, targetSampleRate).c_str();
        audioObject.append_attribute("duration") = formatAdmTime(totalDuration, targetSampleRate).c_str();
        appendText(audioObject, "audioPackFormatIDRef", ids.packId);
        appendText(audioObject, "audioTrackUIDRef", ids.trackUid);
        content.append_child("audioObjectIDRef").text().set(ids.audioObjectId.c_str());

        auto pack = admFormat.append_child("audioPackFormat");
        pack.append_attribute("audioPackFormatID") = ids.packId.c_str();
        pack.append_attribute("audioPackFormatName") = id.c_str();
        pack.append_attribute("typeLabel") = typeLabel.c_str();
        pack.append_attribute("typeDefinition") = typeDefinition.c_str();
        appendText(pack, "audioChannelFormatIDRef", ids.channelId);

        auto channel = admFormat.append_child("audioChannelFormat");
        channel.append_attribute("audioChannelFormatID") = ids.channelId.c_str();
        channel.append_attribute("audioChannelFormatName") = id.c_str();
        channel.append_attribute("typeLabel") = typeLabel.c_str();
        channel.append_attribute("typeDefinition") = typeDefinition.c_str();

        for (size_t b = 0; b < timeline.nodes.size(); ++b) {
            const auto* node = timeline.nodes[b];
            const double start = timeline.times[b];
            if (start >= totalDuration) {
                continue;
            }
            const double nextRaw = (b + 1 < timeline.times.size()) ? timeline.times[b + 1] : totalDuration;
            const double next = std::min(nextRaw, totalDuration);
            const double duration = std::max(0.0, next - start);
            if (duration <= 0.0) {
                continue;
            }

            auto block = channel.append_child("audioBlockFormat");
            block.append_attribute("audioBlockFormatID") =
                ("AB_" + ids.typeDescriptor + ids.elementValue + "_" + decimalPadded(b + 1, 8)).c_str();
            block.append_attribute("rtime") = formatAdmTime(start, targetSampleRate).c_str();
            block.append_attribute("duration") = formatAdmTime(duration, targetSampleRate).c_str();
            if (isObject) {
                block.append_attribute("cartesian") = "1";
            }

            if (node->hasCart) {
                appendCoordinate(block, "X", node->cart[0]);
                appendCoordinate(block, "Y", node->cart[1]);
                appendCoordinate(block, "Z", node->cart[2]);
            } else if (timeline.type == "LFE") {
                appendText(block, "speakerLabel", "LFE");
            }
        }

        auto stream = admFormat.append_child("audioStreamFormat");
        stream.append_attribute("audioStreamFormatID") = ids.streamId.c_str();
        stream.append_attribute("audioStreamFormatName") = id.c_str();
        stream.append_attribute("formatLabel") = "0001";
        stream.append_attribute("formatDefinition") = "PCM";
        appendText(stream, "audioChannelFormatIDRef", ids.channelId);
        appendText(stream, "audioPackFormatIDRef", ids.packId);
        appendText(stream, "audioTrackFormatIDRef", ids.trackFormatId);

        auto trackFormat = admFormat.append_child("audioTrackFormat");
        trackFormat.append_attribute("audioTrackFormatID") = ids.trackFormatId.c_str();
        trackFormat.append_attribute("audioTrackFormatName") = id.c_str();
        trackFormat.append_attribute("formatLabel") = "0001";
        trackFormat.append_attribute("formatDefinition") = "PCM";
        appendText(trackFormat, "audioStreamFormatIDRef", ids.streamId);

        auto trackUidNode = admFormat.append_child("audioTrackUID");
        trackUidNode.append_attribute("UID") = ids.trackUid.c_str();
        appendText(trackUidNode, "audioTrackFormatIDRef", ids.trackFormatId);
        appendText(trackUidNode, "audioPackFormatIDRef", ids.packId);
    }

    return saveXmlToString(doc);
}

AdmWriterResult AdmWriter::writeAdmBw64(
        const std::string& outXmlPath,
        const std::string& outWavPath,
        const LusidScene& scene,
        const std::vector<WavFileInfo>& monoWavs,
        uint32_t targetSampleRate,
        uint64_t expectedFrames
) {
    AdmWriterResult result;

    // Sort WAVs/Nodes based on their ADM canonical mapping requirements
    std::vector<std::string> ids;
    std::map<std::string, std::string> typeById;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (node.type != "direct_speaker" && node.type != "audio_object" && node.type != "LFE") {
                continue;
            }
            ids.push_back(node.id);
            typeById.emplace(node.id, node.type);
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    std::vector<std::string> sortedIds = sortNodeIds(ids);

    uint32_t channelCount = 0;
    uint32_t objectCount = 0;

    // 1. Generate XML
    std::string xmlString = generateAdmXml(scene, monoWavs, targetSampleRate, expectedFrames, channelCount, objectCount);

    // Write XML temp file
    std::string tmpXmlPath = outXmlPath + ".tmp";
    std::ofstream xmlOut(tmpXmlPath);
    if (!xmlOut) {
        result.errorMessage = "Failed to create temp XML file";
        return result;
    }
    xmlOut << xmlString;
    xmlOut.close();

    // 2. Prepare BW64 Writer
    uint16_t bw64Channels = static_cast<uint16_t>(sortedIds.size());
    if (bw64Channels == 0) {
        result.errorMessage = "No channels to write";
        return result;
    }

    auto chnaChunk = std::make_shared<bw64::ChnaChunk>();
    size_t chnaObjectIndex = 0;
    for (size_t c = 0; c < sortedIds.size(); ++c) {
        const auto typeIt = typeById.find(sortedIds[c]);
        if (typeIt == typeById.end()) {
            result.errorMessage = "Missing node type for ADM channel: " + sortedIds[c];
            return result;
        }
        const size_t objectIndex = typeIt->second == "audio_object" ? chnaObjectIndex++ : 0;
        const AdmIds idsForChannel = makeAdmIds(c, objectIndex, sortedIds[c], typeIt->second);
        chnaChunk->addAudioId(bw64::AudioId(
            static_cast<uint16_t>(c + 1),
            idsForChannel.trackUid,
            idsForChannel.trackFormatId,
            idsForChannel.packId));
    }

    std::vector<std::shared_ptr<bw64::Chunk>> extraChunks;
    extraChunks.push_back(chnaChunk);
    auto axmlChunk = std::make_shared<bw64::AxmlChunk>(xmlString);
    extraChunks.push_back(axmlChunk);

    std::string tmpWavPath = outWavPath + ".tmp";

    try {
        bw64::Bw64Writer bw64Writer(tmpWavPath.c_str(), bw64Channels, targetSampleRate, 24, extraChunks);

        // 3. Open mono WAV streams
        std::vector<std::ifstream> streams(bw64Channels);
        for (size_t c = 0; c < bw64Channels; ++c) {
            const std::string& neededId = sortedIds[c];
            std::string monoPath;
            // find path
            for (const auto& info : monoWavs) {
                // heuristic: info.path stem == neededId or neededId_48k
                std::string stem = info.path.substr(info.path.find_last_of("/\\") + 1);
                if (stem == neededId + ".wav" || stem == neededId + "_48k.wav") {
                    monoPath = info.path;
                    break;
                }
            }
            if (neededId == "4.1" && monoPath.empty()) {
                // check LFE.wav
                for (const auto& info : monoWavs) {
                    std::string stem = info.path.substr(info.path.find_last_of("/\\") + 1);
                    if (stem == "LFE.wav" || stem == "LFE_48k.wav") {
                        monoPath = info.path;
                        break;
                    }
                }
            }

            streams[c].open(monoPath, std::ios::binary);
            if (!streams[c].is_open()) {
                result.errorMessage = "Failed to open mono WAV for interlacing: " + neededId;
                return result;
            }
            // skip 44 byte header (assuming standard float32 mono normalized we generated)
            streams[c].seekg(44, std::ios::beg);
        }

        // 4. Chunk Read and Interlace
        const uint64_t framesPerChunk = 4096;
        std::vector<float> monoChunk(framesPerChunk);
        std::vector<float> interleaveBuf(framesPerChunk * bw64Channels);

        uint64_t framesRemaining = expectedFrames;
        while (framesRemaining > 0) {
            uint64_t toRead = std::min(framesPerChunk, framesRemaining);

            // clear interleave buf
            std::fill(interleaveBuf.begin(), interleaveBuf.begin() + (toRead * bw64Channels), 0.0f);

            for (size_t c = 0; c < bw64Channels; ++c) {
                streams[c].read(reinterpret_cast<char*>(monoChunk.data()), toRead * sizeof(float));
                uint64_t readCount = streams[c].gcount() / sizeof(float);

                // missing frames gets padded with 0.0f
                for (size_t f = 0; f < readCount; ++f) {
                    interleaveBuf[f * bw64Channels + c] = monoChunk[f];
                }
            }

            bw64Writer.write(interleaveBuf.data(), toRead);
            framesRemaining -= toRead;
        }

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Bw64Writer error: ") + e.what();
        return result;
    }

    // 5. Rename files safely
    if (std::rename(tmpXmlPath.c_str(), outXmlPath.c_str()) != 0) {
        result.errorMessage = "Failed to finalize XML file";
        return result;
    }
    if (std::rename(tmpWavPath.c_str(), outWavPath.c_str()) != 0) {
        result.errorMessage = "Failed to finalize WAV file";
        return result;
    }

    result.success = true;
    result.channelCount = bw64Channels;
    result.objectCount = objectCount;

    return result;
}

} // namespace cult
