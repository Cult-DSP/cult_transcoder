# AGENTS.md — CULT Transcoder (Execution-Safe, Contract-First)

Repo placement: `spatialroot/cult-transcoder/` (git submodule, already wired).  
Invocation: CLI-first. Spatial Root calls `spatialroot/cult-transcoder/build/cult-transcoder` (macOS/Linux) or the `.bat` wrapper on Windows (see §9).  
Build: CMake. C++17. Cross-platform (macOS + Windows required; Linux later).  
Test framework: Catch2 (via CMake FetchContent; no install required).  
License: Apache-2.0. All source files carry an Apache-2.0 header with Cult-DSP copyright.  
Canonical: LUSID Scene v0.5 JSON.

This file is designed to be executable and non-destructive. Do not guess.

Last verified against the submodule code: 2026-03-31.  
This document is updated to reflect the planned ADM authoring extension from LUSID.  
`internalDocsMD/DEV-PLAN-CULT.md` is outdated and must not be used as a source of truth.

### Development environment

Spatial Root runtime is C++ only: the production app calls CULT and the spatial engine directly and removes all Python from the runtime pipeline. The Python venv is used only for parity/oracle checks.

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

## 0.3. Current Implementation Snapshot (Verified)

Observed in the current submodule (no assumptions beyond code/tests in this repo):

- ADM XML → LUSID JSON conversion is implemented in `src/adm_to_lusid.cpp`.
- CLI supports only `adm_xml` → `lusid_json` today (see `src/main.cpp`, `src/transcoder.cpp`).
- XML parser is pugixml (FetchContent in `CMakeLists.txt`); libadm is not used in the current ingest/parity path.
- Parity tests exist and compare JSON output to a Python oracle fixture in `tests/parity`.
- Report schema v0.1 is implemented in `include/cult_report.hpp` + `src/report.cpp`.
- Report file is written via temp + rename in `src/main.cpp` (atomic for report only).
- LUSID output is written directly in `writeLusidScene()` (non-atomic as of now).
- No LUSID → ADM authoring implementation is currently verified in the repo.

Known doc mismatch to fix later:

- `OVERVIEW.md` phase table still lists Phase 2 as pending and mentions libadm; this is stale.

## 0.4. Architectural Position of the ADM Authoring Extension (Pinned)

The planned LUSID → ADM feature is an **export-side extension** layered on top of the canonical LUSID scene model.
It is **not** a replacement for the existing parity-critical ADM ingest path.

Pinned framing:

- LUSID remains the canonical intermediate.
- Existing ADM ingest/parity behavior remains frozen unless explicitly updated.
- ADM authoring must be added as a separate command/API surface.
- New export-side dependencies must not alter ordering, parsing, or equality behavior in the current `adm_xml -> lusid_json` path.
- Use of `libadm` is allowed for authoring/export work only; it does not become the semantic owner of CULT.
- CULT still owns scene interpretation, canonical normalization, export policy, and loss reporting.

## 0.5. Pinned Implementation Decisions

These decisions are final and must not be changed without a doc-update PR.

| Decision                          | Value                                                                                                                                             |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| C++ standard                      | C++17                                                                                                                                             |
| Test framework                    | Catch2 (CMake FetchContent, no install)                                                                                                           |
| License                           | Apache-2.0, Cult-DSP copyright in every source file                                                                                               |
| XML parser for ingest parity path | pugixml (FetchContent, MIT license) — mirrors Python oracle's raw XML traversal for ordering parity. **Not** libadm for the current parity path   |
| ADM authoring library             | `libadm` may be introduced for the new LUSID → ADM export path only                                                                               |
| BW64 packaging library            | `libbw64` remains the BW64 container dependency                                                                                                   |
| ADM authoring normalized audio    | mono, 48 kHz, float32 WAV (v1 export path)                                                                                                        |
| ADM authoring resampling          | allowed only in `adm-author` normalization stage; must be explicit in report                                                                      |
| ADM authoring duration source     | normalized WAV frame count; scene duration must match or fail                                                                                     |
| Windows binary                    | `build/cult-transcoder.exe` called via `scripts/cult-transcoder.bat` wrapper                                                                      |
| `.bat` wrapper rule               | Must be committed; every call-site in Spatial Root pipeline must have an inline comment explaining the indirection                                |
| Fail-report behavior              | On any error (missing file, unsupported format, etc.) the CLI writes a best-effort `status: "fail"` report to the default path and exits non-zero |

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
- export responsibilities (what CULT now writes, including authored ADM/BW64)
- flags/toolchain behavior

then update, in the same PR:

- `spatialroot/AGENTS.md` (pipeline + GUI wiring)
- `LUSID/LUSID_AGENTS.md`
- if and only if the toolchain contract changes: update `LUSID/internalDocsMD/toolchain_AGENTS.md` and `LUSID/internalDocsMD/CHANGELOG_TOOLCHAIN.md`

No behavior change without doc change.

---

## 1. Repo Layout (current state + planned authoring additions)

