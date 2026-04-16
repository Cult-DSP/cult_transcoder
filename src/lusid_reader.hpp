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
// lusid_reader.hpp — minimal LUSID scene reader for authoring
// ---------------------------------------------------------------------------

#include "adm_to_lusid.hpp"

#include <string>
#include <vector>

namespace cult {

struct LusidSceneLoadResult {
    bool success = false;
    LusidScene scene;
    std::vector<std::string> errors;
};

LusidSceneLoadResult readLusidScene(const std::string& path);

} // namespace cult
