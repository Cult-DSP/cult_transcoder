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
// adm_author.cpp — LUSID -> ADM authoring entry point (stub)
//
// This file implements the admAuthor() API surface. The real authoring
// pipeline (LUSID read, normalization, mapping, XML/BW64 write) will be
// layered behind this entry point in incremental steps.
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "authoring/authoringHelper.hpp"
#include "parsing/lusid_reader.hpp"
#include "audio/normalize_audio.hpp"
#include "audio/wav_io.hpp"
#include "adm_writer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <vector>

namespace fs = std::filesystem;

namespace cult {

// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
namespace {
using authoring_helpers::collectNodeIds;
using authoring_helpers::countUniqueSupportedNodeTypes;
using authoring_helpers::extractDbmdFromRiff;
using authoring_helpers::resolveScenePath;
using authoring_helpers::resolveWavDir;
} // namespace

AdmAuthorResult admAuthor(const AdmAuthorRequest& req) {
    AdmAuthorResult result;
    Report& report = result.report;

    // Populate base args for report continuity (v0.1 fields must remain present).
    report.args.inPath       = req.lusidPath.empty() ? req.lusidPackage : req.lusidPath;
    report.args.inFormat     = "lusid_json";
    report.args.outPath      = req.outWavPath.empty() ? req.outXmlPath : req.outWavPath;
    report.args.outFormat    = "adm_bw64";
    report.args.reportPath   = req.reportPath;
    report.args.stdoutReport = req.stdoutReport;

    const bool hasPackage = !req.lusidPackage.empty();
    const bool hasExplicit = !req.lusidPath.empty() || !req.wavDir.empty();

    if (hasPackage && hasExplicit) {
        report.errors.push_back("adm-author: --lusid-package is mutually exclusive with --lusid/--wav-dir");
        report.status = "fail";
        return result;
    }

    if (!hasPackage) {
        if (req.lusidPath.empty()) {
            report.errors.push_back("adm-author: --lusid is required when --lusid-package is not provided");
        }
        if (req.wavDir.empty()) {
            report.errors.push_back("adm-author: --wav-dir is required when --lusid-package is not provided");
        }
    }

    if (req.outXmlPath.empty()) {
        report.errors.push_back("adm-author: --out-xml is required");
    }
    if (req.outWavPath.empty()) {
        report.errors.push_back("adm-author: --out-wav is required");
    }

    if (!report.errors.empty()) {
        report.status = "fail";
        return result;
    }

    std::vector<char> dbmdData;
    if (!req.dbmdSourcePath.empty()) {
        std::string dbmdError;
        if (!extractDbmdFromRiff(req.dbmdSourcePath, dbmdData, dbmdError)) {
            report.errors.push_back("adm-author: " + dbmdError);
            report.status = "fail";
            return result;
        }
        report.warnings.push_back("adm-author: copied experimental dbmd chunk from " + req.dbmdSourcePath);
    }
    if (req.metadataPostData) {
        report.warnings.push_back("adm-author: wrote experimental post-data axml/chna/dbmd chunk order");
    }

    if (hasPackage && !fs::exists(req.lusidPackage)) {
        report.errors.push_back("adm-author: LUSID package not found: '" + req.lusidPackage + "'");
        report.status = "fail";
        return result;
    }

    if (!hasPackage) {
        if (!fs::exists(req.lusidPath)) {
            report.errors.push_back("adm-author: LUSID scene not found: '" + req.lusidPath + "'");
        }
        if (!fs::exists(req.wavDir)) {
            report.errors.push_back("adm-author: WAV directory not found: '" + req.wavDir + "'");
        }
        if (!report.errors.empty()) {
            report.status = "fail";
            return result;
        }
    }

    const std::string scenePath = resolveScenePath(req);
    const std::string wavDir = resolveWavDir(req);

    if (!fs::exists(scenePath)) {
        report.errors.push_back("adm-author: LUSID scene not found: '" + scenePath + "'");
        report.status = "fail";
        return result;
    }
    if (!fs::exists(wavDir)) {
        report.errors.push_back("adm-author: WAV directory not found: '" + wavDir + "'");
        report.status = "fail";
        return result;
    }

    if (req.onProgress) {
        req.onProgress(ProgressEvent{"metadata", 0, 1, "reading LUSID scene"});
    }

    auto sceneResult = readLusidScene(scenePath);
    if (!sceneResult.success) {
        for (const auto& e : sceneResult.errors) {
            report.errors.push_back("adm-author: " + e);
        }
        report.status = "fail";
        return result;
    }

    const auto nodeIds = collectNodeIds(sceneResult.scene);
    if (nodeIds.empty()) {
        report.errors.push_back("adm-author: no supported nodes found in scene");
        report.status = "fail";
        return result;
    }

    std::vector<WavFileInfo> wavInfos;
    if (req.onProgress) {
        req.onProgress(ProgressEvent{"metadata", 1, 1, "loaded LUSID scene"});
        req.onProgress(ProgressEvent{"inspect", 0, static_cast<uint64_t>(nodeIds.size()), "checking source stems"});
    }
    for (const auto& id : nodeIds) {
        fs::path wavPath = fs::path(wavDir);
        if (id == "4.1") {
            wavPath /= "LFE.wav";
        } else {
            wavPath /= (id + ".wav");
        }
        if (!fs::exists(wavPath)) {
            report.errors.push_back("adm-author: missing WAV for node '" + id + "' at " + wavPath.string());
            continue;
        }

        WavFileInfo info;
        std::string wavError;
        if (!readWavInfo(wavPath.string(), info, wavError)) {
            report.errors.push_back("adm-author: " + wavError + " (" + wavPath.string() + ")");
            continue;
        }
        wavInfos.push_back(info);
        if (req.onProgress) {
            req.onProgress(ProgressEvent{"inspect", static_cast<uint64_t>(wavInfos.size()), static_cast<uint64_t>(nodeIds.size()), "checking source stems"});
        }
    }

    if (!report.errors.empty()) {
        report.status = "fail";
        return result;
    }

    const uint32_t targetSampleRate = 48000;
    fs::path workDir = fs::path(req.outWavPath + ".work");
    if (req.onProgress) {
        req.onProgress(ProgressEvent{"normalize", 0, static_cast<uint64_t>(wavInfos.size()), "normalizing source stems"});
    }
    auto normalized = normalizeWavSet(wavInfos, workDir.string(), targetSampleRate);
    if (!normalized.success) {
        for (const auto& e : normalized.errors) {
            report.errors.push_back("adm-author: " + e);
        }
        report.status = "fail";
        return result;
    }

    report.authoringResample = normalized.files;
    if (req.onProgress) {
        req.onProgress(ProgressEvent{"normalize", static_cast<uint64_t>(normalized.files.size()), static_cast<uint64_t>(wavInfos.size()), "normalized source stems"});
    }

    report.hasAuthoringValidation = true;
    uint64_t minFrames = std::numeric_limits<uint64_t>::max();
    uint64_t maxFrames = 0;
    for (const auto& n : normalized.files) {
        minFrames = std::min(minFrames, n.normalizedFrameCount);
        maxFrames = std::max(maxFrames, n.normalizedFrameCount);
    }

    uint64_t expectedFrames = minFrames;
    bool frameMismatch = false;
    bool toleratedTailMismatch = false;
    if (maxFrames != minFrames) {
        frameMismatch = true;
        toleratedTailMismatch = (maxFrames - minFrames) <= 1;
    }

    for (const auto& n : normalized.files) {
        AuthoringValidationFile vf;
        vf.path = n.normalizedPath;
        vf.frames = n.normalizedFrameCount;
        vf.framesUsed = std::min(n.normalizedFrameCount, expectedFrames);
        vf.truncatedToExpected = n.normalizedFrameCount > expectedFrames;
        vf.ok = (n.normalizedFrameCount == expectedFrames) ||
                (toleratedTailMismatch && vf.truncatedToExpected);
        report.authoringValidation.files.push_back(vf);
    }
    report.authoringValidation.expectedFrames = expectedFrames;

    if (frameMismatch && toleratedTailMismatch) {
        report.warnings.push_back(
            "adm-author: normalized WAV frame counts differ by one frame; authoring uses the shortest length and ignores trailing samples");

        LossLedgerEntry loss;
        loss.kind = "truncated";
        loss.field = "authoringValidation.files";
        loss.reason = "One trailing sample ignored from longer normalized WAV files to tolerate end-of-file frame-count drift.";
        for (const auto& f : report.authoringValidation.files) {
            if (f.truncatedToExpected) {
                ++loss.count;
                if (loss.examples.size() < 3) {
                    loss.examples.push_back(f.path);
                }
            }
        }
        if (loss.count > 0) {
            report.lossLedger.push_back(loss);
        }
    } else if (frameMismatch) {
        report.errors.push_back("adm-author: normalized WAV frame counts do not match");
    }

    if (sceneResult.scene.duration >= 0.0) {
        const uint64_t expectedFromScene = static_cast<uint64_t>(
            std::llround(sceneResult.scene.duration * static_cast<double>(targetSampleRate)));
        if (expectedFromScene != expectedFrames) {
            report.errors.push_back(
                "adm-author: scene duration does not match audio frame count");
        }
    }

    if (!report.errors.empty()) {
        report.status = "fail";
        return result;
    }

    report.summary.durationSec = static_cast<double>(expectedFrames) / static_cast<double>(targetSampleRate);
    report.summary.sampleRate = static_cast<int>(targetSampleRate);
    report.summary.timeUnit = "seconds";
    report.summary.numFrames = static_cast<int>(sceneResult.scene.frames.size());
    report.summary.countsByNodeType = countUniqueSupportedNodeTypes(sceneResult.scene);

    // Prepare normalized wav info
    std::vector<WavFileInfo> normWavInfos;
    for (const auto& n : normalized.files) {
        WavFileInfo wi;
        std::string err;
        if (!readWavInfo(n.normalizedPath, wi, err)) {
            report.errors.push_back("adm-author: failed to read normalized WAV: " + err);
            report.status = "fail";
            return result;
        }
        normWavInfos.push_back(wi);
    }

    // -------------------------------------------------------------
    // Delegate to AdmWriter
    // -------------------------------------------------------------
    AdmWriter writer;
    auto wRes = writer.writeAdmBw64(
        req.outXmlPath,
        req.outWavPath,
        sceneResult.scene,
        normWavInfos,
        targetSampleRate,
        expectedFrames,
        req.onProgress,
        dbmdData,
        req.metadataPostData
    );

    if (!wRes.success) {
        report.errors.push_back(wRes.errorMessage);
        report.status = "fail";
        return result;
    }

    report.status = "pass";
    result.success = true;
    return result;
}

} // namespace cult
