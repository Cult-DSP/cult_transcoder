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
// transcoding/adm/adm_reader.cpp — ADM WAV Ingestion (Phase 3+)
//
// DEVELOPMENT NOTES
// =================
// Phase: 3
// Status: Stub — not compiled or linked yet.
//
// Purpose:
//   Read an ADM-BWF (.wav) file, extract the axml metadata chunk, and
//   optionally write it as a debug artifact to processedData/currentMetaData.xml
//   (pinned behavior D2 — AGENTS-CULT §8).
//
// Phase 3 responsibilities:
//   - Accept --in-format adm_wav in the CLI.
//   - Extract axml chunk from BW64 container (libbw64 or manual chunk reader).
//   - Pass extracted XML buffer to the existing adm_to_lusid converter
//     (src/adm_to_lusid.cpp).
//   - Write debug XML artifact (enabled by default, AGENTS §8).
//   - Output LUSID + report, same as Phase 2 but from WAV input directly.
//
// Dependencies:
//   - libbw64 (FetchContent, Apache-2.0) — for BW64/RF64 chunk reading.
//   - pugixml (already available) — for parsing the extracted axml buffer.
//
// Design considerations:
//   - Parse directly from extracted buffer (avoid writing then re-reading XML).
//   - pugixml supports load_buffer() for in-memory XML parsing.
//   - Debug XML artifact written via pugixml save_file() or raw buffer dump.
//   - Gate: parity against Phase 2 output for same input WAV.
//
// References:
//   - AGENTS-CULT §8 (Phase 3 Ingestion)
//   - DEV-PLAN-CULT Phase 3
//   - DESIGN-DOC-V1-CULT §ADM and Object Metadata
// ---------------------------------------------------------------------------
