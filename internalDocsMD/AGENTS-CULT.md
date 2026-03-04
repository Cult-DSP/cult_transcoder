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
| BW64 reader          | libbw64 (git submodule at `thirdparty/libbw64`, Apache-2.0, header-only) — Phase 3. Used only for axml chunk extraction. **Not** FetchContent.    |
| Windows binary       | `build/cult-transcoder.exe` called via `scripts/cult-transcoder.bat` wrapper                                                                      |
| `.bat` wrapper rule  | Must be committed; every call-site in Spatial Root pipeline must have an inline comment explaining the indirection                                |
| Fail-report behavior | On any error (missing file, unsupported format, etc.) the CLI writes a best-effort `status: "fail"` report to the default path and exits non-zero |
| Atomic output        | All output files written to `<path>.tmp` first, then `std::filesystem::rename()` to final path. Temp cleaned on failure. Enforced Phase 2+.       |

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
_implemented_ files, fully compiled and linked. `adm_profile_resolver.cpp`,
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

| Component              | File                                                | Description                                                                                                                                                                                 |
| ---------------------- | --------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| libbw64 submodule      | `thirdparty/libbw64/`                               | EBU libbw64, Apache-2.0, header-only. Added as git submodule.                                                                                                                               |
| BW64 reader            | `transcoding/adm/adm_reader.hpp` + `.cpp`           | `extractAxmlFromWav()`: opens BW64 WAV, reads axml chunk, writes debug XML artifact, returns XML string in `AxmlResult`.                                                                    |
| Buffer-based converter | `include/adm_to_lusid.hpp` + `src/adm_to_lusid.cpp` | `convertAdmToLusidFromBuffer(xmlBuffer)`: parses ADM XML from in-memory string. Delegates to shared `static parseAdmDocument()` helper — zero logic duplication with `convertAdmToLusid()`. |
| adm_wav dispatch       | `src/transcoder.cpp`                                | `kKnownInFormats = {"adm_xml", "adm_wav"}`. When `adm_wav`: calls `extractAxmlFromWav` then `convertAdmToLusidFromBuffer`.                                                                  |
| Atomic LUSID write     | `src/transcoder.cpp`                                | Writes to `<out>.tmp` then `std::filesystem::rename()` to final path. Temp cleaned on failure.                                                                                              |
| Phase 3 tests          | `tests/test_cli_args.cpp`                           | 3 new tests: adm_wav format recognized, adm_xml regression, non-BW64 file fails at axml (not at format). Total: 10 tests.                                                                   |

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

## 11. Phase 4 — Pre-Flight Notes for Next Agent

**DO NOT BEGIN IMPLEMENTATION until the owner confirms testing of the Phase 3
pipeline is complete and any regressions are resolved.**

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
(and a new `.hpp`) plus a small addition to `src/transcoder.cpp` to pass the
flag through. Nothing in `adm_to_lusid.cpp` should change in Phase 4 — the LFE
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
   - If value contains `"360RA"` or `"360"` (case-insensitive) → `Sony360RA`
2. Search all `audioPackFormat` nodes for `audioPackFormatName` attribute.
   - If value contains `"Atmos"` (case-insensitive) → `DolbyAtmos`
   - If value contains `"360RA"` (case-insensitive) → `Sony360RA`
3. If no signals found → `Unknown` (caller treats as `Generic`)

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

### 11.5 CMake changes required

`adm_profile_resolver.cpp` is currently a stub and **not compiled**. In Phase 4:

- Add `transcoding/adm/adm_profile_resolver.cpp` to both `cult-transcoder` and
  `cult_tests` source lists in `CMakeLists.txt` (same pattern as `adm_reader.cpp`
  was added in Phase 3).
- Add `transcoding/adm` is already in include dirs — no new path needed.
- No new third-party library. pugixml is already linked.

---

### 11.6 Test requirements

Minimum new tests required in `test_cli_args.cpp` (or a new `test_lfe_mode.cpp`):

| Test                                                                      | Input                                 | Expected                                                                                                  |
| ------------------------------------------------------------------------- | ------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| `--lfe-mode hardcoded` explicit = same as default                         | parity fixture                        | passes parity                                                                                             |
| `--lfe-mode speaker-label` on fixture with `speakerLabel="LFE"`           | new fixture or existing 360RA fixture | node typed `LFE`                                                                                          |
| `--lfe-mode speaker-label` on Atmos fixture (LFE at ch4, no speakerLabel) | existing Atmos fixture                | ch4 node typed differently if no speakerLabel, or same if speakerLabel present — document expected result |
| `--lfe-mode garbage`                                                      | any                                   | non-zero exit, `status: "fail"` report                                                                    |
| Profile detection: Atmos XML → `detectedFrom` correct                     | Atmos fixture                         | `DolbyAtmos` detected                                                                                     |
| Profile detection: generic XML → `Generic` or `Unknown`                   | existing parity fixture               | `Unknown` or `Generic`                                                                                    |
| Parity regression with default lfe-mode                                   | parity fixtures                       | 28/28 still pass                                                                                          |

