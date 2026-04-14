# AGENTS.md — CULT Transcoder (Execution-Safe, Contract-First)

## Onboarding Tasks (New Agent)

### Context Reset Prompt (Paste Into New Window)

```
You are continuing CULT Transcoder adm-author work. Resampling + validation are implemented and must not regress the parity-critical adm_xml->lusid_json ingest path. r8brain is a git submodule at cult_transcoder/thirdparty/r8brain and used by src/audio/resampler_r8brain.cpp. WAV I/O is a self-contained RIFF parser in src/audio/wav_io.cpp (no libsndfile). adm-author now normalizes mono WAVs to 48 kHz float32, validates strict equal frame counts, and checks scene duration if present. admAuthor() validates and normalizes but returns "not implemented yet" at the XML/BW64 write step — that is Step 3-4 work. Report schema v0.1 is preserved. Remaining work: LUSID->ADM mapping, ADM XML + BW64 output, and tests for mapping/output. Keep atomic output rules and fail-report behavior.
```

Goal: implement the new export-side `adm-author` path without touching the existing parity-critical `transcode` path.

Scope and constraints:

- Do not modify or regress the existing `adm_xml -> lusid_json` ingest path.
- Use `libadm` and `libbw64` only for export-side authoring.
- LUSID is canonical. Authoring is layered on top and must preserve deterministic ordering.
- Resampling policy: normalize mono WAVs to 48 kHz float32 in the authoring path only.
- Duration policy: use normalized frame count as canonical; fail on mismatches.
- `containsAudio.json` is deprecated; resolve WAVs by deterministic id-to-filename mapping.

Step-by-step tasks:

1. Implement resampling (per internalDocsMD/resamplingPlan.md)

- Vendor r8brain-free-src, wrap in a small internal API.
- Normalize only non-48 kHz mono inputs to 48 kHz float32.
- Report resampling details in the report (source rate, target rate, files).

2. Implement post-normalization validation

- Validate strict equal frame counts across normalized WAVs.
- If `scene.lusid.json` includes `duration`, validate against audio-derived duration.
- On mismatch, fail authoring and report expected vs actual counts per file.

Status update (2026-04-13): Steps 1-2 are implemented and building correctly after merge reconciliation; steps 3-6 remain. The adm-author pipeline validates inputs, normalizes WAVs, and validates frame counts, but returns "not implemented yet" at the XML/BW64 write step. test_adm_author_args.cpp covers CLI validation and is wired into the build.

3. Build authoring mapping layer (LUSID -> ADM model)

- Supported node types: direct_speaker, audio_object, LFE.
- Deterministic ordering: beds (1.1,2.1,3.1,4.1,5.1..10.1), then objects by id.
- Motion: step-hold, one audioBlockFormat per LUSID frame with rtime + duration.

4. Implement ADM XML + BW64 output

- Write XML and BW64 via temp + rename.
- Embed ADM metadata in BW64 with libbw64.

5. Extend report schema usage (no breaking changes)

- Keep v0.1 fields intact.
- Add authoring-specific summary fields as needed.
- Add structured validation/resampling details in a new section if required.

6. Tests

- CLI arg validation for adm-author.
- Missing files, unsupported formats, sample-rate and duration mismatches.
- Mapping tests for bed/object/LFE counts and ordering.
- Integration test that emits ADM XML + BW64 with embedded metadata.
- Ensure no regressions in existing parity tests.

Repo placement: `spatialroot/cult-transcoder/` (git submodule, already wired).  
Invocation: CLI-first. Spatial Root calls `spatialroot/cult-transcoder/build/cult-transcoder` (macOS/Linux) or the `.bat` wrapper on Windows (see §9).  
Build: CMake. C++17. Cross-platform (macOS + Windows required; Linux later).  
Test framework: Catch2 (via CMake FetchContent; no install required).  
License: Apache-2.0. All source files carry an Apache-2.0 header with Cult-DSP copyright.  
Canonical: LUSID Scene v0.5 JSON.

This file is designed to be executable and non-destructive. Do not guess.

Last verified against the submodule code: 2026-04-13.  
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

## 0.3. Current Implementation Snapshot (Verified 2026-04-13)

Observed in the current submodule (no assumptions beyond code/tests in this repo):

**Ingest/parity path (complete):**
- ADM XML → LUSID JSON conversion is implemented in `src/adm_to_lusid.cpp`.
- CLI supports `adm_xml` and `adm_wav` → `lusid_json` (see `src/main.cpp`, `src/transcoder.cpp`).
- XML parser is pugixml (FetchContent); libadm is not used in the ingest/parity path.
- Phase 4 ADM profile resolver + `--lfe-mode` flag: `transcoding/adm/adm_profile_resolver.cpp`.
- Phase 6 Sony 360RA conversion: `transcoding/adm/sony360ra_to_lusid.cpp`.
- Parity tests exist in `tests/parity`; 65/65 tests passing as of Phase 6A.
- Report schema v0.1 in `include/cult_report.hpp` + `src/report.cpp`.
- Report written via temp + rename (atomic). LUSID output is non-atomic (known).

