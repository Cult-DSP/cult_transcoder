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
#include "audio/resampler.hpp"
#include "audio/wav_io.hpp"

#include <filesystem>
#include <string>
#include <vector>

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

    fs::path normalizedDir = workPath / "normalized_audio";
    if (!normalizedDir.empty()) {
        std::error_code ec;
        fs::create_directories(normalizedDir, ec);
        if (ec) {
            result.errors.push_back("Failed to create normalized audio dir: " + normalizedDir.string());
            return result;
        }
    }

    for (const auto& input : inputs) {
        if (input.channels != 1) {
            result.errors.push_back("Unsupported WAV (only mono supported): " + input.path);
            continue;
        }
        if (input.audioFormat != 1 && input.audioFormat != 3) {
            result.errors.push_back("Unsupported WAV format: " + input.path);
            continue;
        }

        const bool needsResample = input.sampleRate != targetSampleRate;
        const bool needsFloat = !(input.audioFormat == 3 && input.bitsPerSample == 32);
        if (!needsResample && !needsFloat) {
            NormalizedWavInfo info;
            info.sourcePath = input.path;
            info.normalizedPath = input.path;
            info.sourceSampleRate = input.sampleRate;
            info.targetSampleRate = targetSampleRate;
            info.sourceFrameCount = input.frameCount;
            info.normalizedFrameCount = input.frameCount;
            info.resampled = false;
            result.files.push_back(info);
            continue;
        }

        fs::path outPath = normalizedDir / (fs::path(input.path).stem().string() + "_48k.wav");
        NormalizeRequest req;
        req.inputPath = input.path;
        req.outputPath = outPath.string();
        req.sourceSampleRate = input.sampleRate;
        req.targetSampleRate = targetSampleRate;
        req.sourceFrameCount = input.frameCount;

        NormalizeResult normalized = normalizeMonoWavTo48kFloat32(req);
        if (!normalized.error.empty()) {
            result.errors.push_back("Normalization failed: " + normalized.error);
            continue;
        }

        NormalizedWavInfo info;
        info.sourcePath = normalized.sourcePath;
        info.normalizedPath = normalized.normalizedPath;
        info.sourceSampleRate = normalized.sourceSampleRate;
        info.targetSampleRate = normalized.targetSampleRate;
        info.sourceFrameCount = normalized.sourceFrameCount;
        info.normalizedFrameCount = normalized.normalizedFrameCount;
        info.resampled = normalized.resampled;
        result.files.push_back(info);
    }

    if (!result.errors.empty()) {
        return result;
    }

    result.success = true;
    return result;
}

} // namespace cult
