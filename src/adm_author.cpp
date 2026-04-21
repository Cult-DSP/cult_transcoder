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
#include "lusid_reader.hpp"
#include "audio/normalize_audio.hpp"
#include "audio/wav_io.hpp"
#include "adm_writer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace cult {

namespace {

std::string resolveScenePath(const AdmAuthorRequest& req) {
    if (!req.lusidPackage.empty()) {
        fs::path pkg(req.lusidPackage);
        return (pkg / "scene.lusid.json").string();
    }
    return req.lusidPath;
}

std::string resolveWavDir(const AdmAuthorRequest& req) {
    if (!req.lusidPackage.empty()) {
        return req.lusidPackage;
    }
    return req.wavDir;
}

std::vector<std::string> collectNodeIds(const LusidScene& scene) {
    std::vector<std::string> ids;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (node.type == "direct_speaker" || node.type == "audio_object" || node.type == "LFE") {
                ids.push_back(node.id);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

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
    }

    if (!report.errors.empty()) {
        report.status = "fail";
        return result;
    }

    const uint32_t targetSampleRate = 48000;
    fs::path workDir = fs::path(req.outWavPath + ".work");
    auto normalized = normalizeWavSet(wavInfos, workDir.string(), targetSampleRate);
    if (!normalized.success) {
        for (const auto& e : normalized.errors) {
            report.errors.push_back("adm-author: " + e);
        }
        report.status = "fail";
        return result;
    }

    report.authoringResample = normalized.files;

    report.hasAuthoringValidation = true;
    uint64_t expectedFrames = normalized.files.front().normalizedFrameCount;
    bool frameMismatch = false;
    for (const auto& n : normalized.files) {
        AuthoringValidationFile vf;
        vf.path = n.normalizedPath;
        vf.frames = n.normalizedFrameCount;
        vf.ok = (n.normalizedFrameCount == expectedFrames);
        if (!vf.ok) frameMismatch = true;
        report.authoringValidation.files.push_back(vf);
    }
    report.authoringValidation.expectedFrames = expectedFrames;

    if (frameMismatch) {
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
        expectedFrames
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