**adm-author path (Steps 1-2 functional, Steps 3-6 pending):**
- `src/adm_author.cpp` — validates inputs and orchestrates the pipeline; returns `"not implemented yet"` at the XML/BW64 write step.
- `src/lusid_reader.cpp` — complete minimal LUSID JSON parser (nlohmann/json, FetchContent).
- `src/audio/wav_io.cpp` — self-contained RIFF WAV reader/writer; no libsndfile dependency.
- `src/audio/normalize_audio.cpp` — 48 kHz float32 normalization coordinator; validates frame counts.
- `src/audio/resampler_r8brain.cpp` — r8brain-backed mono resampling; r8brain at `thirdparty/r8brain` (git submodule).
- `tests/test_adm_author_args.cpp` — CLI validation tests (3 tests; all designed to pass with current stub).

**Skeleton files (copyright header only — not compiled):**
- `transcoding/adm/adm_to_lusid.cpp` — placeholder for future orchestration layer.
- `transcoding/lusid/lusid_validate.cpp` — placeholder for Phase 3+ schema validation.
- `transcoding/lusid/lusid_writer.cpp` — placeholder for Phase 3+ atomic writer.

Known doc mismatch to fix later:

- `README.md` (formerly `OVERVIEW.md`) phase table may reference stale phase numbering.

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
| JSON parser (authoring path)      | nlohmann/json v3.11.3 (FetchContent) — used by `src/lusid_reader.cpp` only; not in ingest path                                                   |
| ADM authoring library             | `libadm` may be introduced for the new LUSID → ADM export path only                                                                               |
| BW64 packaging library            | `libbw64` (git submodule, `thirdparty/libbw64`)                                                                                                   |
| Resampling library                | r8brain-free-src (git submodule, `thirdparty/r8brain`) — authoring path only                                                                      |
| WAV I/O                           | Self-contained RIFF parser in `src/audio/wav_io.cpp` — no libsndfile dependency                                                                   |
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
├── .gitmodules
├── CMakeLists.txt
├── README.md
├── internalDocsMD/
│   ├── AGENTS-CULT.md
│   ├── DEV-PLAN-CULT.md                # outdated; do not use as source of truth
│   ├── DESIGN-DOC-V1-CULT.MD
│   └── design-reference-CULT.md
├── include/
│   ├── adm_to_lusid.hpp
│   ├── cult_transcoder.hpp             # AdmAuthorRequest / AdmAuthorResult defined here
│   ├── cult_report.hpp
│   ├── cult_version.hpp
│   ├── lusid_reader.hpp
│   └── audio/
│       ├── wav_io.hpp
│       ├── resampler.hpp
│       └── normalize_audio.hpp
├── scripts/
│   └── cult-transcoder.bat
├── src/
│   ├── main.cpp
│   ├── transcoder.cpp
│   ├── adm_to_lusid.cpp
│   ├── report.cpp
│   ├── adm_author.cpp                  # adm-author entry point (Steps 1-2 functional)
│   ├── lusid_reader.cpp                # LUSID JSON parser (nlohmann/json)
│   └── audio/
│       ├── wav_io.cpp                  # self-contained RIFF reader/writer (no libsndfile)
│       ├── normalize_audio.cpp         # normalization coordinator
│       └── resampler_r8brain.cpp       # r8brain-backed mono resampling
├── thirdparty/
│   ├── libbw64/                        # git submodule — EBU BW64 container (Phase 3+)
│   └── r8brain/                        # git submodule — r8brain resampler (adm-author)
├── transcoding/
│   ├── adm/
│   │   ├── adm_reader.cpp              # Phase 3: BW64 axml extraction
│   │   ├── adm_reader.hpp
│   │   ├── adm_to_lusid.cpp            # skeleton — not compiled
│   │   ├── adm_profile_resolver.cpp    # Phase 4: ADM profile detection
│   │   ├── adm_profile_resolver.hpp
│   │   └── sony360ra_to_lusid.cpp      # Phase 6: Sony 360RA ADM → LUSID
│   └── lusid/
│       ├── lusid_writer.cpp            # skeleton — not compiled
│       └── lusid_validate.cpp          # skeleton — not compiled
└── tests/
    ├── test_report.cpp
    ├── test_cli_args.cpp
    ├── test_lfe_mode.cpp               # Phase 4: LFE mode + profile detection
    ├── test_360ra.cpp                  # Phase 6: Sony 360RA conversion
    ├── test_adm_author_args.cpp        # adm-author CLI validation (3 tests)
    ├── test_lusid_to_adm_mapping.cpp   # planned: Step 3
    ├── test_adm_author_integration.cpp # planned: Step 4
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

- `0`: success; final output exists; report exists.
- non-zero: failure; final output must not exist; report should exist best-effort; stderr must explain.
- Report is written via temp + rename.
- Existing LUSID output remains non-atomic until separately fixed.
- `adm-author` outputs (`export.adm.xml`, `export.adm.wav`) must be written via temp + rename from day one.

Atomic output rule (non-negotiable):
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

## 11. Phase 4 — Implementation Notes (COMPLETE)

**Phase 4 is fully implemented as of 2026-03-07. 40/40 tests pass.
These notes are preserved for archaeology and future agent context.**

### Agent research delegation rule (applies to all phases)

Agents must **not** attempt brute-force or token-intensive research into
external format specifications, third-party schema conventions, or toolchain
internals they cannot fully resolve from files already in the workspace.
If you hit a question that requires knowledge of an external standard (e.g.,
Sony 360RA ADM schema conventions, MPEG-H MAE metadata semantics, IAMF OBU
structure), **stop and ask the owner**. The owner handles external research and
will return with the answer. Document the open question in §11.9 so it is
visible and trackable.

These notes exist so the next agent has enough context to implement Phase 4
without a clarifying Q&A round. Read all of §11 before touching any file.

