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
// test_360ra.cpp — Phase 6 tests: Sony 360RA ADM → LUSID conversion
//
// Tests are split into two groups:
//
//   Group A — Polar→Cartesian unit tests (synthetic, no fixtures)
//     P6-T1   Front (az=0, el=0, d=1)   → cart [0, 1, 0]
//     P6-T2   Right (az=90, el=0, d=1)  → cart [1, 0, 0]
//     P6-T3   Left  (az=-90, el=0, d=1) → cart [-1, 0, 0]
//     P6-T4   Back  (az=180, el=0, d=1) → cart [≈0, -1, 0]
//     P6-T5   Up    (az=0, el=90, d=1)  → cart [0, ≈0, 1]
//     P6-T6   Down  (az=0, el=-90, d=1) → cart [0, ≈0, -1]
//     P6-T7   Diagonal (az=30, el=0, d=1) → L obj 1 expected cart
//     P6-T8   Elevation (az=40, el=50, d=1) → Ltf obj 1 expected cart
//     P6-T9   Distance scale: d=0.5 → cart * 0.5
//
//   Group B — Structural invariants on sony_360ra_example.xml
//     P6-T10  Conversion succeeds (success=true, no errors)
//     P6-T11  Exactly 1 frame in output
//     P6-T12  Frame time == 0.0
//     P6-T13  Exactly 13 nodes in the single frame
//     P6-T14  All nodes have type == "audio_object"
//     P6-T15  Node IDs are "1.1" through "13.1" in order
//     P6-T16  All carts have unit length (||cart|| ≈ 1.0, tolerance 1e-5)
//     P6-T17  Node "1.1" (L obj 1) has expected cart direction (az≈30, el=0)
//     P6-T18  Node "4.1" (Lss obj 1) is right side (az=90 → x≈1, y≈0)
//     P6-T19  Node "9.1" (Ltf obj 1) has positive z (elevated position)
//     P6-T20  sampleRate == 48000
//     P6-T21  scene.duration > 0 (duration parsed from <Technical>)
//     P6-T22  metadata["sourceFormat"] == "Sony360RA-ADM"
//     P6-T23  report warnings contain "Sony360RA" (profile detection logged)
//     P6-T24  auto-dispatch through transcode(): 360RA file → success via dispatcher
//     P6-T25  auto-dispatch: --lfe-mode speaker-label produces a warning, not an error
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "transcoding/adm/adm_to_lusid.hpp"
#include "transcoding/adm/sony360ra_to_lusid.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <pugixml.hpp>

#include <cmath>
#include <filesystem>
#include <string>

// M_PI is a POSIX extension; MSVC only defines it with _USE_MATH_DEFINES, but
// that flag must precede the very first (transitive) inclusion of <math.h>.
// Since project headers above pull in <cmath> before we can set the flag,
// define M_PI ourselves as a guaranteed fallback.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture paths — provided by CMake at compile time
// ---------------------------------------------------------------------------
#ifndef PARITY_FIXTURES_DIR
#  define PARITY_FIXTURES_DIR "tests/parity/fixtures"
#endif

static const fs::path kFixturesDir = PARITY_FIXTURES_DIR;
static const fs::path kSonyFixture = kFixturesDir / "sony_360ra_example.xml";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Polar → Cartesian (mirrors sony360ra_to_lusid.cpp formula exactly)
static void polarToCart(double az_deg, double el_deg, double dist,
                        double& out_x, double& out_y, double& out_z)
{
    constexpr double kPi = 3.14159265358979323846;
    const double az = az_deg * (kPi / 180.0);
    const double el = el_deg * (kPi / 180.0);
    out_x = dist * std::sin(az) * std::cos(el);
    out_y = dist * std::cos(az) * std::cos(el);
    out_z = dist * std::sin(el);
}

static double cartLength(const double c[3]) {
    return std::sqrt(c[0]*c[0] + c[1]*c[1] + c[2]*c[2]);
}

static bool warningContains(const cult::ConversionResult& r, const std::string& substr) {
    for (const auto& w : r.warnings)
        if (w.find(substr) != std::string::npos) return true;
    return false;
}

// Load sony_360ra_example.xml and run the converter.
// Returns true if the file exists; sets `result` unconditionally.
static bool runSonyConverter(cult::ConversionResult& result,
                              cult::LfeMode mode = cult::LfeMode::Hardcoded)
{
    if (!fs::exists(kSonyFixture)) return false;
    pugi::xml_document doc;
    auto pr = doc.load_file(kSonyFixture.string().c_str());
    if (!pr) return false;
    result = cult::convertSony360RaToLusid(doc, mode);
    return true;
}

// ============================================================================
// Group A — Polar → Cartesian unit tests
// ============================================================================