```text
cult_transcoder/
├── .gitignore
├── CMakeLists.txt
├── OVERVIEW.md
├── internalDocsMD/
│   ├── AGENTS-CULT.md
│   ├── DEV-PLAN-CULT.md                # outdated; do not use as source of truth
│   ├── DESIGN-DOC-V1-CULT.MD
│   └── design-reference-CULT.md
├── include/
│   ├── adm_to_lusid.hpp
│   ├── cult_transcoder.hpp
│   ├── cult_report.hpp
│   └── cult_version.hpp
├── scripts/
│   └── cult-transcoder.bat
├── src/
│   ├── main.cpp
│   ├── transcoder.cpp
│   ├── adm_to_lusid.cpp
│   └── report.cpp
├── transcoding/
│   ├── adm/
│   │   ├── adm_reader.cpp
│   │   ├── adm_to_lusid.cpp
│   │   ├── adm_profile_resolver.cpp
│   │   ├── lusid_to_adm.cpp           # planned: canonical scene -> ADM authoring model
│   │   └── adm_bw64.cpp               # planned: authored ADM + audio -> BW64 output
│   └── lusid/
│       ├── lusid_writer.cpp
│       ├── lusid_validate.cpp
│       └── lusid_reader.cpp           # planned: minimal reader for authoring path
└── tests/
    ├── test_report.cpp
    ├── test_cli_args.cpp
    ├── test_adm_author_args.cpp       # planned
    ├── test_lusid_to_adm_mapping.cpp  # planned
    ├── test_adm_author_integration.cpp# planned
    └── parity/
        ├── run_parity.cpp
        └── fixtures/
```

Notes:

- The `transcoding/` tree may remain partially stubbed until those files are compiled and linked.
- The authoring path must remain cleanly separated from the existing parity-critical ingest path.

---

## 2. CLI Contract (Stable + planned extension)

### Existing stable command

```bash
cult-transcoder transcode \
  --in <path> --in-format adm_xml \
  --out <path> --out-format lusid_json \
  [--report <path>] \
  [--stdout-report]
```

Defaults:

- If `--report` omitted: report path is `<out>.report.json`.
- `--stdout-report` prints report JSON to stdout. File report is still written unless a future option disables it.

### Planned ADM authoring command

```bash
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] \
  [--stdout-report]

# alternate input
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] \
  [--stdout-report]
```

Exit codes for both commands:

- `0`: success; final output(s) exist; report exists.
- non-zero: failure; final output(s) must not exist; report should exist best-effort; stderr must explain.

Atomic output behavior:

- Report is written via temp + rename.
- Existing LUSID output remains non-atomic until separately fixed.
- `adm-author` outputs (`export.adm.xml`, `export.adm.wav`) must be written via temp + rename from day one.

Authoring path input rules (v1):

- `containsAudio.json` is deprecated; resolve WAVs by deterministic id-to-filename mapping
- normalize only non-48 kHz mono WAVs to 48 kHz float32 (resampling reported explicitly)
- validate strict equal frame counts after normalization; fail on mismatch
- if scene duration metadata exists, validate against normalized audio duration; fail on mismatch

---

## 3. Phase 2 Oracle: Python Parser (Do Not Modify)

Phase 2 must mirror the existing working system.

Oracle file (do not modify to match C++):

- `LUSID/src/xml_etree_parser.py`

Rule:

- Fix C++ to match Python output.
- Do not change Python to match C++.
- Parity failures are C++ bugs unless fixture inputs changed.

This rule applies only to the existing ADM ingest/parity path, not to the new authoring/export path.

---

## 4. Phase 2 Scope Decisions (Pinned)

Inputs/outputs (pipeline convention, not enforced by the CLI):

- Pipeline commonly uses input ADM XML at `processedData/currentMetaData.xml`.
- Pipeline commonly writes output LUSID to `processedData/stageForRender/scene.lusid.json`.
- CLI accepts any `--in` and `--out` paths and does not enforce these locations.

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
- Future plan (ingest side): add a flag for improved detection (e.g. speakerLabel or bed-aware). Default must preserve Phase 2 behavior.

Ordering:

- Stable ordering is critical.
- Phase 2 ordering must match Python ordering exactly (see Section 5).

Deletion policy (pipeline-level):

- Any removal of the Python parser in the pipeline must be documented in Spatial Root docs.
- This repository does not enforce pipeline fallback or removal logic.

---

## 5. Stable Ordering Rules (Non-Negotiable)

Purpose:

- Downstream debugging depends on stable identity and ordering.
- Parity tests depend on ordering.
- The authoring/export path also requires deterministic ordering, even though it does not use Python parity.

### Phase 2 ingest ordering must match Python oracle

1. Direct speakers (beds) appear first in the `t=0` frame, in Python encounter order.
2. Objects appear after direct speakers, also in Python encounter order.
3. Frames are in ascending time order.
4. Nodes within each frame are appended in the same loop order Python produces.

Implementation constraints:

