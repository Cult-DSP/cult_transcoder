```markdown
# DEV_PLAN.md — CULT Transcoder (Phased, Safety-Gated)

This plan is derived from the CULT `AGENTS.md` contract.
All phases must preserve the toolchain contract authority: `LUSID/internalDocsMD/toolchain_AGENTS.md`. :contentReference[oaicite:21]{index=21}

---

## Phase 1 — Repo Skeleton + CLI + Report Schema (No Pipeline Changes)

**Status: ✅ COMPLETE**

### Pinned decisions for this phase

| Decision             | Value                                                                                                          |
| -------------------- | -------------------------------------------------------------------------------------------------------------- |
| C++ standard         | C++17                                                                                                          |
| Test framework       | Catch2 via CMake FetchContent (no install required)                                                            |
| License              | Apache-2.0, Cult-DSP copyright header in every source file                                                     |
| Third-party deps     | FetchContent stubs for libadm + libbw64 declared but `EXCLUDE_FROM_ALL`; activated in Phase 2                  |
| Windows binary       | `build/cult-transcoder.exe`; committed `scripts/cult-transcoder.bat` wrapper is the call-site for the pipeline |
| Fail-report behavior | Bad input → write `status: "fail"` report best-effort to default path, exit non-zero                           |

### Goals

- Establish repo structure, CMake build, and CLI framework.
- Produce report JSON with required schema, even before real transcoding.

### Deliverables

| Deliverable                                             | Status |
| ------------------------------------------------------- | ------ |
| `include/` + `src/` + `transcoding/` skeleton           | ✅     |
| `CMakeLists.txt` (C++17, Catch2, libadm/libbw64 stubs)  | ✅     |
| `scripts/cult-transcoder.bat` Windows wrapper           | ✅     |
| `cult-transcoder transcode` parses all defined args     | ✅     |
| `status: "fail"` report on bad input + non-zero exit    | ✅     |
| Unit tests: CLI arg validation (`test_cli_args.cpp`)    | ✅     |
| Unit tests: report schema shape (`test_report.cpp`)     | ✅     |
| Parity test placeholder (`tests/parity/run_parity.cpp`) | ✅     |

### Gates

- No changes to Spatial Root pipeline.
- No changes to LUSID schema or toolchain docs.
- Binary produced at `build/cult-transcoder` (macOS) / `build/cult-transcoder.exe` (Windows).

---

## Phase 2 — ADM XML → LUSID JSON (Mirror Python Oracle Exactly)

**Status: ✅ CORE COMPLETE — 25/25 tests passing (including byte-for-byte parity)**

### Goals

- Implement C++ ADM→LUSID conversion using pugixml (raw encounter-order traversal).
- Output must match Python oracle under parsed-object equality, with stable ordering mirroring Python.
- timeUnit fixed to `"seconds"`.

### Inputs/Outputs

- Input: `processedData/currentMetaData.xml` (produced by existing extractor)
- Output: `processedData/stageForRender/scene.lusid.json`
- Report: default `<out>.report.json`

### Required behavior parity

- Mirror `LUSID/src/xml_etree_parser.py` behavior as oracle.
- Stable ordering matches Python.
- LFE hardcode behavior matches Python (C1).
- Remove/comment out `containsAudio` from pipeline; Phase 2 does not use it.

### Work items

| #   | Work Item                                                                                                                                                                                                  | Status                                                         |
| --- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------- |
| 1   | Replace libadm/libbw64 FetchContent stubs with **pugixml** (MIT, header-friendly). Rationale: Python oracle uses `xml.etree.ElementTree` with raw encounter-order iteration. pugixml mirrors this exactly. | ✅                                                             |
| 2   | Implement `src/adm_to_lusid.cpp` / `include/adm_to_lusid.hpp`: parse ADM XML, extract DS + Objects in encounter order, build frames, LFE hardcoded ch4, no containsAudio.                                  | ✅                                                             |
| 3   | Implement LUSID JSON writer (`lusidSceneToJson()`, `writeLusidScene()` — manual serialization matching `json.dump(indent=2)`)                                                                              | ✅                                                             |
| 4   | Wire into `transcoder.cpp`: replace Phase 1 stub with real pipeline (`convertAdmToLusid` → `writeLusidScene`, populate report summary)                                                                     | ✅                                                             |
| 5   | Implement atomic write behavior (already in main.cpp for report; extend to LUSID output)                                                                                                                   | ✅ completed in Phase 3                                        |
| 6   | Implement parity test runner: generate Python ref (`contains_audio=None`), compare C++ output, hard fail on mismatch. Tests in `tests/parity/run_parity.cpp`, fixtures in `tests/parity/fixtures/`.        | ✅ 8 parity tests passing (including byte-for-byte JSON match) |
| 7   | Update Spatial Root pipeline to call `cult-transcoder` binary for adm_xml→lusid_json                                                                                                                       | ✅ completed in Phase 3 (`runRealtime.py` uses `adm_wav`)      |
| 8   | Remove/comment out `containsAudio` dependency in pipeline and docs                                                                                                                                         | ✅ completed in Phase 3 (configCPP, runRealtime.py)            |

### Gates

- Parity test passes on at least one real ADM export + at least one fixture.
- Manual validation session performed (G3).
- After manual validation: remove/disable Python parser usage in pipeline.
- Update docs:
  - `spatialroot/AGENTS.md` pipeline wiring :contentReference[oaicite:27]{index=27}
  - `LUSID/LUSID_AGENTS.md` notes about CULT replacing Python parser

---

## Phase 3 — Move ADM WAV Ingestion/Extraction Into CULT

**Status: ✅ COMPLETE (2026-03-04) — 28/28 tests passing**

### Goals

- CULT ingests ADM WAV directly via `--in-format adm_wav`.
- CULT extracts axml from BW64 container and produces LUSID.
- Writes `processedData/currentMetaData.xml` as a debug artifact (D2) — always on.
- Parses directly from extracted buffer; disk round-trip only for the debug XML.
- `spatialroot_adm_extract` binary superseded; Python `extractMetaData()` call removed from pipeline.
- `scan_audio` / `containsAudio` logic fully removed from realtime pipeline.

### Work items

| #  | Work Item                                                                                    | Status |
|----|----------------------------------------------------------------------------------------------|--------|
| P3-1 | Add libbw64 as git submodule at `thirdparty/libbw64` (Apache-2.0, header-only)            | ✅     |
| P3-2 | Update `CMakeLists.txt`: libbw64 INTERFACE target, adm_reader in sources, link targets     | ✅     |
| P3-3 | Implement `transcoding/adm/adm_reader.hpp` + `.cpp` (`extractAxmlFromWav()`)               | ✅     |
| P3-4 | Refactor `adm_to_lusid.cpp`: extract `parseAdmDocument()` helper, add `convertAdmToLusidFromBuffer()` | ✅ |
| P3-5 | Wire `adm_wav` dispatch in `transcoder.cpp`; atomic LUSID write (Phase 2 item 5)           | ✅     |
| P3-6 | Add 3 Phase 3 test cases to `test_cli_args.cpp` (10 total, 28/28 pass)                    | ✅     |
| P3-7 | Update `runRealtime.py`: replace Steps 2–4 with single `cult-transcoder adm_wav` call; remove `scan_audio`; remove unused imports | ✅ |
| P3-8 | Update `realtime_runner.py`: remove `scan_audio` from `RealtimeConfig` and `_build_args()` | ✅     |
| P3-9 | Comment out `initializeEbuSubmodules()` + `buildAdmExtractor()` in `configCPP_posix.py` + `configCPP_windows.py` | ✅ |
| P3-10 | Update AGENTS-CULT.md, DEV-PLAN-CULT.md, spatialroot AGENTS.md                           | ✅     |

### Gates

- 28/28 tests passing. ✅
- Parity against Phase 2 output (for same input WAV via XML path) — guaranteed by shared `parseAdmDocument()`. ✅
- `runRealtime.py` has 0 lint errors. ✅

---

## Phase 4 — ADM Profile Resolver + Improved LFE Detection (Flagged)

### Goals

- Support Dolby Atmos variants and Sony 360RA export patterns.
- Introduce LFE detection flag:
  - default remains hardcoded-index (Phase 2 behavior)
  - add future mode for bed-aware or speakerLabel-based detection (C2 plan)
- Introduce flags via Spatial Root’s toolchain flags file (single source of truth).

### Gates

- Flag default produces identical output to Phase 2/3.
- Non-default modes produce report entries in loss ledger + warnings.

---

## Phase 5 — GUI: Transcoding Tab (Report “Open” UX)

### Goals

- Add a “Transcode” tab in GUI.
- GUI calls CULT CLI and then offers “open report” + status output.

### Deliverables

- GUI wiring similar to current pipeline QProcess patterns :contentReference[oaicite:28]{index=28}
- No silent failures; report path always surfaced.

### Gates

- GUI properly handles non-zero exit codes and shows errors.

---

## Phase 6 — MPEG-H Support (Sony 360RA Priority), then IAMF

### MPEG-H approach

- Implement both backends behind CMake options:
  - `WITH_MPEGH_ITTIAM` default ON
  - `WITH_MPEGH_FRAUNHOFER` default OFF
- Add capability matrix + loss ledger updates for conversions.

### Sony 360RA

- Support ingest via exports (ADM or MPEG-H) with resolver.

### Gates

- Fixtures for each format.
- Every new adapter must produce a loss ledger entry when lossy.

---

## Global documentation rules (applies to all phases)

If you change dataflow, paths, flags, schema semantics:

- update `spatialroot/AGENTS.md`
- update `LUSID/LUSID_AGENTS.md`
- if contract changes: update `toolchain_AGENTS.md` + `CHANGELOG_TOOLCHAIN.md` :contentReference[oaicite:29]{index=29}
```
