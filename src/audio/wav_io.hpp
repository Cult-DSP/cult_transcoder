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
// wav_io.hpp — minimal WAV inspection helpers for authoring
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>

namespace cult {

struct WavFileInfo {
    std::string path;
    uint16_t audioFormat = 0;  // 1 = PCM, 3 = IEEE float
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint64_t frameCount = 0;
};

bool readWavInfo(const std::string& path, WavFileInfo& info, std::string& error);
bool readWavMonoSamples(const std::string& path,
                         WavFileInfo& info,
                         std::vector<float>& samples,
                         std::string& error);
bool writeFloat32MonoWav(const std::string& path,
                         uint32_t sampleRate,
                         const std::vector<float>& samples,
                         std::string& error);

} // namespace cult
