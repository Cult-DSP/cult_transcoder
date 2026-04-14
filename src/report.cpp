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
// report.cpp — Implementation of Report serialisation (cult_report.hpp)
//
// Serialises to JSON without any third-party library so Phase 1 has zero
// external dependencies.  Phase 2 may switch to nlohmann/json; the public
// API (cult_report.hpp) will not change.
// ---------------------------------------------------------------------------

#include "cult_report.hpp"
#include "cult_version.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace cult {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Escape a string for JSON embedding.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\t";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string jsonStr(const std::string& s) {
    return "\"" + jsonEscape(s) + "\"";
}

// Indent helper — returns 'count' copies of two spaces.
std::string ind(int count) {
    std::string s;
    for (int i = 0; i < count; ++i) s += "  ";
    return s;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Report::toJson()
// ---------------------------------------------------------------------------
std::string Report::toJson() const {
    std::ostringstream o;

    o << "{\n";

    // reportVersion
    o << ind(1) << "\"reportVersion\": " << jsonStr(kReportSchemaVersion) << ",\n";

    // tool
    o << ind(1) << "\"tool\": {\n";
    o << ind(2) << "\"name\": "      << jsonStr(kToolName)      << ",\n";
    o << ind(2) << "\"version\": "   << jsonStr(kVersionString) << ",\n";
    o << ind(2) << "\"gitCommit\": " << jsonStr(gitCommit())    << "\n";
    o << ind(1) << "},\n";

    // args
    o << ind(1) << "\"args\": {\n";
    o << ind(2) << "\"in\": "            << jsonStr(args.inPath)    << ",\n";
    o << ind(2) << "\"inFormat\": "      << jsonStr(args.inFormat)  << ",\n";
    o << ind(2) << "\"out\": "           << jsonStr(args.outPath)   << ",\n";
    o << ind(2) << "\"outFormat\": "     << jsonStr(args.outFormat) << ",\n";
    o << ind(2) << "\"report\": "        << jsonStr(args.reportPath) << ",\n";
    o << ind(2) << "\"stdoutReport\": "  << (args.stdoutReport ? "true" : "false") << ",\n";
    o << ind(2) << "\"lfeMode\": "       << jsonStr(args.lfeMode)   << "\n";
    o << ind(1) << "},\n";

    // status
    o << ind(1) << "\"status\": " << jsonStr(status) << ",\n";

    // errors
    o << ind(1) << "\"errors\": [";
    for (size_t i = 0; i < errors.size(); ++i) {
        o << (i == 0 ? "\n" : "") << ind(2) << jsonStr(errors[i]);
        if (i + 1 < errors.size()) o << ",";
        o << "\n";
    }
    o << (errors.empty() ? "" : ind(1)) << "],\n";

    // warnings
    o << ind(1) << "\"warnings\": [";
    for (size_t i = 0; i < warnings.size(); ++i) {
        o << (i == 0 ? "\n" : "") << ind(2) << jsonStr(warnings[i]);
        if (i + 1 < warnings.size()) o << ",";
        o << "\n";
    }
    o << (warnings.empty() ? "" : ind(1)) << "],\n";

    // summary
    o << ind(1) << "\"summary\": {\n";
    o << ind(2) << "\"durationSec\": "  << summary.durationSec  << ",\n";
    o << ind(2) << "\"sampleRate\": "   << summary.sampleRate   << ",\n";
    o << ind(2) << "\"timeUnit\": "     << jsonStr(summary.timeUnit) << ",\n";
    o << ind(2) << "\"numFrames\": "    << summary.numFrames    << ",\n";
    o << ind(2) << "\"countsByNodeType\": {";
    if (summary.countsByNodeType.empty()) {
        o << "}";
    } else {
        o << "\n";
        size_t idx = 0;
        for (const auto& [k, v] : summary.countsByNodeType) {
            o << ind(3) << jsonStr(k) << ": " << v;
            if (++idx < summary.countsByNodeType.size()) o << ",";
            o << "\n";
        }
        o << ind(2) << "}";
    }
    o << "\n" << ind(1) << "},\n";

    // authoringResample (optional)
    if (!authoringResample.empty()) {
        o << ind(1) << "\"authoringResample\": [";
        for (size_t i = 0; i < authoringResample.size(); ++i) {
            const auto& e = authoringResample[i];
            o << "\n" << ind(2) << "{\n";
            o << ind(3) << "\"sourcePath\": " << jsonStr(e.sourcePath) << ",\n";
            o << ind(3) << "\"normalizedPath\": " << jsonStr(e.normalizedPath) << ",\n";
            o << ind(3) << "\"sourceSampleRate\": " << e.sourceSampleRate << ",\n";
            o << ind(3) << "\"targetSampleRate\": " << e.targetSampleRate << ",\n";
            o << ind(3) << "\"sourceFrameCount\": " << e.sourceFrameCount << ",\n";
            o << ind(3) << "\"normalizedFrameCount\": " << e.normalizedFrameCount << ",\n";
            o << ind(3) << "\"resampled\": " << (e.resampled ? "true" : "false") << "\n";
            o << ind(2) << "}";
            if (i + 1 < authoringResample.size()) o << ",";
        }
        o << "\n" << ind(1) << "],\n";
    }

    // authoringValidation (optional)
    if (hasAuthoringValidation) {
        o << ind(1) << "\"authoringValidation\": {\n";
        o << ind(2) << "\"expectedFrames\": " << authoringValidation.expectedFrames << ",\n";
        o << ind(2) << "\"files\": [";
        for (size_t i = 0; i < authoringValidation.files.size(); ++i) {
            const auto& f = authoringValidation.files[i];
            o << "\n" << ind(3) << "{\n";
            o << ind(4) << "\"path\": " << jsonStr(f.path) << ",\n";
            o << ind(4) << "\"frames\": " << f.frames << ",\n";
            o << ind(4) << "\"ok\": " << (f.ok ? "true" : "false") << "\n";
            o << ind(3) << "}";
            if (i + 1 < authoringValidation.files.size()) o << ",";
        }
        o << (authoringValidation.files.empty() ? "" : "\n" + ind(2)) << "]\n";
        o << ind(1) << "},\n";
    }

    // lossLedger — required even when empty (§7)
    o << ind(1) << "\"lossLedger\": [";
    for (size_t i = 0; i < lossLedger.size(); ++i) {
        const auto& e = lossLedger[i];
        o << "\n" << ind(2) << "{\n";
        o << ind(3) << "\"kind\": "   << jsonStr(e.kind)   << ",\n";
        o << ind(3) << "\"field\": "  << jsonStr(e.field)  << ",\n";
        o << ind(3) << "\"reason\": " << jsonStr(e.reason) << ",\n";
        o << ind(3) << "\"count\": "  << e.count           << ",\n";
        o << ind(3) << "\"examples\": [";
        for (size_t j = 0; j < e.examples.size(); ++j) {
            o << jsonStr(e.examples[j]);
            if (j + 1 < e.examples.size()) o << ", ";
        }
        o << "]\n";
        o << ind(2) << "}";
        if (i + 1 < lossLedger.size()) o << ",";
    }
    o << (lossLedger.empty() ? "" : "\n" + ind(1)) << "]\n";

    o << "}\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Report::writeTo()
// ---------------------------------------------------------------------------
bool Report::writeTo(const std::string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "[cult-transcoder] ERROR: cannot write report to: " << path << "\n";
        return false;
    }
    f << toJson();
    return f.good();
}

// ---------------------------------------------------------------------------
// Report::printToStdout()
// ---------------------------------------------------------------------------
void Report::printToStdout() const {
    std::cout << toJson();
}

} // namespace cult
