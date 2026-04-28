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
// test_lfe_mode.cpp — Phase 4 tests: LFE mode flag + profile detection
//
// Tests:
//   P4-T1  --lfe-mode hardcoded explicit == default behaviour (parity fixture)
//   P4-T2  --lfe-mode speaker-label promotes speakerLabel="LFE" node to LFE type
//   P4-T3  --lfe-mode speaker-label does NOT promote ch3 without speakerLabel
//   P4-T4  --lfe-mode hardcoded does NOT promote ch3 (only ch4) in fixture
//   P4-T5  report.args.lfeMode == "hardcoded" when flag omitted (default)
//   P4-T6  report.args.lfeMode == "speaker-label" when flag set
//   P4-T7  resolveAdmProfile: Sony 360RA fixture → Sony360RA detected
//   P4-T8  resolveAdmProfile: Generic EBU fixture → Unknown (no signal)
//   P4-T9  resolveAdmProfile: detection warning appears in warnings[]
//   P4-T10 resolveAdmProfile: DolbyAtmos detected via audioProgrammeName
//   P4-T11 resolveAdmProfile: DolbyAtmos detected via audioPackFormatName
//   P4-T12 resolveAdmProfile: Sony360RA detected via audioPackFormatName
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "transcoding/adm/adm_to_lusid.hpp"
#include "transcoding/adm/adm_profile_resolver.hpp"

#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Fixture paths — provided by CMake at compile time
// ---------------------------------------------------------------------------
#ifndef PARITY_FIXTURES_DIR
#  define PARITY_FIXTURES_DIR "tests/parity/fixtures"
#endif

static const fs::path kFixturesDir   = PARITY_FIXTURES_DIR;
static const fs::path kLfeFixture    = kFixturesDir / "lfe_speaker_label_fixture.xml";
static const fs::path kSonyFixture   = kFixturesDir / "sony_360ra_example.xml";
static const fs::path kParityFixture = kFixturesDir / "test_input.xml";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool hasNodeOfType(const cult::ConversionResult& cr,
                           const std::string& type) {
    for (const auto& frame : cr.scene.frames)
        for (const auto& node : frame.nodes)
            if (node.type == type) return true;
    return false;
}

static std::string nodeTypeAt(const cult::ConversionResult& cr,
                               double time, size_t index) {
    for (const auto& frame : cr.scene.frames) {
        if (std::abs(frame.time - time) < 1e-9) {
            if (index < frame.nodes.size())
                return frame.nodes[index].type;
        }
    }
    return "";
}

// ---------------------------------------------------------------------------
// P4-T1: --lfe-mode hardcoded explicit == default (parity fixture)
// The parity fixture has 13 DirectSpeakers; channel 4 (1-based) must be LFE.
// Passing LfeMode::Hardcoded explicitly must produce identical output to
// omitting the parameter.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T1: lfe-mode hardcoded explicit equals default", "[lfe_mode]") {
    REQUIRE(fs::exists(kParityFixture));

    const auto defaultResult  = cult::convertAdmToLusid(kParityFixture.string());
    const auto explicitResult = cult::convertAdmToLusid(
        kParityFixture.string(), cult::LfeMode::Hardcoded);

    REQUIRE(defaultResult.success);
    REQUIRE(explicitResult.success);

    // Node counts must match
    REQUIRE(defaultResult.countsByNodeType == explicitResult.countsByNodeType);

    // Frame count must match
    REQUIRE(defaultResult.numFrames == explicitResult.numFrames);

    // LFE node must exist in both
    REQUIRE(hasNodeOfType(defaultResult,  "LFE"));
    REQUIRE(hasNodeOfType(explicitResult, "LFE"));
}

// ---------------------------------------------------------------------------
// P4-T2: --lfe-mode speaker-label promotes speakerLabel="LFE" node
// The lfe_speaker_label_fixture.xml has LFE at channel 3 (not 4), with
// <speakerLabel>LFE</speakerLabel>. Only speaker-label mode should promote it.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T2: lfe-mode speaker-label promotes speakerLabel=LFE node", "[lfe_mode]") {
    REQUIRE(fs::exists(kLfeFixture));

    const auto result = cult::convertAdmToLusid(
        kLfeFixture.string(), cult::LfeMode::SpeakerLabel);

    REQUIRE(result.success);
    REQUIRE(hasNodeOfType(result, "LFE"));

    // Channel 3 (index 2 in the t=0 frame) must be typed LFE
    CHECK(nodeTypeAt(result, 0.0, 2) == "LFE");
}

