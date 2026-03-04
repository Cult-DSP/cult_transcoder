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
// tests/parity/run_parity.cpp — Phase 2 placeholder
//
// Parity tests compare cult-transcoder's LUSID output against known-good
// reference fixtures.  Phase 1 has no real ADM→LUSID conversion, so this
// file is intentionally empty except for the boilerplate TEST_CASE stub.
//
// TODO (Phase 2): For each fixture in tests/parity/fixtures/:
//   1. Run transcode(inPath=fixture, inFormat="adm_xml",
//                    outFormat="lusid_json", outPath=tmpOut)
//   2. Load tmpOut and reference JSON
//   3. Compare all LUSID §3 fields with acceptable tolerance on float values
// ---------------------------------------------------------------------------

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parity: placeholder — no fixtures yet", "[parity][.][!skip]") {
    // This test is intentionally skipped in Phase 1 (tagged with [.]).
    // Remove [.][!skip] tags when Phase 2 parity fixtures are added.
    SUCCEED("Phase 1 parity stub — no ADM fixtures yet.");
}
