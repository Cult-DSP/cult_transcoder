# AGENTS.md — CULT Transcoder (Execution-Safe, Contract-First)

Repo placement: `spatialroot/cult-transcoder/` (git submodule).  
Invocation: CLI-first. Spatial Root calls `spatialroot/cult-transcoder/build/cult-transcoder`.  
Build: CMake. Cross-platform (macOS + Windows required; Linux later).  
Canonical: LUSID Scene v0.5 JSON.

This file is designed to be executable and non-destructive. Do not guess.

---

## 0. Authorities and Documentation Coupling

Authority order (resolve conflicts in this order):

1. `LUSID/internalDocsMD/toolchain_AGENTS.md` (contract authority)
2. LUSID schema + LUSID scene semantics
3. Spatial Root pipeline docs
4. CULT docs

Self-reflexive documentation rule:
If you change any of:

- CLI args, defaults, or binary path assumptions
- output file paths or naming
- ordering rules
- scene semantics (duration, timeUnit, node types)
- LFE detection behavior
- ingestion responsibilities (who extracts ADM XML)
- flags/toolchain behavior
  then update, in the same PR:
- `spatialroot/AGENTS.md` (pipeline + GUI wiring)
- `LUSID/LUSID_AGENTS.md`
- if and only if the toolchain contract changes: update `LUSID/internalDocsMD/toolchain_AGENTS.md` and `LUSID/internalDocsMD/CHANGELOG_TOOLCHAIN.md`

No behavior change without doc change.

---

## 1. Repo Layout (must exist)

spatialroot/cult-transcoder/
AGENTS.md
DEV_PLAN.md
CMakeLists.txt

include/
cult_transcoder.hpp
cult_report.hpp
cult_version.hpp

src/
main.cpp
transcoder.cpp
report.cpp

transcoding/
adm/
adm_reader.cpp
adm_to_lusid.cpp
adm_profile_resolver.cpp # Phase 4+
lusid/
lusid_writer.cpp
lusid_validate.cpp

tests/
parity/
run_parity.cpp
fixtures/

---

## 2. CLI Contract (Stable)

Command:

cult-transcoder transcode
--in <path> --in-format adm_xml
--out <path> --out-format lusid_json
[--report <path>]
[--stdout-report]

Defaults:

- If `--report` omitted: report path is `<out>.report.json` (F1).
- `--stdout-report` prints report JSON to stdout. File report is still written unless a future option disables it.

Exit codes:

- `0`: success; final output exists; report exists.
- non-zero: failure; final output must not exist; report should exist best-effort; stderr must explain.

Atomic output rule (non-negotiable):

- Write output and report to temporary filenames first, validate, then atomic rename to final paths.
- On failure: remove temp outputs; do not leave partial artifacts.

---

## 3. Phase 2 Oracle: Python Parser (Do Not Modify)

Phase 2 must mirror the existing working system.

Oracle file (do not modify to match C++):

- `LUSID/src/xml_etree_parser.py`

Rule:

- Fix C++ to match Python output.
- Do not change Python to match C++.
- Parity failures are C++ bugs unless fixture inputs changed.

---

## 4. Phase 2 Scope Decisions (Pinned)

Inputs/outputs:

- Input ADM XML path: `processedData/currentMetaData.xml` (Spatial Root extractor output)
- Output LUSID path: `processedData/stageForRender/scene.lusid.json`
- Report path: default `<out>.report.json`

Time model:

- Always output `timeUnit: "seconds"` in Phase 2.
- Add a future note that non-second time units may be required later, but do not change now.

ContainsAudio removal:

- Phase 2 does not use `containsAudio.json`.
- Spatial Root pipeline must remove/comment the `containsAudio` step and any dependency on it.
- CULT assumes channels are active (equivalent to Python’s default behavior when no contains-audio dict is provided).

LFE behavior:

- Phase 2 uses the same behavior as Python hardcoded mode:
  - LFE is the 4th DirectSpeaker encountered (1-based) if that logic is enabled in the Python oracle.
  - Emit node type `LFE` if detected.