---

### 11.1 What Phase 4 actually is (precise scope)

Phase 4 has two separable sub-tasks that should be implemented together because
they share the same XML inspection pass:

**Sub-task A — ADM profile detection**
Inspect the parsed `pugi::xml_document` (already in memory after axml extraction
or file load) and return a `AdmProfile` enum value. The converter uses this to
adjust extraction behavior. In Phase 4, the only behavioral change driven by
profile is LFE detection (sub-task B). Future phases may use profile for bed
channel remapping, Sony 360RA ID normalisation, etc.

**Sub-task B — Improved LFE detection, behind a flag**
The Phase 2 hardcoded rule (4th DirectSpeaker encountered, 1-based) must remain
the **default** forever. Phase 4 adds a new opt-in mode that uses `speakerLabel`
attribute inspection. The flag is a new CLI argument; it must not change any
existing behaviour when absent.

These two sub-tasks live entirely inside `transcoding/adm/adm_profile_resolver.cpp`
(and `adm_profile_resolver.hpp`) plus a small addition to `src/transcoder.cpp` to pass the
flag through. `adm_to_lusid.cpp` also received a `lfeMode` param to implement the
`SpeakerLabel` branch — see §11.4.
override is applied at the `LusidNode` construction layer inside
`parseAdmDocument()` only if the flag is explicitly set.

---

### 11.2 Pinned CLI contract for the new flag

The flag name and semantics are pinned here to prevent an agent from inventing
something incompatible:

```
--lfe-mode <value>
```

| Value           | Behaviour                                                            | Default? |
| --------------- | -------------------------------------------------------------------- | -------- |
| `hardcoded`     | LFE = 4th DirectSpeaker encountered (1-based). Phase 2 behaviour.    | **YES**  |
| `speaker-label` | LFE = any DirectSpeaker whose `speakerLabel` is `"LFE"` or `"LFE1"`. | no       |

Rules:

- If `--lfe-mode` is omitted the behaviour is identical to `--lfe-mode hardcoded`.
- The flag is only meaningful for `--in-format adm_xml` and `--in-format adm_wav`.
  If supplied with any other future in-format, emit a warning to the report and ignore.
- The resolved value of `--lfe-mode` **must appear in the report `args` block**
  (so the report is always self-describing).
- Non-default LFE mode **must produce a `warnings[]` entry** in the report:
  `"lfe-mode=speaker-label: LFE assignment differs from Phase 2 hardcoded default"`
- Parity tests using `--lfe-mode hardcoded` (or omitted) must still pass 28/28.

Do **not** add `--lfe-mode` to `kKnownInFormats` — it is a separate top-level
argument. Add it to `TranscodeRequest` in `cult_transcoder.hpp`.

---

### 11.3 Pinned API shape for adm_profile_resolver

The resolver must expose this public API (in a new `adm_profile_resolver.hpp`):

```cpp
namespace cult {

enum class AdmProfile {
    Generic,       // Standard EBU BS.2076, no tool-specific extensions detected
    DolbyAtmos,    // audioProgrammeName contains "Atmos" (case-insensitive), or
                   // audioPackFormatName contains "Atmos" or "51" + objects pattern
    Sony360RA,     // audioPackFormatName contains "360RA" or "360" (case-insensitive),
                   // OR audioProgrammeName contains "360RA"
    Unknown,       // XML present but no profile signals found — treat as Generic
};

struct ProfileResult {
    AdmProfile profile = AdmProfile::Unknown;
    std::string detectedFrom;   // human-readable: which attribute triggered detection
    std::vector<std::string> warnings;
};

/// Inspect an already-parsed pugi::xml_document and return the detected profile.
/// This is a read-only pass — does not modify the document.
/// Called by transcoder.cpp after extractAxmlFromWav() or before convertAdmToLusid().
ProfileResult resolveAdmProfile(const pugi::xml_document& doc);

} // namespace cult
```

Detection heuristics (implement exactly these, in priority order):

1. Search all `audioProgramme` nodes for `audioProgrammeName` attribute.
   - If value contains `"Atmos"` (case-insensitive) → `DolbyAtmos`
   - If value contains `"360RA"` (case-insensitive) → `Sony360RA`
2. Search all `audioPackFormat` nodes for `audioPackFormatName` attribute.
   - If value contains `"Atmos"` (case-insensitive) → `DolbyAtmos`
   - If value contains `"360RA"` (case-insensitive) → `Sony360RA`
3. If no signals found → `Unknown` (caller treats as `Generic`)

**Note:** bare `"360"` is intentionally excluded as a detection signal — it is
too broad and would produce false positives. Only `"360RA"` is used. The real
Sony fixture (`audioProgrammeName="Gem_OM_360RA_3"`) matches on `"360RA"`.
Do not re-add `"360"` without an explicit owner decision.

**Do not** use libadm for this pass. pugixml is already available and the
detection is purely attribute-string inspection — no ADM semantic model needed.

---

### 11.4 How the profile and LFE flag wire into existing code

The call chain in `transcoder.cpp` becomes:

```
adm_wav path:
  extractAxmlFromWav()
    → resolveAdmProfile(doc)          ← NEW Phase 4 call
    → convertAdmToLusidFromBuffer(xmlBuffer, lfeMode)   ← lfeMode added

adm_xml path:
  doc.load_file()
    → resolveAdmProfile(doc)          ← NEW Phase 4 call
    → convertAdmToLusid(xmlPath, lfeMode)               ← lfeMode added
```

