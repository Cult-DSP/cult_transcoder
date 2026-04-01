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
// resampler_r8brain.cpp — r8brain-backed mono WAV resampling helper
// ---------------------------------------------------------------------------

#include "audio/resampler.hpp"
#include "audio/wav_io.hpp"

#include <CDSPResampler.h>

#include <cmath>
#include <vector>

namespace cult {

namespace {

uint64_t estimateOutputFrames(uint64_t inputFrames,
                              uint32_t sourceRate,
                              uint32_t targetRate) {
    if (sourceRate == 0 || targetRate == 0) return 0;
    const double ratio = static_cast<double>(targetRate) / static_cast<double>(sourceRate);
    return static_cast<uint64_t>(std::llround(static_cast<double>(inputFrames) * ratio));
}

} // namespace

NormalizeResult normalizeMonoWavTo48kFloat32(const NormalizeRequest& request) {
    NormalizeResult result;
    result.sourcePath = request.inputPath;
    result.normalizedPath = request.outputPath;
    result.sourceSampleRate = request.sourceSampleRate;
    result.targetSampleRate = request.targetSampleRate;
    result.sourceFrameCount = request.sourceFrameCount;

    std::vector<float> inputSamples;
    WavFileInfo info;
    std::string error;
    if (!readWavMonoSamples(request.inputPath, info, inputSamples, error)) {
        result.error = error;
        return result;
    }

    if (request.sourceSampleRate == request.targetSampleRate) {
        if (!writeFloat32MonoWav(request.outputPath,
                                 request.targetSampleRate,
                                 inputSamples,
                                 error)) {
            result.error = error;
            return result;
        }
        result.normalizedFrameCount = inputSamples.size();
        result.resampled = false;
        return result;
    }

    const int blockSize = 8192;
    r8b::CDSPResampler resampler(
        static_cast<double>(request.sourceSampleRate),
        static_cast<double>(request.targetSampleRate),
        blockSize);

    const uint64_t expectedFrames = estimateOutputFrames(
        request.sourceFrameCount,
        request.sourceSampleRate,
        request.targetSampleRate);

    std::vector<float> outputSamples;
    outputSamples.reserve(static_cast<size_t>(expectedFrames + blockSize));

    std::vector<double> inputBlock(static_cast<size_t>(blockSize));
    size_t readIndex = 0;
    uint64_t outputFrames = 0;

    while (readIndex < inputSamples.size()) {
        const size_t remaining = inputSamples.size() - readIndex;
        const int readCount = static_cast<int>(remaining > static_cast<size_t>(blockSize)
                                               ? blockSize
                                               : remaining);
        for (int i = 0; i < readCount; ++i) {
            inputBlock[static_cast<size_t>(i)] = inputSamples[readIndex + static_cast<size_t>(i)];
        }

        double* outPtr = nullptr;
        const int outCount = resampler.process(inputBlock.data(), readCount, outPtr);
        const uint64_t usable = static_cast<uint64_t>(outCount);
        const uint64_t capped = (outputFrames + usable > expectedFrames)
                                    ? (expectedFrames - outputFrames)
                                    : usable;
        for (uint64_t i = 0; i < capped; ++i) {
            outputSamples.push_back(static_cast<float>(outPtr[i]));
        }
        outputFrames += capped;
        readIndex += static_cast<size_t>(readCount);
    }

    while (outputFrames < expectedFrames) {
        std::fill(inputBlock.begin(), inputBlock.end(), 0.0);
        double* outPtr = nullptr;
        const int outCount = resampler.process(inputBlock.data(), blockSize, outPtr);
        if (outCount <= 0) break;
        const uint64_t usable = static_cast<uint64_t>(outCount);
        const uint64_t capped = (outputFrames + usable > expectedFrames)
                                    ? (expectedFrames - outputFrames)
                                    : usable;
        for (uint64_t i = 0; i < capped; ++i) {
            outputSamples.push_back(static_cast<float>(outPtr[i]));
        }
        outputFrames += capped;
    }

    if (!writeFloat32MonoWav(request.outputPath,
                             request.targetSampleRate,
                             outputSamples,
                             error)) {
        result.error = error;
        return result;
    }

    result.normalizedFrameCount = outputSamples.size();
    result.resampled = true;
    return result;
}

} // namespace cult