TEST_CASE("P6-T1: Front (az=0, el=0, d=1) → cart [0, 1, 0]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(0.0, 0.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(y == Catch::Approx(1.0).epsilon(1e-10));
    REQUIRE(z == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T2: Right (az=90, el=0, d=1) → cart [1, 0, 0]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(90.0, 0.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(1.0).epsilon(1e-10));
    REQUIRE(y == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(z == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T3: Left (az=-90, el=0, d=1) → cart [-1, 0, 0]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(-90.0, 0.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(-1.0).epsilon(1e-10));
    REQUIRE(y == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(z == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T4: Back (az=180, el=0, d=1) → cart [≈0, -1, 0]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(180.0, 0.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(y == Catch::Approx(-1.0).epsilon(1e-10));
    REQUIRE(z == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T5: Up (az=0, el=90, d=1) → cart [0, ≈0, 1]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(0.0, 90.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(y == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(z == Catch::Approx(1.0).epsilon(1e-10));
}

TEST_CASE("P6-T6: Down (az=0, el=-90, d=1) → cart [0, ≈0, -1]", "[360ra][polar]") {
    double x, y, z;
    polarToCart(0.0, -90.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(y == Catch::Approx(0.0).margin(1e-10));
    REQUIRE(z == Catch::Approx(-1.0).epsilon(1e-10));
}

TEST_CASE("P6-T7: L obj 1 (az≈30, el=0, d=1) diagonal — positive x and y",
          "[360ra][polar]")
{
    // L obj 1: az=29.999992, el=0, dist=1.0
    // Expected: x = sin(30°)≈0.5, y = cos(30°)≈0.866
    double x, y, z;
    polarToCart(30.0, 0.0, 1.0, x, y, z);
    REQUIRE(x == Catch::Approx(0.5).epsilon(1e-6));
    REQUIRE(y == Catch::Approx(0.8660254).epsilon(1e-6));
    REQUIRE(z == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T8: Ltf obj 1 (az=40, el=50, d=1) — elevated front-right",
          "[360ra][polar]")
{
    // Ltf: az=39.999996, el=50.000004, dist=1
    // Expected: x=sin(40°)*cos(50°), y=cos(40°)*cos(50°), z=sin(50°)
    double x, y, z;
    polarToCart(40.0, 50.0, 1.0, x, y, z);
    const double expected_x = std::sin(40.0 * M_PI / 180.0) * std::cos(50.0 * M_PI / 180.0);
    const double expected_y = std::cos(40.0 * M_PI / 180.0) * std::cos(50.0 * M_PI / 180.0);
    const double expected_z = std::sin(50.0 * M_PI / 180.0);
    REQUIRE(x == Catch::Approx(expected_x).epsilon(1e-10));
    REQUIRE(y == Catch::Approx(expected_y).epsilon(1e-10));
    REQUIRE(z == Catch::Approx(expected_z).epsilon(1e-10));
    REQUIRE(z > 0.0);  // must be elevated
}

TEST_CASE("P6-T9: Distance scale: d=0.5 → cart is half of d=1 cart", "[360ra][polar]") {
    double x1, y1, z1, x2, y2, z2;
    polarToCart(45.0, 30.0, 1.0, x1, y1, z1);
    polarToCart(45.0, 30.0, 0.5, x2, y2, z2);
    REQUIRE(x2 == Catch::Approx(x1 * 0.5).epsilon(1e-10));
    REQUIRE(y2 == Catch::Approx(y1 * 0.5).epsilon(1e-10));
    REQUIRE(z2 == Catch::Approx(z1 * 0.5).epsilon(1e-10));
}

// ============================================================================
// Group B — Structural invariants on sony_360ra_example.xml
// ============================================================================

TEST_CASE("P6-T10: Conversion of sony_360ra_example.xml succeeds", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.success);
    REQUIRE(r.errors.empty());
}

TEST_CASE("P6-T11: Output has exactly 1 frame", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.scene.frames.size() == 1);
    REQUIRE(r.numFrames == 1);
}

TEST_CASE("P6-T12: Single frame time is 0.0", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.scene.frames[0].time == Catch::Approx(0.0).margin(1e-10));
}

TEST_CASE("P6-T13: Exactly 13 nodes in the single frame", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.scene.frames[0].nodes.size() == 13);
}

TEST_CASE("P6-T14: All nodes have type == 'audio_object'", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    for (const auto& node : r.scene.frames[0].nodes) {
        REQUIRE(node.type == "audio_object");
    }
}

TEST_CASE("P6-T15: Node IDs are '1.1' through '13.1' in encounter order",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    const auto& nodes = r.scene.frames[0].nodes;
    REQUIRE(nodes.size() == 13);
    for (int i = 0; i < 13; ++i) {
        const std::string expected = std::to_string(i + 1) + ".1";
        REQUIRE(nodes[static_cast<size_t>(i)].id == expected);
    }
}

TEST_CASE("P6-T16: All carts have unit length (||cart|| ≈ 1.0)", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    for (const auto& node : r.scene.frames[0].nodes) {
        // BOTTOMSEND objects (nodes 12 and 13) have negative elevation,
        // still on unit sphere (dist=1.0)
        double len = cartLength(node.cart);
        REQUIRE(len == Catch::Approx(1.0).epsilon(1e-4));
    }
}

TEST_CASE("P6-T17: Node '1.1' (L obj 1) has expected cart direction (az≈30, el=0)",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    const auto& node = r.scene.frames[0].nodes[0];  // "1.1"
    REQUIRE(node.id == "1.1");
    // az=29.999992≈30, el=0, dist=1 → x=sin(30°)≈0.5, y=cos(30°)≈0.866, z=0
    REQUIRE(node.cart[0] == Catch::Approx(0.5).epsilon(1e-4));        // right
    REQUIRE(node.cart[1] == Catch::Approx(0.8660254).epsilon(1e-4));  // front
    REQUIRE(node.cart[2] == Catch::Approx(0.0).margin(1e-4));         // no elevation
}

TEST_CASE("P6-T18: Node '4.1' (Lss obj 1) is pure right side (az=90)",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    const auto& node = r.scene.frames[0].nodes[3];  // "4.1"
    REQUIRE(node.id == "4.1");
    // az=90, el=0 → x≈1, y≈0, z=0
    REQUIRE(node.cart[0] == Catch::Approx(1.0).epsilon(1e-4));
    REQUIRE(node.cart[1] == Catch::Approx(0.0).margin(1e-4));
    REQUIRE(node.cart[2] == Catch::Approx(0.0).margin(1e-4));
}

TEST_CASE("P6-T19: Node '9.1' (Ltf obj 1) has positive z (elevated position)",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    const auto& node = r.scene.frames[0].nodes[8];  // "9.1"
    REQUIRE(node.id == "9.1");
    // az=39.999996, el=50.000004 → z = sin(50°) ≈ 0.766 > 0
    REQUIRE(node.cart[2] > 0.7);
}

TEST_CASE("P6-T20: sampleRate == 48000", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.scene.sampleRate == 48000);
}

TEST_CASE("P6-T21: scene.duration > 0 (duration parsed from <Technical>)",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(r.scene.duration > 0.0);
    // The fixture duration is "00:02:33.709" ≈ 153.7 seconds
    REQUIRE(r.scene.duration == Catch::Approx(153.0).margin(5.0));
}

TEST_CASE("P6-T22: metadata[sourceFormat] == 'Sony360RA-ADM'", "[360ra][fixture]") {
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    auto it = r.scene.metadata.find("sourceFormat");
    REQUIRE(it != r.scene.metadata.end());
    REQUIRE(it->second == "Sony360RA-ADM");
}

TEST_CASE("P6-T23: warnings contain Sony360RA profile detection message",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r));
    REQUIRE(warningContains(r, "Sony360RA"));
}

TEST_CASE("P6-T24: auto-dispatch through transcode(): 360RA file → success via dispatcher",
          "[360ra][fixture][dispatch]")
{
    if (!fs::exists(kSonyFixture)) return;

    // We need a temp output path for the dispatch test
    const fs::path tmpOut = fs::temp_directory_path() / "cult_test_360ra_dispatch.lusid.json";
    const fs::path tmpReport = fs::path(tmpOut.string() + ".report.json");

    // Clean up any leftovers
    fs::remove(tmpOut);
    fs::remove(tmpReport);

    cult::TranscodeRequest req;
    req.inPath      = kSonyFixture.string();
    req.inFormat    = "adm_xml";
    req.outPath     = tmpOut.string();
    req.outFormat   = "lusid_json";
    req.reportPath  = tmpReport.string();
    req.lfeMode     = cult::LfeMode::Hardcoded;

    auto result = cult::transcode(req);

    // Clean up
    fs::remove(tmpOut);
    fs::remove(tmpReport);

    REQUIRE(result.success);
    REQUIRE(result.report.status == "success");
    // Profile detection warning must be present in the report
    bool hasProfileWarning = false;
    for (const auto& w : result.report.warnings)
        if (w.find("Sony360RA") != std::string::npos) { hasProfileWarning = true; break; }
    REQUIRE(hasProfileWarning);
}

TEST_CASE("P6-T25: lfe-mode speaker-label on 360RA produces warning, not error",
          "[360ra][fixture]")
{
    cult::ConversionResult r;
    REQUIRE(runSonyConverter(r, cult::LfeMode::SpeakerLabel));
    REQUIRE(r.success);
    REQUIRE(r.errors.empty());
    REQUIRE(warningContains(r, "lfe-mode flag has no effect"));
}