- Future plan (Phase 4): add a flag for improved detection (e.g., speakerLabel or bed-aware). Default must preserve Phase 2 behavior.

Ordering:

- Stable ordering is critical.
- Phase 2 ordering must match Python ordering exactly (see Section 5).

Deletion policy:

- After manual validation (G3), disable/remove Python parser usage in Spatial Root. No fallback flag.

---

## 5. Stable Ordering Rules (Non-Negotiable)

Purpose:

- Downstream debugging depends on stable identity and ordering.
- Parity tests depend on ordering.

Phase 2 ordering must match Python oracle:

1. Direct speakers (beds) appear first in the `t=0` frame, in Python encounter order.
2. Objects appear after direct speakers, also in Python encounter order.
3. Frames are in ascending time order.
4. Nodes within each frame are appended in the same loop order Python produces.

Implementation constraints:

- If libadm traversal order differs from Python encounter order, implement explicit order preservation.
- Do not introduce sorted-by-id ordering in Phase 2.
- Do not “canonicalize” ordering in Phase 2.

---

## 6. Parity and Equality (Hard Fail)

Equality definition (Phase 2):

- Parse Python output JSON and CULT output JSON.
- Deep-compare parsed JSON objects.
- Numbers compare by value (so 1 and 1.0 are equal).
- Hard fail on any mismatch.

Important:

- Because ordering is pinned to Python, ordering mismatches should not occur; treat them as failures.

Parity test requirements:

- Provide at least one “real” ADM export and at least one minimal fixture under `tests/parity/fixtures/`.
- Parity runner produces:
  - CULT output LUSID
  - Python output LUSID
  - Diff report on mismatch (structural path to first difference)

---

## 7. Report Schema (v0.1, Required From Day 1)

Report must include:

- `reportVersion` (e.g., "0.1")
- `tool`: `{ name, version, gitCommit }`
- `args`: full resolved CLI args/options used
- `status`: "success" | "fail"
- `errors[]`, `warnings[]`
- `summary`: `{ durationSec, sampleRate, timeUnit, numFrames, countsByNodeType }`
- `lossLedger[]`: required field even if empty
  - each entry: `{ kind, field, reason, count, examples[] }`

Report location:

- Default `<out>.report.json`
- Also printable to stdout via `--stdout-report`

No heavy hashing in v0.1 (no input SHA).

---

## 8. Phase 3 Ingestion (Pinned)

Phase 3 goal:

- Move ADM WAV ingestion/extraction into CULT.
- Keep `processedData/currentMetaData.xml` as a debug artifact, but parsing can occur directly from extracted axml buffer.

Pinned behavior (D2):

- Default: parse directly, and write XML when debug output is enabled by default.
- Keep the XML artifact in the same location for continuity.

Phase 3 output:

- Only XML and LUSID (no containsAudio in Phase 3 either).
- Report next to LUSID.

---

## 9. Build and Binary Path (Pinned)

Binary path expected by pipeline:

- `spatialroot/cult-transcoder/build/cult-transcoder`

Agents must ensure:

- documented build steps produce this exact path deterministically.
- Spatial Root pipeline calls this exact path (no PATH lookup).

---

## 10. Future MPEG-H Support (Pinned Strategy)

Plan to support both MPEG-H backends behind CMake options:

- `WITH_MPEGH_ITTIAM` default ON (OSI-friendly BSD)
- `WITH_MPEGH_FRAUNHOFER` default OFF (FDK-licensed optional)

Sony 360RA priority:

- Ingest via exports (ADM or MPEG-H) with a profile resolver later.

---

## 11. Agent PR Checklist (Required)

Every PR must include:

- Phase targeted (P1–P6)
- Parity status (P2): pass/fail and what fixtures were used
- Ordering preservation confirmed (yes/no)
- timeUnit is "seconds" (yes/no)
- LFE behavior unchanged (yes/no)
- Pipeline dependency on containsAudio removed/commented (yes/no)
- Docs updated (list exact files)
- Report schema version and fields confirmed (yes/no)

No merge if:

- parity fails (Phase 2)
- ordering rules drift (Phase 2)
- output is non-atomic
- docs not updated for behavior changes
