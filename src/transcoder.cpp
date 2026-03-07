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
#include "adm_reader.hpp"            // Phase 3: BW64 axml extraction
#include "adm_profile_resolver.hpp"  // Phase 4: ADM profile detection
#include <pugixml.hpp>

#include <filesystem>
#include <set>

namespace cult {

namespace {

// Formats recognised by the CLI contract (§2).
// adm_xml  → Phase 2: input is a pre-extracted ADM XML file
// adm_wav  → Phase 3: input is a BW64/RF64 WAV containing an axml chunk
const std::set<std::string> kKnownInFormats  = { "adm_xml", "adm_wav" };
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
    report.args.lfeMode      = (req.lfeMode == LfeMode::SpeakerLabel)
                                   ? "speaker-label" : "hardcoded";

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
    ConversionResult conversion;

    if (req.inFormat == "adm_xml") {
        // Phase 2 path: input is a pre-extracted ADM XML file.
        // Phase 4: load doc first so we can run profile detection before converting.
        pugi::xml_document doc;
        auto parseRes = doc.load_file(req.inPath.c_str());
        if (!parseRes) {
            report.errors.push_back(
                "Failed to parse XML: " + std::string(parseRes.description()));
            report.status = "fail";
            return result;
        }

        // Profile detection (Phase 4) — always runs, always logs to warnings.
        ProfileResult profile = resolveAdmProfile(doc);
        for (auto& w : profile.warnings)
            report.warnings.push_back(w);

        conversion = convertAdmToLusid(req.inPath, req.lfeMode);

    } else {
        // Phase 3 path: input is a BW64/WAV file containing an axml chunk.
        auto axmlResult = extractAxmlFromWav(req.inPath);

        // Forward warnings from extraction (e.g. debug XML artifact write failure)
        for (auto& w : axmlResult.warnings)
            report.warnings.push_back(w);

        if (!axmlResult.success) {
            for (auto& e : axmlResult.errors)
                report.errors.push_back(e);
            report.status = "fail";
            return result;
        }

        // Phase 4: profile detection on the extracted XML buffer.
        pugi::xml_document doc;
        doc.load_string(axmlResult.xmlData.c_str());
        ProfileResult profile = resolveAdmProfile(doc);
        for (auto& w : profile.warnings)
            report.warnings.push_back(w);

        // Parse directly from the in-memory XML buffer — no disk re-read.
        conversion = convertAdmToLusidFromBuffer(axmlResult.xmlData, req.lfeMode);
    }

    // Forward warnings from conversion
    for (auto& w : conversion.warnings)
        report.warnings.push_back(w);

    if (!conversion.success) {
        for (auto& e : conversion.errors)
            report.errors.push_back(e);
        report.status = "fail";
        return result;
    }

    // Write LUSID scene JSON — atomic write (work item 5, Phase 3).
    // Write to a temp file first, then rename to final path so no partial
    // artifact is ever visible at the final path (AGENTS §2 atomic rule).
    {
        const std::string tmpOut = req.outPath + ".tmp";
        if (!writeLusidScene(conversion.scene, tmpOut)) {
            report.errors.push_back(
                "Failed to write output file: '" + req.outPath + "'");
            report.status = "fail";
            // Clean up temp file if it exists
            std::filesystem::remove(tmpOut);
            return result;
        }
        std::error_code ec;
        std::filesystem::rename(tmpOut, req.outPath, ec);
        if (ec) {
            report.errors.push_back(
                "Failed to finalize output file (rename failed): '" +
                req.outPath + "': " + ec.message());
            report.status = "fail";
            std::filesystem::remove(tmpOut);
            return result;
        }
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
