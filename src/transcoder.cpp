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
// transcoder.cpp — Core transcode() implementation
//
// Phase 2 behaviour:
//   - Validates that --in file exists on disk.
//   - Validates that --in-format / --out-format are recognised.
//   - On any failure: fills report with status="fail" + error message.
//   - On success: converts ADM XML → LUSID Scene JSON via adm_to_lusid,
//     writes output file, populates report summary with real stats.
//
// The function signature (TranscodeRequest / TranscodeResult) is stable and
// must NOT change between phases.
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "cult_version.hpp"
#include "adm_to_lusid.hpp"

#include <filesystem>
#include <set>

namespace cult {

namespace {

// Formats recognised by the CLI contract (§2).  Only adm_xml→lusid_json is
// implemented; others are rejected with a clear error so the contract exit
// code semantics are exercised.
const std::set<std::string> kKnownInFormats  = { "adm_xml" };
const std::set<std::string> kKnownOutFormats = { "lusid_json" };

} // anonymous namespace

// ---------------------------------------------------------------------------
// transcode()
// ---------------------------------------------------------------------------
TranscodeResult transcode(const TranscodeRequest& req) {
    TranscodeResult result;
    Report& report = result.report;

    // Populate args for the report (§7: args must reflect resolved values)
    report.args.inPath       = req.inPath;
    report.args.inFormat     = req.inFormat;
    report.args.outPath      = req.outPath;
    report.args.outFormat    = req.outFormat;
    report.args.reportPath   = req.reportPath;
    report.args.stdoutReport = req.stdoutReport;

    // --- Validation ---

    // 1. --in-format must be recognised
    if (kKnownInFormats.find(req.inFormat) == kKnownInFormats.end()) {
        report.errors.push_back(
            "Unsupported --in-format: '" + req.inFormat +
            "'. Supported: adm_xml");
        report.status = "fail";
        return result;
    }

    // 2. --out-format must be recognised
    if (kKnownOutFormats.find(req.outFormat) == kKnownOutFormats.end()) {
        report.errors.push_back(
            "Unsupported --out-format: '" + req.outFormat +
            "'. Supported: lusid_json");
        report.status = "fail";
        return result;
    }

    // 3. --in file must exist
    if (!req.inPath.empty() && !std::filesystem::exists(req.inPath)) {
        report.errors.push_back(
            "Input file not found: '" + req.inPath + "'");
        report.status = "fail";
        return result;
    }

    // 4. --out must not be empty
    if (req.outPath.empty()) {
        report.errors.push_back("--out path must not be empty");
        report.status = "fail";
        return result;
    }

    // --- ADM → LUSID Conversion ---
    auto conversion = convertAdmToLusid(req.inPath);

    // Forward warnings from conversion
    for (auto& w : conversion.warnings)
        report.warnings.push_back(w);

    if (!conversion.success) {
        for (auto& e : conversion.errors)
            report.errors.push_back(e);
        report.status = "fail";
        return result;
    }

    // Write LUSID scene JSON
    if (!writeLusidScene(conversion.scene, req.outPath)) {
        report.errors.push_back(
            "Failed to write output file: '" + req.outPath + "'");
        report.status = "fail";
        return result;
    }

    // --- Populate report summary ---
    report.status = "success";
    report.summary.timeUnit    = "seconds";
    report.summary.numFrames   = conversion.numFrames;
    report.summary.sampleRate  = conversion.scene.sampleRate;
    report.summary.durationSec = (conversion.scene.duration >= 0.0)
                                    ? conversion.scene.duration
                                    : 0.0;
    report.summary.countsByNodeType = conversion.countsByNodeType;

    result.success = true;
    return result;
}

} // namespace cult
