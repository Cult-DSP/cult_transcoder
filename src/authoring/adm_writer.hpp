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

#pragma once

// ---------------------------------------------------------------------------
// adm_writer.hpp - LUSID to ADM Mapping & BW64 Audio generation
// ---------------------------------------------------------------------------

#include "lusid_scene.hpp"
#include "audio/wav_io.hpp"
#include "progress.hpp"

#include <string>
#include <vector>
#include <map>

namespace cult {

struct AdmWriterResult {
    bool success = false;
    std::string errorMessage;
    uint32_t channelCount = 0;
    uint32_t objectCount = 0;
};

// ---------------------------------------------------------------------------
// Maps LUSID Scene directly into ADM XML String (via pugixml).
// Then orchestrates multi-channel streaming of WavFileInfo to the dest wav
// alongside the AXML.
// ---------------------------------------------------------------------------
class AdmWriter {
public:
    AdmWriter() = default;

    // Orchestrates mapping + generating ADM BW64
    AdmWriterResult writeAdmBw64(
        const std::string& outXmlPath,
        const std::string& outWavPath,
        const LusidScene& scene,
        const std::vector<WavFileInfo>& monoWavs,
        uint32_t targetSampleRate,
        uint64_t expectedFrames,
        const ProgressCallback& onProgress = nullptr,
        const std::vector<char>& dbmdData = {}
    );

private:
    std::string generateAdmXml(const LusidScene& scene, const std::vector<WavFileInfo>& monoWavs, uint32_t targetSampleRate, uint64_t expectedFrames, uint32_t& outChannelCount, uint32_t& outObjectCount);

    // Convert float time to ADM S48000 format
    std::string formatAdmTime(double timeSeconds, uint32_t sampleRate) const;

    // Sort logic
    std::vector<std::string> sortNodeIds(const std::vector<std::string>& ids) const;
};

} // namespace cult