`parseAdmDocument()` in `adm_to_lusid.cpp` gains one new parameter:

```cpp
static ConversionResult parseAdmDocument(
    pugi::xml_document& doc,
    LfeMode lfeMode = LfeMode::Hardcoded   // new, default = Hardcoded
);
```

Where `LfeMode` is a simple enum in `adm_to_lusid.hpp`:

```cpp
enum class LfeMode { Hardcoded, SpeakerLabel };
```

The `speaker-label` branch inside `parseAdmDocument()`:

- When `lfeMode == SpeakerLabel`: a DirectSpeaker node is typed `LFE` if its
  `speakerLabel` child element text equals `"LFE"` or `"LFE1"` (case-insensitive).
  The hardcoded channel-4 rule is **not applied** in this branch.
- When `lfeMode == Hardcoded` (default): existing Phase 2 logic unchanged.

The profile result is stored in `ConversionResult::warnings` if Sony360RA or
DolbyAtmos is detected (for report transparency), but does **not** change
conversion behaviour in Phase 4 beyond the LFE mode. Full profile-driven
conversion is Phase 5+.

---

### 11.5 CMake changes (done)

`adm_profile_resolver.cpp` is **implemented and compiled**. Both targets
(`cult-transcoder` and `cult_tests`) include it. `adm_profile_resolver.hpp`
is in `transcoding/adm/` which is already on the include path.

---

### 11.6 Test requirements (done — see `tests/test_lfe_mode.cpp`)

All 12 Phase 4 tests are implemented in `tests/test_lfe_mode.cpp`.
The full suite runs as 40 test cases / 150 assertions (was 28/105).

| Test                                                                      | Input                            | Expected                                                                                                  |
| ------------------------------------------------------------------------- | -------------------------------- | --------------------------------------------------------------------------------------------------------- |
| `--lfe-mode hardcoded` explicit = same as default                         | parity fixture                   | passes parity                                                                                             |
| `--lfe-mode speaker-label` on fixture with `speakerLabel="LFE"`           | new fixture (see below)          | node typed `LFE`                                                                                          |
| `--lfe-mode speaker-label` on Atmos fixture (LFE at ch4, no speakerLabel) | existing Atmos fixture           | ch4 node typed differently if no speakerLabel, or same if speakerLabel present — document expected result |
| `--lfe-mode garbage`                                                      | any                              | non-zero exit, `status: "fail"` report                                                                    |
| Profile detection: Atmos XML → `detectedFrom` correct                     | Atmos fixture                    | `DolbyAtmos` detected                                                                                     |
| Profile detection: generic XML → `Generic` or `Unknown`                   | existing parity fixture          | `Unknown` or `Generic`                                                                                    |
| Profile detection: 360RA XML → `Sony360RA` detected                       | `sony_360ra_example.xml` fixture | `Sony360RA` detected via `audioProgrammeName="Gem_OM_360RA_3"` (contains `"360RA"`)                       |
| Parity regression with default lfe-mode                                   | parity fixtures                  | 28/28 still pass                                                                                          |

**Fixtures needed** (must be committed to `tests/parity/fixtures/`):

- `sony_360ra_example.xml` — copy of `processedData/sony360RA_example.xml`
  (already confirmed present in the workspace). Do not modify the source file.
  Use this file directly for profile detection tests; its `audioProgrammeName`
  is `"Gem_OM_360RA_3"` which contains `"360RA"` — the unambiguous detection
  signal. **No Python oracle reference LUSID is committed for this fixture.**
  The Python oracle cannot parse Sony 360RA ADM (`<audioFormatExtended>` root)
  and is not a valid baseline. Sony 360RA support was never in the oracle scope.

- `sony_360ra_example.xml` is used **only** for Phase 4 profile detection tests
  (assert `Sony360RA` is returned by `resolveAdmProfile()`). It is **not** used
  for LUSID conversion parity in Phase 4. Full 360RA conversion support is a
  later phase (see §11.9 open questions).

- The existing `speakerLabel`-based LFE test needs a minimal synthetic fixture
  (a small hand-authored ADM XML with a DirectSpeaker element that has a
  `<speakerLabel>LFE</speakerLabel>` child). Craft this fixture in-repo at
  `tests/parity/fixtures/lfe_speaker_label_fixture.xml`. Do not derive it from
  the Sony 360RA file (that file has no LFE/speakerLabel elements — confirmed
  by inspection).

---

### 11.7 Report contract additions

`lfeMode` must be added to `ResolvedArgs` in `cult_report.hpp` so it is copied
from `TranscodeRequest` alongside all other args before transcoding begins.
This mirrors the existing pattern: `inPath`, `inFormat`, etc. are already on
`ResolvedArgs` and serialized verbatim. `lfeMode` follows the same path —
`TranscodeRequest` → copy into `Report::args` → serialized to JSON. The field
name in the JSON report args block is `"lfeMode"` (camelCase).

Profile detection results must **always** produce a `warnings[]` entry,
unconditionally — even when the detected profile does not change conversion
behavior (e.g., `DolbyAtmos` detected but `--lfe-mode hardcoded` in use). The
report is a transparency artifact for the pipeline and renderer; it must reflect
everything that was observed, not just what changed.

No other report schema changes in Phase 4. `reportVersion` stays `"0.1"`.

---

### 11.8 Files to touch in Phase 4 (complete list)

