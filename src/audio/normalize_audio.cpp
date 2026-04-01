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
// normalize_audio.cpp — authoring audio normalization scaffolding
// ---------------------------------------------------------------------------

#include "audio/normalize_audio.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace cult {

NormalizeSetResult normalizeWavSet(const std::vector<WavFileInfo>& inputs,
                                   const std::string& workDir,
                                   uint32_t targetSampleRate) {
    NormalizeSetResult result;

    if (inputs.empty()) {
        result.errors.push_back("No WAV inputs provided for normalization");
        return result;
    }

    fs::path workPath(workDir);
    if (!workPath.empty()) {
        std::error_code ec;
        fs::create_directories(workPath, ec);
        if (ec) {
            result.errors.push_back("Failed to create normalization work dir: " + workDir);
            return result;
        }
    }

    for (const auto& input : inputs) {
        NormalizedWavInfo info;
        info.sourcePath = input.path;
        info.normalizedPath = input.path;
        info.sourceSampleRate = input.sampleRate;
        info.targetSampleRate = targetSampleRate;
        info.sourceFrameCount = input.frameCount;
        info.normalizedFrameCount = input.frameCount;
        info.resampled = false;

        if (input.sampleRate != targetSampleRate) {
            result.errors.push_back(
                "Resampling not implemented yet for: " + input.path);
        }

        result.files.push_back(info);
    }

    if (!result.errors.empty()) {
        return result;
    }

    result.success = true;
    return result;
}

} // namespace cult
