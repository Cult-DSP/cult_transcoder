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
// transcoding/adm/adm_to_lusid.cpp — Future ADM→LUSID adapter (Phase 3+)
//
// DEVELOPMENT NOTES
// =================
// Phase: 3+
// Status: Stub — not compiled or linked yet.
//
// NOTE: The *active* Phase 2 ADM→LUSID conversion lives in src/adm_to_lusid.cpp
// and include/adm_to_lusid.hpp. That implementation uses pugixml for raw
// document-order traversal to maintain parity with the Python oracle.
//
// Purpose of THIS file (future):
//   This adapter will serve as a higher-level orchestration layer once Phase 3
//   introduces ADM WAV ingestion (adm_reader.cpp). It will coordinate:
//     1. Receiving an XML buffer or pugixml document from adm_reader
//     2. Delegating to the core converter (src/adm_to_lusid.cpp)
//     3. Applying any Phase 4+ profile resolution or LFE flag overrides
//
// Why two files?
//   - src/adm_to_lusid.cpp: core XML→LUSID conversion logic (Phase 2, stable)
//   - transcoding/adm/adm_to_lusid.cpp: orchestration + profile resolution (Phase 3+)
//
// This separation keeps the Phase 2 parity-tested core untouched while
// allowing Phase 3+ to add wrapper logic for new input paths and flags.
//
// References:
//   - AGENTS-CULT §4 (Phase 2 scope), §8 (Phase 3)
//   - DEV-PLAN-CULT Phase 3
// ---------------------------------------------------------------------------
