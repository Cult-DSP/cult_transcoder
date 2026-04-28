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
// test_report.cpp — Unit tests for Report serialisation (Phase 1)
//
// Covers:
//   - Report::toJson() produces valid JSON with all required schema fields
//     (reportVersion, tool, args, status, errors, warnings, summary,
//      lossLedger).
//   - lossLedger is present even when empty.
//   - Status values "success" and "fail" round-trip correctly.
//   - LossLedgerEntry fields are serialised correctly.
// ---------------------------------------------------------------------------

#include "reporting/cult_report.hpp"
#include "cult_version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static std::string readFile(const fs::path& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    std::ostringstream out;
    out << f.rdbuf();
    return out.str();
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("Report toJson contains all required schema fields", "[report][schema]") {
    cult::Report r;
    r.status              = "success";
    r.args.inPath         = "/data/input.xml";
    r.args.inFormat       = "adm_xml";
    r.args.outPath        = "/data/output.json";
    r.args.outFormat      = "lusid_json";
    r.args.reportPath     = "/data/output.json.report.json";
    r.args.stdoutReport   = false;
    r.summary.timeUnit    = "seconds";

    const std::string json = r.toJson();

    // Required top-level keys (AGENTS §7)
    REQUIRE(contains(json, "\"reportVersion\""));
    REQUIRE(contains(json, "\"tool\""));
    REQUIRE(contains(json, "\"args\""));
    REQUIRE(contains(json, "\"status\""));
    REQUIRE(contains(json, "\"errors\""));
    REQUIRE(contains(json, "\"warnings\""));
    REQUIRE(contains(json, "\"summary\""));
    REQUIRE(contains(json, "\"lossLedger\""));

    // tool sub-keys
    REQUIRE(contains(json, "\"name\""));
    REQUIRE(contains(json, "\"version\""));
    REQUIRE(contains(json, "\"gitCommit\""));

    // summary sub-keys
    REQUIRE(contains(json, "\"durationSec\""));
    REQUIRE(contains(json, "\"sampleRate\""));
    REQUIRE(contains(json, "\"timeUnit\""));
    REQUIRE(contains(json, "\"numFrames\""));
    REQUIRE(contains(json, "\"countsByNodeType\""));
}

TEST_CASE("Report schema version is pinned to 0.1", "[report][schema]") {
    cult::Report r;
    r.status = "success";
    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"reportVersion\": \"0.1\""));
}

TEST_CASE("Report tool name matches kToolName", "[report][schema]") {
    cult::Report r;
    r.status = "success";
    const std::string json = r.toJson();
    REQUIRE(contains(json, std::string("\"name\": \"") + cult::kToolName + "\""));
}

TEST_CASE("Report status fail round-trips", "[report][status]") {
    cult::Report r;
    r.status = "fail";
    r.errors.push_back("something went wrong");
    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"status\": \"fail\""));
    REQUIRE(contains(json, "something went wrong"));
}

TEST_CASE("Report status success round-trips", "[report][status]") {
    cult::Report r;
    r.status = "success";
    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"status\": \"success\""));
}

TEST_CASE("lossLedger is present even when empty", "[report][lossLedger]") {
    cult::Report r;
    r.status = "success";
    // Do NOT add any lossLedger entries
    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"lossLedger\": []"));
}

TEST_CASE("lossLedger entry fields are serialised", "[report][lossLedger]") {
    cult::Report r;
    r.status = "success";

    cult::LossLedgerEntry e;
    e.kind    = "truncated";
    e.field   = "nodes[0].position.x";
    e.reason  = "value out of range";
    e.count   = 3;
    e.examples.push_back("1.5");
    e.examples.push_back("-2.0");
    r.lossLedger.push_back(e);

    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"kind\": \"truncated\""));
    REQUIRE(contains(json, "\"field\": \"nodes[0].position.x\""));
    REQUIRE(contains(json, "\"reason\": \"value out of range\""));
    REQUIRE(contains(json, "\"count\": 3"));
    REQUIRE(contains(json, "\"1.5\""));
}

TEST_CASE("Report args are serialised verbatim", "[report][args]") {
    cult::Report r;
    r.status             = "success";
    r.args.inPath        = "/some/path/input.xml";
    r.args.inFormat      = "adm_xml";
    r.args.outPath       = "/some/path/out.lusid.json";
    r.args.outFormat     = "lusid_json";
    r.args.reportPath    = "/some/path/out.lusid.json.report.json";
    r.args.stdoutReport  = true;

    const std::string json = r.toJson();
    REQUIRE(contains(json, "/some/path/input.xml"));
    REQUIRE(contains(json, "adm_xml"));
    REQUIRE(contains(json, "/some/path/out.lusid.json"));
    REQUIRE(contains(json, "lusid_json"));
    REQUIRE(contains(json, "true")); // stdoutReport
}

TEST_CASE("Report summary timeUnit defaults to seconds", "[report][summary]") {
    cult::Report r;
    r.status = "success";
    // timeUnit not explicitly set — must default to "seconds" per §4
    const std::string json = r.toJson();
    REQUIRE(contains(json, "\"timeUnit\": \"seconds\""));
}

TEST_CASE("JSON escaping handles special characters", "[report][escaping]") {
    cult::Report r;
    r.status = "fail";
    r.errors.push_back("path has \"quotes\" and \\backslash");
    const std::string json = r.toJson();
    // Verify the string is present and escaped
    REQUIRE(contains(json, "\\\"quotes\\\""));
    REQUIRE(contains(json, "\\\\backslash"));
}

TEST_CASE("Report writeTo matches toJson output", "[report][io]") {
    const fs::path outPath = fs::temp_directory_path() / "cult_report_write_to.json";
    fs::remove(outPath);

    cult::Report r;
    r.status = "success";
    r.args.inPath = "/tmp/input.xml";
    r.args.outPath = "/tmp/output.json";
    r.summary.timeUnit = "seconds";
    r.warnings.push_back("warning");

    const std::string expected = r.toJson();
    REQUIRE(r.writeTo(outPath.string()));
    REQUIRE(readFile(outPath) == expected);

    std::error_code ec;
    fs::remove(outPath, ec);
}

TEST_CASE("Report writeJson matches toJson output", "[report][io]") {
    cult::Report r;
    r.status = "success";
    r.args.inPath = "/tmp/input.xml";
    r.errors.push_back("none");

    std::ostringstream out;
    r.writeJson(out);
    REQUIRE(out.str() == r.toJson());
}