| File                                                       | Change                                                                                               |
| ---------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| `transcoding/adm/adm_profile_resolver.hpp`                 | **NEW** — ProfileResult, AdmProfile enum, resolveAdmProfile()                                        |
| `transcoding/adm/adm_profile_resolver.cpp`                 | **IMPLEMENT** stub → full detection logic                                                            |
| `include/adm_to_lusid.hpp`                                 | Add `LfeMode` enum; add `lfeMode` param to `convertAdmToLusid()` and `convertAdmToLusidFromBuffer()` |
| `src/adm_to_lusid.cpp`                                     | Add `lfeMode` param to `parseAdmDocument()`; add `SpeakerLabel` branch                               |
| `include/cult_transcoder.hpp`                              | Add `lfeMode` field to `TranscodeRequest`                                                            |
| `include/cult_report.hpp`                                  | Add `lfeMode` field to `ResolvedArgs` (copied from `TranscodeRequest` before transcoding)            |
| `src/transcoder.cpp`                                       | Parse `--lfe-mode` arg; pass to `resolveAdmProfile()` + converter calls                              |
| `src/main.cpp`                                             | Copy `lfeMode` into `report.args`; profile detection warnings into `report.warnings`                 |
| `src/report.cpp`                                           | Serialise `lfeMode` in the `args` JSON block                                                         |
| `CMakeLists.txt`                                           | Add `adm_profile_resolver.cpp` to both targets                                                       |
| `tests/test_cli_args.cpp` or new `tests/test_lfe_mode.cpp` | New Phase 4 tests (see §11.6)                                                                        |
| `tests/parity/fixtures/`                                   | Add `sony_360ra_example.xml` + synthetic `lfe_speaker_label_fixture.xml`                             |
| `internalDocsMD/AGENTS-CULT.md`                            | Update §1 repo layout, §2 CLI contract, §11 status                                                   |
| `internalDocsMD/DEV-PLAN-CULT.md`                          | Mark Phase 4 work items complete                                                                     |
| `spatialroot/internalDocsMD/AGENTS.md`                     | Note `--lfe-mode` flag exists, update binary contract if needed                                      |

**Do not touch**: `transcoding/lusid/lusid_writer.cpp`, `lusid_validate.cpp`
(Phase 5+ stubs), `runPipeline.py` (deprecated), any LUSID Python files.

---

### 11.9 Open questions — ask the owner, do not guess

These questions require external research or owner judgment. The agent must
**stop and ask** rather than guess or implement speculatively. Document answers
here once resolved.

1. **Sony 360RA conversion scope (RESOLVED — Phase 4 scope is detection only)**
   Confirmed: Phase 4 only detects the 360RA profile and reports it. Full
   360RA ADM-to-LUSID conversion (the `<audioFormatExtended>` root structure)
   is out of scope for Phase 4. The Sony fixture is used only for profile
   detection tests. The Python oracle does not support 360RA and is not used
   as a reference for this format.

2. **Sony 360RA speakerLabel / LFE convention (RESOLVED — not applicable in Phase 4)**
   Confirmed by file inspection: `processedData/sony360RA_example.xml` contains
   no `speakerLabel` elements and no LFE channels. It is a 13-channel object-only
   360RA file (no DirectSpeakers bed). The `--lfe-mode speaker-label` branch
   therefore cannot be tested against this file; a synthetic fixture is used
   instead (see §11.6).

   **Bass management note (owner-confirmed, 2026-03-07):** Sony 360RA has no
   LFE channel by design. Bass management is the responsibility of the playback
   engine, not the authoring tool. Spatial Root v1 does **not** implement bass
   management. This is intentional and must not be worked around in CULT.
   A future exploration item exists for Spatial Root v2 (see DEV-PLAN Phase 6
   note). Do not add any bass management logic without an explicit owner decision.

3. **Full 360RA ADM parsing strategy (RESOLVED — Phase 6)**
   The Sony 360RA ADM XML root is `<conformance_point_document><File><aXML><format><audioFormatExtended version="ITU-R_BS.2076-2">`.
   This is a bwfmetaedit export wrapper around a standard BS.2076-2 document. The existing
   `parseTimecode()` with `sscanf` already handles the `S48000` sample-frame suffix correctly
   (stops parsing at `S`). Full conversion is implemented in Phase 6 as a separate converter
   module (`sony360ra_to_lusid.*`) — see §15. Auto-dispatch via profile detection.

4. **`--lfe-mode` exposure in `runRealtime.py` (OPEN)**
   Should the Python pipeline ever pass `--lfe-mode speaker-label` to
   cult-transcoder, or is this flag only for CLI / testing use? If the pipeline
   should expose it, `RealtimeConfig` in `realtime_runner.py` needs a new field.
   Ask the owner before wiring this into the pipeline.

5. **Profile-driven bed remapping in Phase 5+ (RESOLVED — not needed for 360RA)**
   The Sony 360RA fixture has no DirectSpeakers bed — all 13 channels are Objects
   (`typeDefinition="Objects"`, `AP_0003XXXX`). No bed remapping is required for 360RA.
   For Dolby Atmos, bed remapping remains an open question for a future phase.

---

## 13. Phase 5 — GUI Transcoding Tab (COMPLETE, 2026-03-07)

Phase 5 adds a **TRANSCODE tab** to the existing realtime GUI (`gui/realtimeGUI/`).
It exposes the `cult-transcoder transcode` CLI to users without requiring a terminal.
The C++ binary is unchanged; all new code is Python/PySide6 only.

