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
// cult_report.hpp — Report data model and serialisation for CULT Transcoder
//
// Implements Report Schema v0.1 (AGENTS §7).
//
// Contract requirements:
//   - reportVersion     "0.1"
//   - tool              { name, version, gitCommit }
//   - args              full resolved CLI args/options used
//   - status            "success" | "fail"
//   - errors[]          free-form strings
//   - warnings[]        free-form strings
//   - summary           { durationSec, sampleRate, timeUnit, numFrames,
//                         countsByNodeType }
//   - lossLedger[]      required even when empty; each entry has
//                         { kind, field, reason, count, examples[] }
//
// Report is serialised to JSON manually (no third-party JSON lib in Phase 1)
// so there is zero extra dependency.  Phase 2 can swap in nlohmann/json if
// desired — the public API of this header stays stable.
// ---------------------------------------------------------------------------

#include <map>
#include <string>
#include <vector>

namespace cult {

// ---------------------------------------------------------------------------
// LossLedgerEntry — one entry in the lossLedger array (§7)
// ---------------------------------------------------------------------------
struct LossLedgerEntry {
    std::string kind;            // e.g. "truncated", "unsupported"
    std::string field;           // JSON path of the affected field
    std::string reason;          // human-readable explanation
    int         count = 0;       // how many occurrences
    std::vector<std::string> examples; // up to ~3 example values
};

// ---------------------------------------------------------------------------
// ReportSummary — the summary sub-object (§7)
// ---------------------------------------------------------------------------
struct ReportSummary {
    double      durationSec  = 0.0;
    int         sampleRate   = 0;
    std::string timeUnit     = "seconds"; // always "seconds" in Phase 2 (§4)
    int         numFrames    = 0;
    std::map<std::string, int> countsByNodeType; // e.g. {"DirectSpeaker":8,"Object":4}
};

// ---------------------------------------------------------------------------
// ResolvedArgs — the CLI args recorded verbatim in the report (§7)
// ---------------------------------------------------------------------------
struct ResolvedArgs {
    std::string inPath;
    std::string inFormat;
    std::string outPath;
    std::string outFormat;
    std::string reportPath;
    bool        stdoutReport = false;
};

// ---------------------------------------------------------------------------
// Report — top-level report object
// ---------------------------------------------------------------------------
struct Report {
    // Populated by writeReport() / the CLI before any transcoding begins.
    ResolvedArgs             args;

    // Populated as transcoding proceeds.
    std::string              status   = "fail"; // "success" | "fail"
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    ReportSummary            summary;
    std::vector<LossLedgerEntry> lossLedger; // required even when empty

    // ---------------------------------------------------------------------------
    // Serialise to a JSON string.
    // Suitable for writing to <out>.report.json or printing to stdout.
    // ---------------------------------------------------------------------------
    [[nodiscard]] std::string toJson() const;

    // ---------------------------------------------------------------------------
    // Write the report JSON to the given file path.
    // Returns true on success, false if the file could not be written.
    // On error, a message is appended to stderr (not to this report — the report
    // itself may be in a fail state already).
    // ---------------------------------------------------------------------------
    bool writeTo(const std::string& path) const;

    // ---------------------------------------------------------------------------
    // Print the report JSON to stdout (used when --stdout-report is set).
    // ---------------------------------------------------------------------------
    void printToStdout() const;
};

} // namespace cult
