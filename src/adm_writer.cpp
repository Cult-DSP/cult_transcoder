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
std::string to_string_fixed(double val, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << val;
    return ss.str();
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
    (void)outXmlPath;
    (void)outWavPath;
    (void)monoWavs;
    (void)targetSampleRate;
    (void)expectedFrames;
    (void)to_string_fixed;
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

    result.errorMessage = "Not fully implemented buffer interlace";
    result.success = false;

    return result;
}

} // namespace cult