**Fixtures needed** (must be committed to `tests/parity/fixtures/`):

- `sony_360ra_example.xml` — Sony 360RA ADM XML export. Source: use
  `sourceData/sony360RA_example.xml` already present in the spatialroot repo
  (confirmed at `spatialroot/processedData/sony360RA_example.xml` and
  `spatialroot/sourceData/`). Copy or symlink it. Do not modify the source file.
- A reference output `sony_360ra_reference.lusid.json` generated by the Python
  oracle (`parse_adm_xml_to_lusid_scene`) before implementing Phase 4, so parity
  is verifiable.

---

### 11.7 Report contract additions

The report `args` block must include the resolved `--lfe-mode` value even when
defaulted. This means `TranscodeRequest` needs a `lfeMode` field, and
`report.cpp` / `main.cpp` must serialise it. The field name in the JSON report
args block is `"lfeMode"` (camelCase, matching existing report field naming).

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
| `src/transcoder.cpp`                                       | Parse `--lfe-mode` arg; pass to `resolveAdmProfile()` + converter calls                              |
| `src/main.cpp`                                             | Serialise `lfeMode` in report args block                                                             |
| `CMakeLists.txt`                                           | Add `adm_profile_resolver.cpp` to both targets                                                       |
| `tests/test_cli_args.cpp` or new `tests/test_lfe_mode.cpp` | New Phase 4 tests (see §11.6)                                                                        |
| `tests/parity/fixtures/`                                   | Add Sony 360RA fixture + reference LUSID                                                             |
| `internalDocsMD/AGENTS-CULT.md`                            | Update §1 repo layout, §2 CLI contract, §11 status                                                   |
| `internalDocsMD/DEV-PLAN-CULT.md`                          | Mark Phase 4 work items complete                                                                     |
| `spatialroot/internalDocsMD/AGENTS.md`                     | Note `--lfe-mode` flag exists, update binary contract if needed                                      |

**Do not touch**: `transcoding/lusid/lusid_writer.cpp`, `lusid_validate.cpp`
(Phase 5+ stubs), `runPipeline.py` (deprecated), any LUSID Python files.

---

### 11.9 Open questions that need owner answers before Phase 4 starts

These are **not** things the agent should guess or decide unilaterally:

1. **Sony 360RA speakerLabel pattern**: Does the `sony360RA_example.xml` in
   `sourceData/` have `speakerLabel="LFE"` or `"LFE1"` attributes, or does it
   use a different convention? The agent must inspect the file before implementing
   the `SpeakerLabel` branch. Run:

   ```bash
   grep -i "speakerLabel\|LFE" sourceData/sony360RA_example.xml | head -20
   ```

2. **360RA fixture parity baseline**: Should the Python oracle be run on the
   360RA fixture _before_ Phase 4 begins (to create a reference LUSID), or is
   the Phase 4 goal to improve on what the oracle produces? Clarify whether
   Phase 4 must maintain oracle parity for 360RA or is allowed to differ.

3. **Profile-driven bed remapping**: In Phase 4, the profile is detected but
   only the LFE flag uses it. Is any other profile-driven behaviour (e.g.,
   different channel-order assumptions for Sony vs Atmos) required in Phase 4,
   or is that deferred to Phase 5?

4. **`--lfe-mode` exposure in `runRealtime.py`**: Should the Python pipeline
   ever pass `--lfe-mode speaker-label` to cult-transcoder, or is this flag
   only for CLI / testing use? If the pipeline should expose it, the
   `RealtimeConfig` dataclass in `realtime_runner.py` needs a new field.

---

## 12. Agent PR Checklist (Required)

Every PR must include:

- Phase targeted (P1–P6)
- Parity status (P2/P4): pass/fail and what fixtures were used
- `--lfe-mode hardcoded` regression confirmed (Phase 4+): yes/no
- Ordering preservation confirmed (yes/no)
- timeUnit is "seconds" (yes/no)
- LFE behavior unchanged under default flag (yes/no)
- Pipeline dependency on containsAudio removed/commented (yes/no)
- Docs updated (list exact files)
- Report schema version and fields confirmed (yes/no)

No merge if:

- parity fails (Phase 2/4)
- ordering rules drift (Phase 2)
- output is non-atomic
- docs not updated for behavior changes