### 13.1 Scope

- Offline ADM → LUSID transcoding from the GUI (not real-time engine)
- Exposes `--in`, `--in-format`, `--out-format lusid_json`, `--report`, `--lfe-mode` flags
- Streams `cult-transcoder` stdout/stderr live into the GUI log
- "Open Report" button to view the JSON report after a successful run
- Cross-platform file dialog pattern (see §13.4)

### 13.2 New files

**`gui/realtimeGUI/realtime_panels/RealtimeTranscodePanel.py`** — PySide6 panel for the TRANSCODE tab.

Signals:

- `run_requested(in_path: str, in_format: str, out_path: str, lfe_mode: str)` — emitted when TRANSCODE button clicked
- `open_report_requested(report_path: str)` — emitted when Open Report button clicked

Public API:

- `set_running(running: bool)` — disables inputs + shows spinner state
- `set_finished(success: bool, report_path: str)` — re-enables inputs, colours status, reveals Open Report
- `append_line(text: str)` — appends one line to the streaming log widget

Browse pattern (two separate buttons per §13.4):

- `File…` button → `QFileDialog.getOpenFileName(filter="ADM Files (*.wav *.xml);;All Files (*)")`
- `Dir…` button → `QFileDialog.getExistingDirectory()`

Combos:

- In-format: `auto`, `adm_wav`, `adm_xml`, `lusid_json`
- LFE mode: `hardcoded` (default), `speaker-label`

---

**`gui/realtimeGUI/realtime_transcoder_runner.py`** — `QProcess` wrapper.

Signals:

- `output(str)` — one line of stdout/stderr
- `started()` — process started
- `finished(int, str)` — (exit_code, report_path)

Binary resolution (`_resolve_binary()`):

```python
project_root / "cult_transcoder" / "build" / "cult-transcoder"      # macOS/Linux
project_root / "cult_transcoder" / "build" / "cult-transcoder.exe"  # Windows
```

`project_root` is resolved as `Path(__file__).resolve().parents[3]` (3 levels up from `gui/realtimeGUI/`).

Command constructed:

```
cult-transcoder transcode
  --in         <in_path>
  --in-format  <in_format>
  --out        <out_path>
  --out-format lusid_json
  --report     <out_path>.report.json
  --lfe-mode   <lfe_mode>
```

`out_path` defaults to `processedData/stageForRender/scene.lusid.json` (relative to project root) if not specified by the user.

Drains stdout and stderr on `_on_finished()` before emitting `finished`.

### 13.3 Modified files

**`gui/realtimeGUI/realtimeGUI.py`**:

- `RealtimeWindow` now wraps a `QTabWidget` (`self._tabs`) with two tabs: **ENGINE** (all original 4 panels) and **TRANSCODE** (`RealtimeTranscodePanel` in its own scroll area)
- `self._tc_runner = RealtimeTranscoderRunner(project_root)` instantiated in `__init__`
- Signal wiring in `_connect_runner()`: `tc_runner.output → panel.append_line`, `tc_runner.started → panel.set_running(True)`, `tc_runner.finished → _on_transcode_finished`
- Handlers: `_on_run_transcode()` (starts QProcess), `_on_transcode_finished(exit_code, report_path)`

**`gui/realtimeGUI/realtime_panels/theme.py`** (font system overhaul):

- Added `_UI_FONT_FAMILY = "Courier New"` constant
- Added `ui_font(size: int = 8) -> QFont` helper — `QFont("Courier New", size + 2)` with `StyleHint.TypeWriter`. **No bold/DemiBold.**
- QSS base rule: `QWidget { font-family: 'Courier New', 'Courier', monospace; }`
- All QSS font sizes bumped +2pt: `7pt→9pt`, `8pt→10pt`, `9pt→10pt`, `12pt→13pt`
- Added `QTabWidget` / `QTabBar` styles for ENGINE/TRANSCODE tab appearance

**`gui/realtimeGUI/realtime_panels/RealtimeInputPanel.py`**:

- Replaced single `Browse` button with two: `File…` (getOpenFileName) + `Pkg…` (getExistingDirectory — for LUSID packages)
- `set_enabled_for_state()` updated to include both new buttons
- `QFont("Space Mono", N)` → `ui_font(N)` throughout

All other panel files (`RealtimeLogPanel.py`, `RealtimeControlsPanel.py`, `RealtimeTransportPanel.py`):

- `QFont("Space Mono", N)` → `ui_font(N)` throughout; broken sed-fused import lines fixed

### 13.4 Cross-platform file dialog pattern (pinned)

macOS `NSOpenPanel` cannot mix file and directory selection in a single dialog.
The two-button pattern is the cross-platform solution used everywhere in this codebase:

| Button label     | Dialog call                             | Used for                                    |
| ---------------- | --------------------------------------- | ------------------------------------------- |
| `File…`          | `QFileDialog.getOpenFileName(...)`      | ADM WAV / ADM XML input                     |
| `Dir…` or `Pkg…` | `QFileDialog.getExistingDirectory(...)` | LUSID package directory or output directory |

Do **not** use `QFileDialog.getOpenFileUrl` with `ShowDirsOnly` or try to combine file+dir selection in a single call — this is broken on macOS and inconsistent on Windows/Linux.

### 13.5 Open questions

