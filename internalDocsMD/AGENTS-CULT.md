# AGENTS.md — CULT Transcoder (Execution-Safe, Contract-First)

Repo placement: `spatialroot/cult-transcoder/` (git submodule, already wired).  
Invocation: CLI-first. Spatial Root calls `spatialroot/cult-transcoder/build/cult-transcoder` (macOS/Linux) or the `.bat` wrapper on Windows (see §9).  
Build: CMake. C++17. Cross-platform (macOS + Windows required; Linux later).  
Test framework: Catch2 (via CMake FetchContent; no install required).  
License: Apache-2.0. All source files carry an Apache-2.0 header with Cult-DSP copyright.  
Canonical: LUSID Scene v0.5 JSON.

This file is designed to be executable and non-destructive. Do not guess.

### Development environment

The `spatialroot` repo uses a Python virtual environment (`source activate.sh`).
The LUSID package and all Python dependencies are already installed in the venv —
**do not install packages or create new environments**. Just activate and import:

```bash
cd /Users/lucian/projects/spatialroot
source activate.sh
python3 -c "from LUSID.src.xml_etree_parser import parse_adm_xml_to_lusid_scene"
```

Use this venv whenever you need to run the Python oracle for parity testing or
any other Python tooling in the workspace.

---

## 0.5. Pinned Implementation Decisions

These decisions are final and must not be changed without a doc-update PR.

| Decision             | Value                                                                                                                                             |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| C++ standard         | C++17                                                                                                                                             |
| Test framework       | Catch2 (CMake FetchContent, no install)                                                                                                           |
| License              | Apache-2.0, Cult-DSP copyright in every source file                                                                                               |
| XML parser           | pugixml (FetchContent, MIT license) — mirrors Python oracle's raw XML traversal for ordering parity. **Not** libadm (different traversal order).  |
| BW64 reader          | libbw64 (git submodule at `thirdparty/libbw64`, Apache-2.0, header-only) — Phase 3. Used only for axml chunk extraction. **Not** FetchContent.   |
| Windows binary       | `build/cult-transcoder.exe` called via `scripts/cult-transcoder.bat` wrapper                                                                      |
| `.bat` wrapper rule  | Must be committed; every call-site in Spatial Root pipeline must have an inline comment explaining the indirection                                |
| Fail-report behavior | On any error (missing file, unsupported format, etc.) the CLI writes a best-effort `status: "fail"` report to the default path and exits non-zero |
| Atomic output        | All output files written to `<path>.tmp` first, then `std::filesystem::rename()` to final path. Temp cleaned on failure. Enforced Phase 2+.      |

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

## 1. Repo Layout (Phase 3 — current state)

```
cult_transcoder/
├── .gitignore                         # excludes build/, .DS_Store, IDE dirs
├── CMakeLists.txt                     # C++17, Catch2 + pugixml FetchContent, libbw64 submodule, git SHA injection
├── OVERVIEW.md                        # project overview with phase table
├── internalDocsMD/
│   ├── AGENTS-CULT.md                 # this file
│   ├── DEV-PLAN-CULT.md
│   ├── DESIGN-DOC-V1-CULT.MD
│   └── design-reference-CULT.md       # research/literature design reference
├── include/
│   ├── adm_to_lusid.hpp              # Phase 2+3: structs + convertAdmToLusid() + convertAdmToLusidFromBuffer()
│   ├── cult_transcoder.hpp            # TranscodeRequest / TranscodeResult / transcode()
│   ├── cult_report.hpp                # Report model (LossLedgerEntry, ReportSummary, …)
│   └── cult_version.hpp               # kVersionString, kReportSchemaVersion, gitCommit()
├── scripts/
│   └── cult-transcoder.bat            # Windows wrapper — call via this, not .exe directly
├── src/
│   ├── main.cpp                       # CLI entry point, atomic report write, exit codes
│   ├── transcoder.cpp                 # Phase 3: validates args → adm_xml or adm_wav dispatch → atomic write
│   ├── adm_to_lusid.cpp              # Phase 2+3: ADM XML → LUSID (pugixml, encounter order, parseAdmDocument helper)
│   └── report.cpp                     # JSON serializer (zero external deps)
├── thirdparty/
│   └── libbw64/                       # Phase 3: git submodule (ebu/libbw64, Apache-2.0, header-only)
│                                      #   CMake INTERFACE target 'libbw64'
│                                      #   Used only in transcoding/adm/adm_reader.cpp
├── transcoding/
│   ├── adm/
│   │   ├── adm_reader.hpp             # Phase 3: AxmlResult struct, extractAxmlFromWav() declaration
│   │   ├── adm_reader.cpp             # Phase 3: BW64 → axml extraction, debug XML artifact write
│   │   └── adm_profile_resolver.cpp   # Phase 4: Dolby Atmos / Sony 360RA profile detection (stub)
│   └── lusid/
│       ├── lusid_writer.cpp           # Phase 3+: enhanced writer (stub)
│       └── lusid_validate.cpp         # Phase 3+: LUSID schema validation (stub)
└── tests/
    ├── test_report.cpp                # 10 tests — §7 report schema contract
    ├── test_cli_args.cpp              # 10 tests — transcode() arg validation (3 Phase 3 tests added)
    └── parity/
        ├── run_parity.cpp             # Phase 2: real parity tests against Python oracle
        └── fixtures/
            ├── .gitkeep
            ├── test_input.xml         # ADM XML test fixture (Dolby Atmos master)
            └── reference_scene.lusid.json  # Python oracle output (contains_audio=None)
```

**Note on test_main.cpp**: Catch2 v3 provides its own entry point via
`Catch2::Catch2WithMain`; a separate `test_main.cpp` is not needed.