// ---------------------------------------------------------------------------
// P4-T3: --lfe-mode hardcoded does NOT promote ch3 in the LFE fixture
// Channel 3 has speakerLabel="LFE" but hardcoded mode ignores speakerLabel.
// Channel 4 does not exist in this 3-DirectSpeaker fixture → no LFE node.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T3: lfe-mode hardcoded does not promote ch3 (fixture has no ch4)", "[lfe_mode]") {
    REQUIRE(fs::exists(kLfeFixture));

    const auto result = cult::convertAdmToLusid(
        kLfeFixture.string(), cult::LfeMode::Hardcoded);

    REQUIRE(result.success);

    // Channel 3 must be direct_speaker, NOT LFE
    CHECK(nodeTypeAt(result, 0.0, 2) == "direct_speaker");

    // No LFE node at all (only 3 DirectSpeakers, ch4 doesn't exist)
    CHECK_FALSE(hasNodeOfType(result, "LFE"));
}

// ---------------------------------------------------------------------------
// P4-T4: default (no lfeMode arg) == hardcoded on LFE fixture
// Ensures API default matches hardcoded explicitly.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T4: default lfe-mode equals hardcoded on LFE fixture", "[lfe_mode]") {
    REQUIRE(fs::exists(kLfeFixture));

    const auto defaultResult  = cult::convertAdmToLusid(kLfeFixture.string());
    const auto hardcodedResult = cult::convertAdmToLusid(
        kLfeFixture.string(), cult::LfeMode::Hardcoded);

    REQUIRE(defaultResult.success);
    REQUIRE(hardcodedResult.success);
    CHECK(defaultResult.countsByNodeType == hardcodedResult.countsByNodeType);
}

// ---------------------------------------------------------------------------
// P4-T5: report.args.lfeMode == "hardcoded" when flag omitted (default)
// Exercises the full transcode() path to verify report serialization.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T5: report.args.lfeMode is hardcoded when using default", "[lfe_mode][report]") {
    REQUIRE(fs::exists(kParityFixture));

    // Write output to a temp file
    const fs::path outPath = fs::temp_directory_path() / "cult_p4t5_out.lusid.json";
    fs::remove(outPath);

    cult::TranscodeRequest req;
    req.inPath    = kParityFixture.string();
    req.inFormat  = "adm_xml";
    req.outPath   = outPath.string();
    req.outFormat = "lusid_json";
    req.reportPath = outPath.string() + ".report.json";
    // lfeMode not set — uses default LfeMode::Hardcoded

    const auto result = cult::transcode(req);

    REQUIRE(result.success);
    CHECK(result.report.args.lfeMode == "hardcoded");

    fs::remove(outPath);
    fs::remove(req.reportPath);
}

// ---------------------------------------------------------------------------
// P4-T6: report.args.lfeMode == "speaker-label" when flag set
// ---------------------------------------------------------------------------
TEST_CASE("P4-T6: report.args.lfeMode is speaker-label when set", "[lfe_mode][report]") {
    REQUIRE(fs::exists(kLfeFixture));

    const fs::path outPath = fs::temp_directory_path() / "cult_p4t6_out.lusid.json";
    fs::remove(outPath);

    cult::TranscodeRequest req;
    req.inPath     = kLfeFixture.string();
    req.inFormat   = "adm_xml";
    req.outPath    = outPath.string();
    req.outFormat  = "lusid_json";
    req.reportPath = outPath.string() + ".report.json";
    req.lfeMode    = cult::LfeMode::SpeakerLabel;

    const auto result = cult::transcode(req);

    REQUIRE(result.success);
    CHECK(result.report.args.lfeMode == "speaker-label");

    fs::remove(outPath);
    fs::remove(req.reportPath);
}

// ---------------------------------------------------------------------------
// P4-T7: resolveAdmProfile — Sony 360RA fixture → Sony360RA detected
// The fixture's audioProgrammeName is "Gem_OM_360RA_3" which contains "360RA".
// ---------------------------------------------------------------------------
TEST_CASE("P4-T7: resolveAdmProfile detects Sony360RA from audioProgrammeName", "[profile]") {
    REQUIRE(fs::exists(kSonyFixture));

    pugi::xml_document doc;
    REQUIRE(doc.load_file(kSonyFixture.string().c_str()));

    const auto profile = cult::resolveAdmProfile(doc);

    CHECK(profile.profile == cult::AdmProfile::Sony360RA);
    CHECK(profile.detectedFrom.find("audioProgrammeName") != std::string::npos);
    CHECK(profile.detectedFrom.find("360RA") != std::string::npos);
    REQUIRE_FALSE(profile.warnings.empty());
}