| #   | Question                                                                                                           | Status                                                                       |
| --- | ------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------- |
| 1   | Should `runRealtime.py` ever pass `--lfe-mode speaker-label` to cult-transcoder, or is this flag CLI/testing only? | **OPEN** — ask owner (§11.9 Q4)                                              |
| 2   | Output path for GUI transcode: always `stageForRender/scene.lusid.json`, or user-configurable?                     | Currently defaults to stageForRender; user can override in the TRANSCODE tab |

---

## 15. Phase 6 — Sony 360RA ADM → LUSID Conversion (IN PROGRESS, 2026-03-07)

Phase 6 adds a dedicated converter for Sony 360RA ADM XML exports, dispatched
automatically when the profile resolver detects `Sony360RA`. The C++ binary
CLI contract is **unchanged** — no new flags, no new `--in-format` values.

---

### 15.1 Scope

- Convert Sony 360RA ADM XML (ADM-variant mode, bwfmetaedit export) → LUSID Scene JSON
- Auto-dispatch: profile detection (`Sony360RA`) routes to new converter transparently
- 13 audio objects, all `typeDefinition="Objects"`, no DirectSpeakers bed, no LFE
- Static position extraction: first non-muted block per object (see §15.3)
- Polar → Cartesian coordinate conversion (see §15.5)
- LUSID node IDs: `1.1` through `13.1` (encounter order of leaf audioObjects)
- MPEG-H 360RA mode: **deferred** — see §15.9

### 15.2 New files

| File                                                    | Purpose                                                       |
| ------------------------------------------------------- | ------------------------------------------------------------- |
| `transcoding/adm/sony360ra_to_lusid.hpp`                | Public API: `convertSony360RaToLusid()`                       |
| `transcoding/adm/sony360ra_to_lusid.cpp`                | Full implementation                                           |
| `tests/test_360ra.cpp`                                  | Phase 6 tests (polar→cart unit tests + structural invariants) |
| `tests/parity/fixtures/sony_360ra_reference.lusid.json` | Reference LUSID output for regression                         |

---

### 15.3 Sony 360RA XML structure (pinned — from fixture inspection)

**Root path:**

```
<conformance_point_document>
  <File>
    <aXML>
      <format>
        <audioFormatExtended version="ITU-R_BS.2076-2">
```

This is a bwfmetaedit export wrapper. The `<audioFormatExtended>` node is the
ADM root. Extract it by navigating: `conformance_point_document/File/aXML/format/audioFormatExtended`.

**Object structure:**

- 1 container `audioObject` (`AO_8001`) with `audioObjectIDRef` children — **skip this**.
- 13 leaf `audioObject` elements (`AO_1001`–`AO_100d`) each with exactly one `audioTrackUIDRef` and one `audioPackFormatIDRef`.
- Detection: a leaf audioObject has a `<audioTrackUIDRef>` child; a container has `<audioObjectIDRef>` children.

**`audioChannelFormat` → `audioBlockFormat` structure:**

```xml
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="L obj 1"
    typeDefinition="Objects" typeLabel="0003">
  <audioBlockFormat audioBlockFormatID="AB_00031001_00000001"
      rtime="00:00:00.00000S48000" duration="00:01:32.40449S48000">
    <cartesian>0</cartesian>
    <gain gainUnit="linear">0.999999</gain>
    <position coordinate="azimuth">29.999992</position>
    <position coordinate="elevation">0.000000</position>
    <position coordinate="distance">1.000000</position>
    <width>0.000000</width>
    <height>0.000000</height>
    <depth>0.000000</depth>
    <jumpPosition>0</jumpPosition>
  </audioBlockFormat>
  <!-- multiple blocks follow (gain-mute pattern) -->
```

**`<cartesian>0</cartesian>`** — always present, always 0 in 360RA exports. Means polar coords.

**Time format:** `HH:MM:SS.fffffS<sampleRate>` (e.g. `00:01:32.40449S48000`). The existing
`parseTimecode()` (`sscanf("%d:%d:%lf", ...)`) handles this correctly — `sscanf` stops at `S`.
**No changes to `parseTimecode()` are needed.**

---

### 15.4 Frame strategy: Option A — single static frame (PINNED)

The multi-block pattern in 360RA is a Sony gain-mute artifact, not keyframe animation.
Objects have a fixed spatial position; Sony inserts 0.01024s silence blocks at section
boundaries for content-routing reasons unrelated to spatial movement.

**Rule:** take the **first block whose `gain > 0`** for each object. Extract its polar
position and convert to Cartesian. Emit a single LUSID frame at `t=0`. All 13 nodes
are in this one frame.

Consequences:

- The LUSID scene has exactly **1 frame** for any 360RA file following this pattern.
- The gain-mute interleaving is silently discarded (not a lossy conversion for spatial data).
- This is the correct semantic: the "scene" of this 360RA mix is a static immersive bed.

**Future deviation:** if a future 360RA file is encountered with genuinely different positions
across blocks (animated objects), the converter will produce incorrect output. A future
"multi-block mode" should be considered then. Do NOT add it preemptively.

---

### 15.5 Gain field: OMITTED (PINNED)

The LUSID schema allows an optional `gain` field on `audio_object` nodes (default 1.0).
The Sony 360RA `audioBlockFormat` always contains `<gain gainUnit="linear">0.999999</gain>` (≈1.0).

**Rule:** the `gain` field is **not written** to LUSID output nodes in Phase 6.
The renderer assumes gain=1.0 for all nodes without an explicit gain field.