- If library traversal order differs from Python encounter order, implement explicit order preservation.
- Do not introduce sorted-by-id ordering in Phase 2.
- Do not “canonicalize” ordering in Phase 2.

### ADM authoring ordering rules (new)

The authoring path must define and preserve its own deterministic ordering policy.

Pinned default until a stricter profile rule is documented:

1. Direct-speaker / bed elements first.
2. LFE within the bed structure, not as a free object.
3. Audio objects after bed elements.
4. Source ordering must follow LUSID frame-zero node order unless the target ADM profile requires a documented override.
5. Any override required for target compatibility must be stated explicitly in docs and tests.

---

## 6. Parity, Equality, and Validation

### Phase 2 equality definition (hard fail)

- Parse Python output JSON and CULT output JSON.
- Deep-compare parsed JSON objects.
- Numbers compare by value (so 1 and 1.0 are equal).
- Hard fail on any mismatch.

Important:

- Because ordering is pinned to Python, ordering mismatches should not occur; treat them as failures.

Parity test requirements:

- Provide at least one real ADM export and at least one minimal fixture under `tests/parity/fixtures/`.
- Parity runner produces:
  - CULT output LUSID
  - Python output LUSID
  - Diff report on mismatch (structural path to first difference)

### ADM authoring validation requirements (new)

- Validate CLI arg combinations and required paths.
- Validate supported LUSID node types before writing output.
- Validate required WAV presence, mono/stem policy, sample-rate agreement, and duration agreement.
- Validate authored output structurally after generation.
- Prefer semantic tests over raw XML string equality.
- Manual import into Logic Pro Atmos is the acceptance target for the first implementation.

---

## 7. Report Schema (v0.1 baseline, extension allowed)

Report must include:

- `reportVersion` (e.g. `"0.1"` unless incremented deliberately)
- `tool`: `{ name, version, gitCommit }`
- `args`: full resolved CLI args/options used
- `status`: `"success" | "fail"`
- `errors[]`, `warnings[]`
- `summary`: `{ durationSec, sampleRate, timeUnit, numFrames, countsByNodeType }`
- `lossLedger[]`: required field even if empty
  - each entry: `{ kind, field, reason, count, examples[] }`

Report location:

- Default `<out>.report.json` for `transcode`
- For `adm-author`, default report path should be derived from the main output stem and documented explicitly when implemented
- Also printable to stdout via `--stdout-report`

No heavy hashing in v0.1 (no input SHA).

Authoring-specific report extensions may include:

- authored bed count
- authored object count
- authored channel count
- wav file count resolved
- validation failures or approximations specific to the authoring mapping

---

## 8. ADM Authoring Extension Scope (Pinned)

Purpose:

- Author an Atmos-compatible ADM XML file and ADM BW64 WAV from a canonical LUSID scene or package.
- Keep CULT scene-first: authoring is an export target layered above the canonical model.

### Supported first-pass input contract

Must support both:

1. LUSID package folder containing:
   - `scene.lusid.json`
   - referenced mono WAV assets

2. Explicit inputs:
   - `scene.lusid.json`
   - WAV directory

### First-pass output contract

- `export.adm.xml`
- `export.adm.wav`
- report JSON

### First-pass target

- Logic Pro Atmos import is the must-pass practical target.
- The first implementation is not required to solve every ADM authoring possibility.
- Unsupported or approximate mappings must be surfaced in `lossLedger[]`.

### First-pass authoring subset

Must be specified in code/docs before broad implementation assumptions are made:

- supported LUSID node types
- mapping of `direct_speaker`, `audio_object`, and `LFE`
- time/automation strategy from frame-based LUSID data to authored ADM representation
- deterministic ID policy for authored ADM entities
- WAV assumptions (mono, sample rate, duration agreement)
- precise handling of unsupported semantics

### Non-goals for first pass

- replacing the current ADM ingest/parity path
- generic DAW authoring abstraction
- renderer-side bass management
- venue-specific final render policy
- silent invention of unsupported motion or metadata semantics

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

- Phase targeted
- Existing parity status unchanged (yes/no)
- Existing ingest ordering preservation confirmed (yes/no)
- `timeUnit` is unchanged for current ingest path (yes/no)
- Existing LFE ingest behavior unchanged unless explicitly intended (yes/no)
- Docs updated (list exact files)
- Report schema/version impact described (yes/no)

For ADM authoring PRs, also include:

- authoring subset defined in writing (yes/no)
- supported node types listed (yes/no)
- WAV validation policy documented (yes/no)
- deterministic authoring ordering confirmed (yes/no)
- atomic XML and BW64 writes confirmed (yes/no)
- loss-ledger behavior for unsupported mappings confirmed (yes/no)
- Logic Pro Atmos manual validation attempted (yes/no; if no, state explicitly)

No merge if:

- existing parity regresses
- existing ingest ordering rules drift
- report atomic write regresses
- authoring outputs are non-atomic
- docs are not updated for behavior changes
- unsupported authoring behavior is silently invented instead of reported
