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
// cult_version.hpp — Version constants for CULT Transcoder
//
// CULT_VERSION_STRING  Human-readable semver string embedded in every report.
// CULT_REPORT_SCHEMA   Report schema version; increment independently of tool
//                      version when the report JSON structure changes.
// ---------------------------------------------------------------------------

#include <string>

namespace cult {

constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 1;
constexpr int kVersionPatch = 0;

// Human-readable "MAJOR.MINOR.PATCH"
constexpr const char* kVersionString = "0.1.0";

// Report schema version (v0.1 per contract §7)
constexpr const char* kReportSchemaVersion = "0.1";

// Tool name embedded in every report (§7: tool.name)
constexpr const char* kToolName = "cult-transcoder";

// ---------------------------------------------------------------------------
// Git commit SHA — injected at build time via CMake configure_file or a
// compile-time definition.  Falls back to "unknown" if not defined.
// ---------------------------------------------------------------------------
#ifndef CULT_GIT_COMMIT
#  define CULT_GIT_COMMIT "unknown"
#endif

inline std::string gitCommit() { return CULT_GIT_COMMIT; }

} // namespace cult
