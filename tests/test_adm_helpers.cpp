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

#include "transcoding/adm/admHelper.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

#include <string>
#include <vector>

namespace {

pugi::xml_document loadXml(const char* xml) {
    pugi::xml_document doc;
    const auto result = doc.load_string(xml);
    REQUIRE(result);
    return doc;
}

} // namespace

TEST_CASE("ADM helpers preserve descendant encounter order", "[adm_helpers]") {
    auto doc = loadXml(
        "<root>"
        "  <branch><target id='1'/><other/><target id='2'/></branch>"
        "  <target id='3'/>"
        "</root>");

    const auto nodes = cult::adm_helpers::findAllDescendants(doc.document_element(), "target");
    REQUIRE(nodes.size() == 3);
    CHECK(std::string(nodes[0].attribute("id").value()) == "1");
    CHECK(std::string(nodes[1].attribute("id").value()) == "2");
    CHECK(std::string(nodes[2].attribute("id").value()) == "3");
}

TEST_CASE("ADM helpers find ebuCoreMain through wrapper and fallback", "[adm_helpers]") {
    auto wrapped = loadXml(
        "<conformance_point_document>"
        "  <File>"
        "    <aXML><ebuCoreMain id='wrapped'/></aXML>"
        "  </File>"
        "</conformance_point_document>");
    CHECK(std::string(cult::adm_helpers::findEbuRoot(wrapped).attribute("id").value()) == "wrapped");

    auto fallback = loadXml(
        "<outer><inner><ebuCoreMain id='fallback'/></inner></outer>");
    CHECK(std::string(cult::adm_helpers::findEbuRoot(fallback).attribute("id").value()) == "fallback");
}

TEST_CASE("ADM helpers find audioFormatExtended and Technical nodes across wrappers", "[adm_helpers]") {
    auto wrapped = loadXml(
        "<conformance_point_document>"
        "  <File>"
        "    <Technical id='tech'/>"
        "    <aXML><format><audioFormatExtended id='afe'/></format></aXML>"
        "  </File>"
        "</conformance_point_document>");
    CHECK(std::string(cult::adm_helpers::findAdmRoot(wrapped).attribute("id").value()) == "afe");
    CHECK(std::string(cult::adm_helpers::findTechnicalNode(wrapped).attribute("id").value()) == "tech");

    auto bare = loadXml(
        "<audioFormatExtended id='bare'><meta><Technical id='fallback-tech'/></meta></audioFormatExtended>");
    CHECK(std::string(cult::adm_helpers::findAdmRoot(bare).attribute("id").value()) == "bare");
    CHECK(std::string(cult::adm_helpers::findTechnicalNode(bare).attribute("id").value()) == "fallback-tech");
}

TEST_CASE("ADM helpers parse ADM timecodes to seconds", "[adm_helpers]") {
    CHECK(cult::adm_helpers::parseTimecodeToSeconds("00:00:00.01000") == Catch::Approx(0.01));
    CHECK(cult::adm_helpers::parseTimecodeToSeconds("00:00:00.01024S48000") == Catch::Approx(0.01024));
    CHECK(cult::adm_helpers::parseTimecodeToSeconds("00:00:00.000020833") ==
          Catch::Approx(1.0 / 48000.0).margin(1e-9));
    CHECK(cult::adm_helpers::parseTimecodeToSeconds("01:02:03.50000") == Catch::Approx(3723.5));
    CHECK(cult::adm_helpers::parseTimecodeToSeconds("") == Catch::Approx(0.0));
}