**Note on transcoding/ stubs**: `adm_reader.hpp` and `adm_reader.cpp` are Phase 3
*implemented* files, fully compiled and linked. `adm_profile_resolver.cpp`,
`lusid_writer.cpp`, and `lusid_validate.cpp` remain Phase 4+ stubs (not compiled).

---

## 2. CLI Contract (Stable)

Command:

```
cult-transcoder transcode
  --in <path> --in-format <adm_xml|adm_wav>
  --out <path> --out-format lusid_json
  [--report <path>]
  [--stdout-report]
```

Known `--in-format` values:
- `adm_xml` (Phase 2): input is a bare ADM XML file (e.g. `currentMetaData.xml`)
- `adm_wav` (Phase 3): input is a BW64 WAV with embedded axml chunk

Defaults:

- If `--report` omitted: report path is `<out>.report.json` (F1).
- `--stdout-report` prints report JSON to stdout. File report is still written unless a future option disables it.

Exit codes:

- `0`: success; final output exists; report exists.
- non-zero: failure; final output must not exist; report should exist best-effort; stderr must explain.

Atomic output rule (non-negotiable):

- Write output and report to temporary filenames (`<path>.tmp`) first, validate, then atomic rename to final paths.
- On failure: remove temp outputs; do not leave partial artifacts.
- Implemented in Phase 2 (LUSID write) and Phase 3 (report write via main.cpp).

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

## 8. Phase 3 Ingestion — **IMPLEMENTED (2026-03-04)**

Phase 3 goal: Move ADM WAV ingestion/extraction into CULT entirely, replacing
`spatialroot_adm_extract` + the Python `extractMetaData()` subprocess.

**Status: COMPLETE.** All items below are implemented and 28/28 tests pass.

### What was built

| Component | File | Description |
|-----------|------|-------------|
| libbw64 submodule | `thirdparty/libbw64/` | EBU libbw64, Apache-2.0, header-only. Added as git submodule. |
| BW64 reader | `transcoding/adm/adm_reader.hpp` + `.cpp` | `extractAxmlFromWav()`: opens BW64 WAV, reads axml chunk, writes debug XML artifact, returns XML string in `AxmlResult`. |
| Buffer-based converter | `include/adm_to_lusid.hpp` + `src/adm_to_lusid.cpp` | `convertAdmToLusidFromBuffer(xmlBuffer)`: parses ADM XML from in-memory string. Delegates to shared `static parseAdmDocument()` helper — zero logic duplication with `convertAdmToLusid()`. |
| adm_wav dispatch | `src/transcoder.cpp` | `kKnownInFormats = {"adm_xml", "adm_wav"}`. When `adm_wav`: calls `extractAxmlFromWav` then `convertAdmToLusidFromBuffer`. |
| Atomic LUSID write | `src/transcoder.cpp` | Writes to `<out>.tmp` then `std::filesystem::rename()` to final path. Temp cleaned on failure. |
| Phase 3 tests | `tests/test_cli_args.cpp` | 3 new tests: adm_wav format recognized, adm_xml regression, non-BW64 file fails at axml (not at format). Total: 10 tests. |

### Pinned behavior (D2 — unchanged)

- Debug XML artifact always written to `processedData/currentMetaData.xml` (relative to CWD).
- Failure to write the artifact is a **warning**, not an error. Transcode continues.
- `containsAudio` is NOT used or written in Phase 3 (same as Phase 2 — all channels assumed active, §4).
- Report is written alongside LUSID at `<out>.report.json`.

### spatialroot_adm_extract — DEPRECATED

The `src/adm_extract/` CMake project and `spatialroot_adm_extract` binary in the
spatialroot repo are superseded by Phase 3. As of this session:

- `runRealtime.py` no longer calls `extractMetaData()`.  
  It calls `cult_transcoder/build/cult-transcoder transcode --in-format adm_wav` instead.
- `configCPP_posix.py` and `configCPP_windows.py` have the `initializeEbuSubmodules()` and  
  `buildAdmExtractor()` calls commented out with Phase 3 explanation.
- `src/adm_extract/` remains in place (not deleted) pending formal cleanup.
- `runPipeline.py` is deprecated (do-not-touch); its `extractMetaData()` call is left as-is.

### Pipeline call site (runRealtime.py)

```python
# cult_transcoder/build/cult-transcoder
CULT_BINARY = project_root / "cult_transcoder" / "build" / "cult-transcoder"
scene_json_path = "processedData/stageForRender/scene.lusid.json"

result = subprocess.run([
    str(CULT_BINARY), "transcode",
    "--in",         source_adm_file,
    "--in-format",  "adm_wav",
    "--out",        scene_json_path,
    "--out-format", "lusid_json",
], check=False)

if result.returncode != 0:
    return False
```

---

## 9. Build and Binary Path (Pinned)

Binary path expected by pipeline:

- macOS / Linux: `spatialroot/cult-transcoder/build/cult-transcoder`
- Windows: `spatialroot/cult-transcoder/build/cult-transcoder.exe`

On Windows, Spatial Root's pipeline scripts must call the binary via a thin
`.bat` wrapper (`spatialroot/cult-transcoder/build/cult-transcoder.bat`) so
that the call-site code does not need OS branching. The wrapper and its purpose
**must be documented with inline comments wherever it is invoked in the Spatial
Root pipeline** so the indirection is obvious to maintainers.

Agents must ensure:

- Documented build steps produce the binary at this exact path deterministically.
- Spatial Root pipeline calls this exact path (no PATH lookup).
- The `.bat` wrapper is committed alongside the CMake build output instructions
  and kept in sync if the binary name changes.

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
