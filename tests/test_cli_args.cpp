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
// test_cli_args.cpp — Unit tests for transcode() arg validation
//
// Tests the transcode() function in transcoder.cpp via TranscodeRequest,
// verifying that:
//   - Missing input file produces status="fail" + error in report.
//   - Unknown --in-format produces status="fail".
//   - Unknown --out-format produces status="fail".
//   - Empty XML input produces status="fail" (parse error).
//   - report.args mirrors the request values exactly (even on failure).
//   - summary.timeUnit is always "seconds" on success.
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture helper — create a temp file and remove it on scope exit
// ---------------------------------------------------------------------------
struct TempFile {
    fs::path path;
    explicit TempFile(const std::string& suffix = ".xml") {
        path = fs::temp_directory_path() / ("cult_test_" + std::to_string(
            std::hash<std::string>{}(suffix + __FILE__)) + suffix);
        std::ofstream f(path); // create empty file
    }
    ~TempFile() { fs::remove(path); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool containsError(const cult::Report& r, const std::string& substr) {
    for (const auto& e : r.errors)
        if (e.find(substr) != std::string::npos) return true;
    return false;
}

static bool containsWarning(const cult::Report& r, const std::string& substr) {
    for (const auto& w : r.warnings)
        if (w.find(substr) != std::string::npos) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("transcode: missing input file returns fail", "[cli][validation]") {
    cult::TranscodeRequest req;
    req.inPath    = "/nonexistent/path/file.xml";
    req.inFormat  = "adm_xml";
    req.outPath   = "/tmp/out.lusid.json";
    req.outFormat = "lusid_json";
    req.reportPath = "/tmp/out.lusid.json.report.json";

    auto result = cult::transcode(req);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "not found"));
}

TEST_CASE("transcode: unknown in-format returns fail", "[cli][validation]") {
    cult::TranscodeRequest req;
    req.inPath    = "/some/file.wav";
    req.inFormat  = "mpeg_h";      // not yet supported
    req.outPath   = "/tmp/out.lusid.json";
    req.outFormat = "lusid_json";
    req.reportPath = "/tmp/out.lusid.json.report.json";

    auto result = cult::transcode(req);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "in-format"));
}

TEST_CASE("transcode: unknown out-format returns fail", "[cli][validation]") {
    cult::TranscodeRequest req;
    req.inPath    = "/some/file.xml";
    req.inFormat  = "adm_xml";
    req.outPath   = "/tmp/out.iamf";
    req.outFormat = "iamf";        // not yet supported
    req.reportPath = "/tmp/out.iamf.report.json";

    auto result = cult::transcode(req);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE(containsError(result.report, "out-format"));
}

TEST_CASE("transcode: empty out path returns fail", "[cli][validation]") {
    TempFile tmp;
    cult::TranscodeRequest req;
    req.inPath    = tmp.path.string();
    req.inFormat  = "adm_xml";
    req.outPath   = "";            // intentionally empty
    req.outFormat = "lusid_json";
    req.reportPath = "/tmp/out.report.json";

    auto result = cult::transcode(req);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
}

TEST_CASE("transcode: empty XML input returns fail (parse error)",
          "[cli][validation]") {
    TempFile tmp(".xml");
    cult::TranscodeRequest req;
    req.inPath     = tmp.path.string();
    req.inFormat   = "adm_xml";
    req.outPath    = "/tmp/out.lusid.json";
    req.outFormat  = "lusid_json";
    req.reportPath = "/tmp/out.lusid.json.report.json";

    auto result = cult::transcode(req);

    // Empty XML cannot be parsed — conversion must fail
    REQUIRE_FALSE(result.success);
    REQUIRE(result.report.status == "fail");
    REQUIRE((containsError(result.report, "parse") || containsError(result.report, "XML")));
}

TEST_CASE("transcode: report.args mirrors request values", "[cli][report]") {
    TempFile tmp(".xml");
    cult::TranscodeRequest req;
    req.inPath       = tmp.path.string();
    req.inFormat     = "adm_xml";
    req.outPath      = "/tmp/scene.lusid.json";
    req.outFormat    = "lusid_json";
    req.reportPath   = "/tmp/scene.lusid.json.report.json";
    req.stdoutReport = true;

    auto result = cult::transcode(req);

    // Args must be populated regardless of success/failure
    const auto& a = result.report.args;
    REQUIRE(a.inPath       == req.inPath);
    REQUIRE(a.inFormat     == req.inFormat);
    REQUIRE(a.outPath      == req.outPath);
    REQUIRE(a.outFormat    == req.outFormat);
    REQUIRE(a.reportPath   == req.reportPath);
    REQUIRE(a.stdoutReport == req.stdoutReport);
}

TEST_CASE("transcode: summary.timeUnit is always 'seconds'", "[cli][summary]") {
    TempFile tmp(".xml");
    cult::TranscodeRequest req;
    req.inPath    = tmp.path.string();
    req.inFormat  = "adm_xml";
    req.outPath   = "/tmp/out.lusid.json";
    req.outFormat = "lusid_json";
    req.reportPath = "/tmp/out.lusid.json.report.json";

    auto result = cult::transcode(req);

    // timeUnit must be "seconds" regardless of success/failure — pinned by AGENTS §4
    REQUIRE(result.report.summary.timeUnit == "seconds");
}
