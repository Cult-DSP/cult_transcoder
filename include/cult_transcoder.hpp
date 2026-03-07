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
// cult_transcoder.hpp — Public API for the CULT Transcoder core
//
// Phase 1: only the TranscodeRequest / TranscodeResult types and the main
//          transcode() entry point are defined here.  The function body is a
//          stub that always returns a fail result until Phase 2.
//
// Phase 2: adm_reader / adm_to_lusid implementations will be wired in via
//          the transcoding/ subdirectory; this header remains the stable
//          call-site for the CLI.
// ---------------------------------------------------------------------------

#include "cult_report.hpp"
#include "adm_to_lusid.hpp"  // LfeMode enum

#include <string>

namespace cult {

// ---------------------------------------------------------------------------
// TranscodeRequest — everything the transcoder needs from the CLI
// ---------------------------------------------------------------------------
struct TranscodeRequest {
    std::string inPath;
    std::string inFormat;   // "adm_xml" | "adm_wav"
    std::string outPath;
    std::string outFormat;  // "lusid_json"
    std::string reportPath; // resolved path for the report file
    bool        stdoutReport = false;
    LfeMode     lfeMode = LfeMode::Hardcoded; // Phase 4: --lfe-mode flag
};

// ---------------------------------------------------------------------------
// TranscodeResult — returned by transcode()
// ---------------------------------------------------------------------------
struct TranscodeResult {
    bool   success = false;
    Report report;          // fully populated; caller writes it to disk
};

// ---------------------------------------------------------------------------
// transcode() — main entry point
//
// Phase 1 behaviour (stub):
//   - Validates that inPath exists and formats are recognised.
//   - On any validation failure: populates report.status = "fail",
//     appends to report.errors, returns success = false.
//   - On valid-looking args: returns success = true with a stub report
//     (status = "success", empty summary — no actual conversion yet).
//
// Phase 2 will replace the stub body with real ADM→LUSID conversion.
// The function signature must NOT change between phases.
// ---------------------------------------------------------------------------
TranscodeResult transcode(const TranscodeRequest& req);

} // namespace cult