**⚠ Known future bug risk:** if a future 360RA file has meaningful per-object gain variation
(e.g. dialogue vs music ducking encoded in the gain field), CULT will silently discard it
and the renderer will play all objects at equal gain. If unexpected mix imbalances are observed
on a 360RA render, check the `gain` values in the source ADM and consider implementing gain
pass-through. Document any such finding in the loss ledger.

---

### 15.6 Container audioObject handling (PINNED)

The container `audioObject` (`AO_8001`) references 13 leaf objects via `<audioObjectIDRef>`.
It carries no position data and exists purely as a grouping/programme hierarchy artifact.

**Rule:** skip any `audioObject` that has at least one `<audioObjectIDRef>` child element.
Only process leaf objects that have a `<audioTrackUIDRef>` child.

**⚠ Known future limitation:** the container object may carry metadata (gain, importance,
dialogue kind) that applies to all children. In a future 360RA development pass, consider
whether container-level gain or mute state should be applied to leaf nodes. Document this
in §15.9 open questions for Phase 6+.

---

### 15.7 Polar → LUSID Cartesian conversion (PINNED)

ADM polar convention (BS.2076-2):

- `azimuth`: degrees, clockwise from front (0=front, +90=right, −90=left, ±180=back)
- `elevation`: degrees, 0=horizontal plane, +90=up, −90=down
- `distance`: meters (360RA always 1.0 = unit sphere)

LUSID Cartesian convention (schema `cart[x, y, z]`):

- `x`: right = +x
- `y`: front = +y
- `z`: up = +z

Conversion formula:

```
az_rad  = azimuth  * π/180
el_rad  = elevation * π/180

x = distance * sin(az_rad)  * cos(el_rad)   // right = +x ✓
y = distance * cos(az_rad)  * cos(el_rad)   // front = +y ✓
z = distance * sin(el_rad)                   // up    = +z ✓
```

This is the standard ADM-to-Cartesian formula. Do not invent an alternative.
Verified against LUSID schema `cart` description: `x: left(-)/right(+), y: back(-)/front(+), z: down(-)/up(+)`.

---

### 15.8 Dispatcher strategy: profile-based auto-dispatch (PINNED)

No new `--in-format` flag. When `resolveAdmProfile()` returns `Sony360RA`, the
dispatcher in `transcoder.cpp` automatically routes to `convertSony360RaToLusid()`
instead of `convertAdmToLusid()` / `convertAdmToLusidFromBuffer()`.

Both the `adm_xml` path and the `adm_wav` path check the profile after parsing.
The `pugi::xml_document` (already in memory) is passed directly to `convertSony360RaToLusid()`.

```
adm_xml path:
  doc.load_file()
    → resolveAdmProfile(doc)
    → if Sony360RA: convertSony360RaToLusid(doc)
    → else: convertAdmToLusid(xmlPath, lfeMode)

adm_wav path:
  extractAxmlFromWav()
    → doc.load_string(axmlResult.xmlData)
    → resolveAdmProfile(doc)
    → if Sony360RA: convertSony360RaToLusid(doc)
    → else: convertAdmToLusidFromBuffer(axmlResult.xmlData, lfeMode)
```

**`--lfe-mode` behaviour for 360RA:** The flag is irrelevant (360RA has no DirectSpeakers
or LFE), but if supplied it must not error. Emit a warning: `"lfe-mode flag has no effect on Sony360RA input"`.

---

### 15.9 Open questions — Phase 6+

| #   | Question                                                                 | Status                                             |
| --- | ------------------------------------------------------------------------ | -------------------------------------------------- |
| 1   | MPEG-H 360RA ingestion mode                                              | **DEFERRED** — see §15.10                          |
| 2   | Container audioObject metadata (gain, importance) — apply to leaf nodes? | **OPEN** — do not implement without owner decision |
| 3   | Multi-block animated objects in 360RA — future keyframe support?         | **OPEN** — do not implement without owner decision |
| 4   | Per-object gain pass-through from ADM `<gain>` to LUSID?                 | **OPEN** — see §15.5 bug risk note                 |

---

### 15.10 MPEG-H 360RA mode (DEFERRED)

Sony 360RA content exists in two authoring workflows:

1. **ADM-variant mode** (Phase 6 — THIS phase): Sony's Spatial Content Creation tool
   exports ADM metadata embedded in a BW64 WAV, or as a standalone ADM XML via bwfmetaedit.
   The ADM root is `<conformance_point_document>...<audioFormatExtended version="ITU-R_BS.2076-2">`.
   **This is what Phase 6 implements.**

2. **MPEG-H music format mode** (DEFERRED — future phase): Sony's Music Production pipeline
   produces 360RA content as an MPEG-H 3D Audio bitstream (`.mpf` / `.mhas` / `.m4f` container).
   This requires the MPEG-H decoder library (Ittiam or Fraunhofer) for MAE metadata extraction.
   Detection would be by container format and/or MPEG-H Audio Scene Information (MAEI) headers,
   not by ADM programme name. No implementation work should begin here without:
   - Owner-confirmed format samples (`.mpf` or `.mhas` fixture)
   - Owner decision on Ittiam vs Fraunhofer backend
   - A dedicated design doc section (DESIGN-DOC-V1-CULT.MD updated first)

**Do not conflate these two modes.** Phase 6 ONLY addresses mode 1 (ADM-variant).
Mode 2 follows after MPEG-H infrastructure is in place (see DEV-PLAN Phase 6 MPEG-H section).

---

## 14. Agent PR Checklist (Required)

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
