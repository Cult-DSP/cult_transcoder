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
// resampler.hpp — r8brain-backed mono WAV resampling helper
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>

namespace cult {

struct NormalizeRequest {
    std::string inputPath;
    std::string outputPath;
    uint32_t    sourceSampleRate = 0;
    uint32_t    targetSampleRate = 0;
    uint64_t    sourceFrameCount = 0;
};

struct NormalizeResult {
    std::string sourcePath;
    std::string normalizedPath;
    uint32_t    sourceSampleRate = 0;
    uint32_t    targetSampleRate = 0;
    uint64_t    sourceFrameCount = 0;
    uint64_t    normalizedFrameCount = 0;
    bool        resampled = false;
    std::string error;
};

NormalizeResult normalizeMonoWavTo48kFloat32(const NormalizeRequest& request);

} // namespace cult