// ---------------------------------------------------------------------------
// P4-T8: resolveAdmProfile — generic EBU fixture → Unknown (no signal)
// test_input.xml is a standard Dolby Atmos ADM; but let's confirm Unknown
// only fires when there is truly no signal.  We use a minimal inline doc.
// ---------------------------------------------------------------------------
TEST_CASE("P4-T8: resolveAdmProfile returns Unknown for generic EBU ADM", "[profile]") {
    pugi::xml_document doc;
    doc.load_string(R"(
        <ebuCoreMain>
          <coreMetadata>
            <format>
              <audioFormatExtended>
                <audioProgramme audioProgrammeName="Generic Mix"/>
                <audioPackFormat audioPackFormatName="5.1_Mix"/>
              </audioFormatExtended>
            </format>
          </coreMetadata>
        </ebuCoreMain>
    )");

    const auto profile = cult::resolveAdmProfile(doc);

    CHECK(profile.profile == cult::AdmProfile::Unknown);
    CHECK(profile.warnings.empty()); // Unknown produces no warning
}

// ---------------------------------------------------------------------------
// P4-T9: resolveAdmProfile — detection warning appears in warnings[] always
// ---------------------------------------------------------------------------
TEST_CASE("P4-T9: resolveAdmProfile always emits a warning when profile is detected", "[profile]") {
    REQUIRE(fs::exists(kSonyFixture));

    pugi::xml_document doc;
    REQUIRE(doc.load_file(kSonyFixture.string().c_str()));

    const auto profile = cult::resolveAdmProfile(doc);

    REQUIRE(profile.profile == cult::AdmProfile::Sony360RA);
    REQUIRE_FALSE(profile.warnings.empty());
    CHECK(profile.warnings[0].find("Sony360RA") != std::string::npos);
}

// ---------------------------------------------------------------------------
// P4-T10: resolveAdmProfile — DolbyAtmos detected via audioProgrammeName
// ---------------------------------------------------------------------------
TEST_CASE("P4-T10: resolveAdmProfile detects DolbyAtmos via audioProgrammeName", "[profile]") {
    pugi::xml_document doc;
    doc.load_string(R"(
        <ebuCoreMain>
          <audioFormatExtended>
            <audioProgramme audioProgrammeName="Atmos_Master_7.1.4"/>
          </audioFormatExtended>
        </ebuCoreMain>
    )");

    const auto profile = cult::resolveAdmProfile(doc);

    CHECK(profile.profile == cult::AdmProfile::DolbyAtmos);
    CHECK(profile.detectedFrom.find("audioProgrammeName") != std::string::npos);
    CHECK_FALSE(profile.warnings.empty());
}

// ---------------------------------------------------------------------------
// P4-T11: resolveAdmProfile — DolbyAtmos detected via audioPackFormatName
// (audioProgrammeName has no signal, so pass 2 fires)
// ---------------------------------------------------------------------------
TEST_CASE("P4-T11: resolveAdmProfile detects DolbyAtmos via audioPackFormatName", "[profile]") {
    pugi::xml_document doc;
    doc.load_string(R"(
        <ebuCoreMain>
          <audioFormatExtended>
            <audioProgramme audioProgrammeName="SomeGenericMix"/>
            <audioPackFormat audioPackFormatName="Atmos_BedPack"/>
          </audioFormatExtended>
        </ebuCoreMain>
    )");

    const auto profile = cult::resolveAdmProfile(doc);

    CHECK(profile.profile == cult::AdmProfile::DolbyAtmos);
    CHECK(profile.detectedFrom.find("audioPackFormatName") != std::string::npos);
}

// ---------------------------------------------------------------------------
// P4-T12: resolveAdmProfile — Sony360RA detected via audioPackFormatName
// (audioProgrammeName has no signal, so pass 2 fires)
// ---------------------------------------------------------------------------
TEST_CASE("P4-T12: resolveAdmProfile detects Sony360RA via audioPackFormatName", "[profile]") {
    pugi::xml_document doc;
    doc.load_string(R"(
        <ebuCoreMain>
          <audioFormatExtended>
            <audioProgramme audioProgrammeName="StereoMix"/>
            <audioPackFormat audioPackFormatName="360RA_ObjectPack"/>
          </audioFormatExtended>
        </ebuCoreMain>
    )");

    const auto profile = cult::resolveAdmProfile(doc);

    CHECK(profile.profile == cult::AdmProfile::Sony360RA);
    CHECK(profile.detectedFrom.find("audioPackFormatName") != std::string::npos);
}
