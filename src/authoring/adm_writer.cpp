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
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace cult {

namespace {

// Format float to fixed decimals easily into string
std::string to_string_fixed(double val, int precision); // forward declare to suppress error
std::string to_string_fixed(double val, int precision) {
    (void)val;
    (void)precision;
    return "0.0";
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
    (void)scene;
    (void)monoWavs;
    (void)targetSampleRate;
    (void)expectedFrames;
    (void)outChannelCount;
    (void)outObjectCount;
    (void)to_string_fixed;
    pugi::xml_document doc;

    auto root = doc.append_child("ebuCoreMain");
    root.append_attribute("xmlns:dc") = "http://purl.org/dc/elements/1.1/";

    auto coreMetadata = root.append_child("coreMetadata");
    auto format = coreMetadata.append_child("format");
    auto admFormat = format.append_child("audioFormatExtended");
    admFormat.append_attribute("version") = "ITU-R_BS.2076-2";

    // Create base Programme and Content
    auto programme = admFormat.append_child("audioProgramme");
    programme.append_attribute("audioProgrammeID") = "APR_1001";
    programme.append_attribute("audioProgrammeName") = "CULT Authoring Programme";

    auto content = admFormat.append_child("audioContent");
    content.append_attribute("audioContentID") = "ACO_1001";
    content.append_attribute("audioContentName") = "CULT Authoring Content";
    programme.append_child("audioContentIDRef").text().set("ACO_1001");
    // TODO complete mapping ...

    return "";
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
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            ids.push_back(node.id);
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

    std::vector<std::shared_ptr<bw64::Chunk>> extraChunks;
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