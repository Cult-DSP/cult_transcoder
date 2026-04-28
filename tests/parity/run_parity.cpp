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
// tests/parity/run_parity.cpp — Phase 2 parity tests
//
// These tests compare cult-transcoder's LUSID output against a known-good
// reference fixture, preserving the original Python-oracle encounter order
// while using the current LUSID Scene v1.0 version field.
//
// The reference was generated via:
//   from LUSID.src.xml_etree_parser import parse_adm_xml_to_lusid_scene
//   scene = parse_adm_xml_to_lusid_scene('processedData/currentMetaData.xml',
//                                         contains_audio=None)
//   json.dump(scene.to_dict(), f, indent=2)
//
// Fixtures:
//   tests/parity/fixtures/test_input.xml            — ADM XML test file
//   tests/parity/fixtures/reference_scene.lusid.json — Python oracle output
// ---------------------------------------------------------------------------

#include <catch2/catch_test_macros.hpp>
#include "transcoding/adm/adm_to_lusid.hpp"

#include <fstream>
#include <sstream>
#include <string>

#ifndef PARITY_FIXTURES_DIR
    #error "PARITY_FIXTURES_DIR must be defined by CMake"
#endif

namespace {

std::string fixturesDir() {
    return PARITY_FIXTURES_DIR;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Count occurrences of a substring
int countSubstring(const std::string& str, const std::string& sub) {
    int count = 0;
    size_t pos = 0;
    while ((pos = str.find(sub, pos)) != std::string::npos) {
        ++count;
        pos += sub.length();
    }
    return count;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test: conversion succeeds on test XML
// ---------------------------------------------------------------------------
TEST_CASE("parity: conversion succeeds on test XML", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);

    REQUIRE(result.success);
    CHECK(result.errors.empty());
    CHECK(result.scene.version == "1.0");
    CHECK(result.scene.timeUnit == "seconds");
}

// ---------------------------------------------------------------------------
// Test: frame count matches Python oracle
// ---------------------------------------------------------------------------
TEST_CASE("parity: frame count matches Python oracle", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);

    // Python oracle produces 2823 frames
    CHECK(result.numFrames == 2823);
    CHECK(result.scene.frames.size() == 2823);
}

// ---------------------------------------------------------------------------
// Test: node type counts match Python oracle
// ---------------------------------------------------------------------------
TEST_CASE("parity: node type counts match Python oracle", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);

    // Python oracle: 9 direct_speaker, 1 LFE, 7094 audio_object
    CHECK(result.countsByNodeType["direct_speaker"] == 9);
    CHECK(result.countsByNodeType["LFE"] == 1);
    CHECK(result.countsByNodeType["audio_object"] == 7094);
}

// ---------------------------------------------------------------------------
// Test: frame 0 structure — all 48 nodes present
// ---------------------------------------------------------------------------
TEST_CASE("parity: frame 0 has 48 nodes (10 DS + 38 objects)", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);
    REQUIRE_FALSE(result.scene.frames.empty());

    auto& frame0 = result.scene.frames[0];
    CHECK(frame0.time == 0.0);
    CHECK(frame0.nodes.size() == 48);
}

// ---------------------------------------------------------------------------
// Test: first DirectSpeaker node matches Python oracle
// ---------------------------------------------------------------------------
TEST_CASE("parity: first DirectSpeaker node matches", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);
    REQUIRE(result.scene.frames.size() > 0);
    REQUIRE(result.scene.frames[0].nodes.size() > 0);

    auto& node = result.scene.frames[0].nodes[0];
    CHECK(node.id == "1.1");
    CHECK(node.type == "direct_speaker");
    CHECK(node.hasCart);
    CHECK(node.cart[0] == -1.0);
    CHECK(node.cart[1] == 1.0);
    CHECK(node.cart[2] == 0.0);
    CHECK(node.speakerLabel == "RC_L");
    CHECK(node.channelID == "AC_00011001");
}

// ---------------------------------------------------------------------------
// Test: LFE node is at position 3 (0-indexed) with no cart
// ---------------------------------------------------------------------------
TEST_CASE("parity: LFE node at index 3", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);
    REQUIRE(result.scene.frames[0].nodes.size() > 3);

    auto& lfe = result.scene.frames[0].nodes[3];
    CHECK(lfe.id == "4.1");
    CHECK(lfe.type == "LFE");
    CHECK_FALSE(lfe.hasCart);
}

// ---------------------------------------------------------------------------
// Test: sampleRate and metadata match
// ---------------------------------------------------------------------------
TEST_CASE("parity: sampleRate and metadata", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);

    // No <Technical> section → defaults to 48000
    CHECK(result.scene.sampleRate == 48000);

    // Duration should be -1.0 (not set, no Technical section)
    CHECK(result.scene.duration < 0.0);

    // metadata: only sourceFormat since no Technical section
    CHECK(result.scene.metadata.at("sourceFormat") == "ADM");
    CHECK(result.scene.metadata.size() == 1);
}

// ---------------------------------------------------------------------------
// Test: JSON output matches Python oracle byte-for-byte
// ---------------------------------------------------------------------------
TEST_CASE("parity: JSON output matches Python oracle", "[parity]") {
    std::string inputXml = fixturesDir() + "/test_input.xml";
    auto result = cult::convertAdmToLusid(inputXml);
    REQUIRE(result.success);

    std::string cppJson = cult::lusidSceneToJson(result.scene);
    std::string refJson = readFile(fixturesDir() + "/reference_scene.lusid.json");

    REQUIRE_FALSE(refJson.empty());

    // Add trailing newline to C++ output if reference has one
    // (Python json.dump does not add trailing newline, but our ref file
    //  might or might not depending on the shell)
    // Strip trailing whitespace from both for comparison
    while (!cppJson.empty() && (cppJson.back() == '\n' || cppJson.back() == '\r' || cppJson.back() == ' '))
        cppJson.pop_back();
    while (!refJson.empty() && (refJson.back() == '\n' || refJson.back() == '\r' || refJson.back() == ' '))
        refJson.pop_back();

    if (cppJson != refJson) {
        // Find first difference for diagnostic
        size_t diffPos = 0;
        size_t minLen = std::min(cppJson.size(), refJson.size());
        for (size_t i = 0; i < minLen; ++i) {
            if (cppJson[i] != refJson[i]) {
                diffPos = i;
                break;
            }
            diffPos = i + 1;
        }

        // Show context around the difference
        size_t contextStart = (diffPos > 40) ? diffPos - 40 : 0;
        size_t contextEnd = std::min(diffPos + 40, minLen);

        INFO("First difference at byte " << diffPos
             << " (C++ len=" << cppJson.size()
             << ", ref len=" << refJson.size() << ")");
        INFO("C++ context: ..." << cppJson.substr(contextStart, contextEnd - contextStart) << "...");
        INFO("Ref context: ..." << refJson.substr(contextStart, contextEnd - contextStart) << "...");

        // Also check structural counts to narrow down the issue
        INFO("C++ frames count marker: " << countSubstring(cppJson, "\"time\":"));
        INFO("Ref frames count marker: " << countSubstring(refJson, "\"time\":"));

        CHECK(cppJson == refJson);
    }
}
