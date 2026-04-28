# ADM Authoring Contract (cult_transcoder)

Last verified against the submodule code: April 2026.  
This contract supersedes the earlier rough agent plan by explicitly treating LUSID -> ADM as an export-side extension rather than an implicit continuation of the current ingest baseline.

`internalDocsMD/DEV-PLAN-CULT.md` is outdated and must not be used as a source of truth.

## Documentation Map

- `internalDocsMD/admAuthoring.md` is this stable architecture and contract document.
- `internalDocsMD/AUTHORING.md` is the implementation and validation record, including Logic Pro findings and roundtrip evidence.
- `src/authoring/README.md` maps authoring code ownership.
- `src/packaging/README.md` maps ADM WAV -> LUSID package-generation ownership.
- `internalDocsMD/audit.md` explains SpatialSeed integration status.

Rule of thumb: keep this file concise and contract-focused; put detailed experiments and validation artifacts in `AUTHORING.md`.

## 0. Purpose

Support LUSID -> authored ADM in `cult_transcoder` (C++), producing:

- `export.adm.xml`
- `export.wav` (ADM BWF/BW64 WAV with embedded ADM metadata)
- report JSON

The practical acceptance target is successful import into Logic Pro Atmos. This is currently satisfied by the Logic-shaped authoring output documented in `AUTHORING.md`.

## 1. Architectural Framing

This feature is an **export-side extension** layered on top of the canonical LUSID scene model.
It must not redefine or regress the current parity-critical ADM ingest path.

Pinned framing:

- LUSID remains the canonical intermediate.
- Existing `adm_xml -> lusid_json` behavior remains frozen unless explicitly changed.
- The authoring path is a separate command and API surface.
- `libadm` may be introduced for authoring/export only.
- The current ingest/parity path continues to use pugixml and the Python oracle model.
- CULT continues to own scene interpretation, mapping policy, deterministic behavior, and loss reporting.

## 2. Scope and Constraints

### In scope

- LUSID package folder input
- explicit `scene.lusid.json` + WAV directory input
- generation of authored ADM XML
- generation of authored BW64 WAV with embedded metadata
- deterministic ordering and deterministic ID policy
- explicit loss-ledger reporting for unsupported or approximate mappings
- tests for validation, mapping, and output generation

### Out of scope for the current supported subset

- modification of the current parity-critical ingest path
- generic ADM authoring completeness beyond the supported subset
- renderer-side bass management or venue-specific playback policy
- silent fallback behavior for unsupported semantics
- claiming lossless support for semantics not actually represented in authored output

## 3. Current Repo Reality (Do Not Ignore)

Observed 2026-04-27:

- CLI supports `adm_xml`/`adm_wav -> lusid_json` through `transcode` and LUSID -> ADM/BW64 through `adm-author`.
- Existing parity depends on pugixml traversal matching the Python oracle.
- `libadm` is not part of the current parity-critical path; authoring XML generation is owned by CULT and written through pugixml.
- The LUSID -> ADM/BW64 path is implemented in `src/authoring/`, covered by mapping/integration tests, and Logic-validated.

Implication:

Authoring changes must continue to avoid disturbing the existing ingest/parity baseline.

## 4. Input / Output Contract

### Accepted inputs

Support both:

1. LUSID package folder:
   - contains `scene.lusid.json`
   - contains referenced WAV assets

2. Explicit input files:
   - `scene.lusid.json`
   - WAV directory path

### WAV assumptions

Current supported behavior:

- mono WAV assets for authored sources
- normalized audio format for authoring: mono, 48 kHz, float32
- explicit resampling for non-48 kHz mono WAVs (see resampling plan)
- consistent duration by normalized frame counts after applying the one-frame EOF tolerance
- explicit failure on missing assets or unsupported asset layouts

Resampling, padding, truncation, or interleaved-channel splitting must not occur silently. Resampling is allowed only via the dedicated authoring normalization stage and must be reported explicitly. The only permitted truncation is ignoring one trailing sample from longer normalized WAVs, and that must be reported as a warning plus a loss-ledger entry.

### containsAudio usage

`containsAudio.json` is deprecated for the authoring path. The authoring resolver must fall back to deterministic id-to-filename mapping for WAV resolution.

### Outputs

- `export.adm.xml`
- `export.wav`
- report JSON

### Atomicity

All three outputs must be written via temp + rename.

## 5. CLI Contract

Authoring command:

```bash
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>]

# Alternate input:
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>]
```

### Exit codes

- `0`: success, outputs exist
- `1`: authoring failure, no final XML/WAV outputs
- `2`: arg/usage error

### Contract rules

- Keep existing `transcode` command unchanged.
- `adm-author` must not overload the meaning of the existing ingest command.
- Best-effort fail report still applies on errors.

## 6. Authoring Mapping Policy

### 6.1 Supported LUSID node types

Supported node types:

- `direct_speaker`
- `audio_object`
- `LFE`

Any other node type must be ignored with explicit loss-ledger entries. These are not hard failures unless they prevent authoring.

### 6.2 Bed / Object / LFE Policy

- `direct_speaker` nodes map to authored ADM bed/direct-speaker structures.
- `audio_object` nodes map to authored ADM objects.
- `LFE` maps inside the bed structure, not as a free object.
- Logic-compatible output uses one `Master` bed object/pack for the bed and LFE allocation.

### 6.3 Ordering policy

Determinism is required.

Pinned default:

1. Bed/direct-speaker elements first in fixed template order (1.1, 2.1, 3.1, 4.1 LFE, 5.1..10.1)
2. Objects after beds, ordered by ascending LUSID group id (11.1, 12.1, ...)
3. Motion blocks ordered by ascending frame time per object

Do not rely on `libadm` traversal order as the source of truth.

### 6.4 Time and motion policy

LUSID is frame-based. Authored ADM needs an explicit time/motion strategy.

Policy for v1 authoring:

- Use step-hold object authoring: one `audioBlockFormat` per LUSID frame per object.
- Set explicit `rtime` and `duration` for each block.
- Treat this as a compatibility-first motion export, not a guarantee of smooth motion equivalence.
- Logic validation passed for the current step-hold output; future motion changes need fresh DAW validation.

### 6.5 ID policy

Define deterministic mapping from LUSID IDs to authored ADM entity IDs.

Rules:

- ID assignment must be deterministic across runs.
- Do not allow library auto-generation to become the only stable identity source.
- If IDs must be renumbered for profile compatibility, preserve a traceable mapping in code/reporting.

### 6.6 Loss-ledger policy

Report, do not invent.

Examples of authoring-side losses that must be surfaced:

- unsupported node types
- unsupported motion patterns
- unsupported metadata fields
- asset mismatches requiring rejection or approximation
- profile-driven reductions or omissions

Validation errors (missing WAVs, unsupported formats, normalization failure, post-normalization frame-count mismatches larger than one frame, and scene-duration disagreement) are hard failures and must appear in `errors[]`, not `lossLedger`. The one permitted EOF truncation is reported as both a warning and a loss-ledger entry.

## 7. Implementation Status

### Step 1: Doc-first contract update (Implemented)

Update, in the same change set if contract behavior changes:

- `internalDocsMD/AGENTS-CULT.md`
- design doc language around export-side authoring extension
- coupled LUSID / toolchain docs if the toolchain contract changes

### Step 2: Add separate request/result API (Implemented)

`cult_transcoder.hpp` exposes dedicated structures:

- `AdmAuthorRequest`
- `AdmAuthorResult`

Do not overload the existing transcode API semantically.

### Step 3: Add CLI subcommand in `src/main.cpp` (Implemented)

`adm-author` argument parsing and dispatch are implemented.

### Step 4: Add a LUSID reader / resolver for authoring (Implemented)

The authoring reader handles:

- `scene.lusid.json`
- package path resolution
- asset lookup and validation

### Step 5: Implement validation layer before writing output (Implemented)

Validate:

- mutually valid CLI arg combinations
- required files exist
- supported node types only
- sample rate agreement
- duration agreement
- WAV layout assumptions

Additional v1 rules:

- normalize audio to mono 48 kHz float32 before duration validation
- use normalized frame counts as the authoritative duration source
- if `scene.lusid.json` includes `duration`, validate it against the normalized audio duration; fail on mismatch
- tolerate only a one-frame spread across normalized WAVs by using the shortest length and ignoring trailing samples from longer files
- fail larger normalized frame-count mismatches

The path fails early with report output.

### Step 6: Build an internal authoring model (Implemented)

Create a clean intermediate representation between LUSID and `pugixml`.

This model should own:

- source ordering
- entity typing
- timing/motion interpretation
- deterministic IDs
- mapping-loss bookkeeping

Do not let `libadm` become the implicit mapping policy.

_(We fully isolated this in `src/authoring/adm_writer.cpp` with deterministic ordering rules and exact IDs.)_

Module location note (April 2026 re-org slice):

- `adm-author` entrypoint implementation: `src/authoring/adm_author.cpp`
- authoring writer/mapping implementation: `src/authoring/adm_writer.cpp`
- authoring writer interface: `src/authoring/adm_writer.hpp`

### Step 7: Write authored ADM XML (Implemented)

Use `pugixml` at this stage.

Requirements:

- preserve explicit ordering from the internal authoring model
- write XML via temp + rename
- avoid hidden library defaults becoming contract behavior

### Step 8: Package BW64 output (Implemented)

Use `libbw64` to package audio + embedded metadata.

Requirements:

- write BW64 via temp + rename
- validate expected metadata embedding
- keep authored XML and packaged BW64 generation logically separate so either can be debugged

### Step 9: Extend report generation (Implemented)

Preserve existing required report fields.

Add authoring-specific summary fields as needed, such as:

- authored bed count
- authored object count
- authored channel count
- wav file count resolved
- authoring warnings / approximations

Current implementation:

- `summary.durationSec` is derived from normalized audio frame count at 48 kHz.
- `summary.sampleRate` is the authoring target sample rate (`48000`).
- `summary.timeUnit` remains `"seconds"`.
- `summary.numFrames` is the number of LUSID frames in the scene.
- `summary.countsByNodeType` records unique supported node IDs by LUSID node type.

Validation and normalization reporting:

- resampling must be reported explicitly (source rate, target rate, file list)
- post-normalization frame-count mismatches larger than one frame are hard errors in `errors[]`
- one trailing-frame mismatches are warnings; affected files set `truncatedToExpected: true` and `framesUsed` to the authored frame count
- include per-file expected vs actual frame counts in a structured report section

### Step 10: Add tests (Implemented)

Required minimum test coverage:

- CLI arg validation tests
- asset validation tests
- mapping tests for node-type counts and ordering
- deterministic ID tests
- integration test generating XML + BW64
- validation of embedded metadata presence
- zero regressions to existing parity tests

## 8. Validation Plan (Logic Pro Atmos)

### Automated validation

- verify outputs exist only on success
- verify report behavior on both success and failure
- verify authoring subset constraints
- verify deterministic output structure across repeated runs

Status 2026-04-27:

- `tests/test_lusid_to_adm_mapping.cpp` verifies deterministic bed/LFE/object ordering, object counts, direct-speaker/object channel typing, LFE placement, and step-hold `audioBlockFormat` timing.
- `tests/test_adm_author_integration.cpp` verifies `admAuthor()` emits sidecar ADM XML and BW64, and that the BW64 `axml` chunk exactly matches the XML sidecar.
- `tests/test_adm_package.cpp` verifies the separate ADM WAV -> LUSID package flow and its split progress callback.
- `ctest --test-dir build --output-on-failure` passes 72/72 tests, including existing ingest/parity tests.

### Manual validation

Import `export.wav` into Logic Pro Atmos and verify:

- bed channels are recognized correctly
- objects appear correctly
- LFE is recognized in the intended structure
- timing aligns with LUSID frame times within the documented authoring policy
- no obvious import rejection due to malformed metadata or unsupported structure

Manual validation status should be stated explicitly when authoring behavior changes.

Status 2026-04-27: `exported/lusid_package_logic_shaped.wav` imported successfully in Logic Pro. Keep the current Logic-shaped ADM XML conventions unless a newer Logic/Dolby compatibility fixture proves otherwise.

Dolby Atmos Conversion Tool status: current output still shows an unsupported-master warning because it is not recognized as created by Dolby-approved software, but the tool successfully converted the candidate and the converted file opened in Logic. The earlier FFOA range message is not the current blocker.

## 8.1 Progress and Packaging Boundary

`adm-author` is the export-side authoring path. It now reports progress through `AdmAuthorRequest::onProgress` using `ProgressEvent` phases such as `metadata`, `inspect`, `normalize`, and `interleave`.

`package-adm-wav` is a separate source-packaging path for ADM BWF/WAV -> LUSID package generation. It reports package progress through `PackageAdmWavRequest::onProgress`, especially the long-running `split` phase.

The two paths should remain separate:

- authoring fixes target Logic-compatible ADM BWF output from an existing LUSID package
- package generation extracts ADM metadata and splits interleaved audio into mono stems for a LUSID package

Open item:

- Stereo-pair reconstruction from adjacent mono ADM tracks is not implemented. Package generation currently preserves mono tracks as mono stems and documents the generated node order in `channel_order.txt`.

## 9. Risks and Open Items

### Highest-risk items

1. **Mapping policy drift**  
   The supported LUSID -> ADM subset is now defined, but future edits can accidentally broaden it without matching tests/docs.

2. **Over-broad use of libadm**  
   `libadm` should be confined to the export-writing layer, not allowed to redefine CULT’s canonical semantics.

3. **Ordering drift**  
   Deterministic ordering must come from CULT’s own mapping policy, not from incidental library traversal.

4. **Target mismatch**  
   “Generic ADM” is not the same thing as “must-pass in Logic Pro Atmos.” The first implementation should optimize for the latter.

5. **Silent reductions**  
   Unsupported motion or metadata must not be silently dropped without a loss-ledger entry.

### Remaining open items

- Stereo-pair reconstruction from mono L/R ADM tracks is not implemented.
- Future motion interpolation beyond step-hold needs a written policy and fresh Logic validation.
- Future metadata expansion beyond the current Logic-compatible ADM subset needs explicit loss/reporting policy.
- Dolby-approved-master recognition is not yet solved; investigate Dolby-private/provenance metadata if that target becomes required. Conversion usability is currently validated.

## 10. Deliverables

- `cult-transcoder adm-author` command implemented and documented
- dedicated authoring API surface added
- explicit mapping-policy documentation committed
- authored ADM XML writer integrated
- authored BW64 packager integrated
- tests covering validation, mapping, and integration added
- updated docs reflecting the new export-side contract
- explicit statement of Logic Pro Atmos validation status: passed for `exported/lusid_package_logic_shaped.wav` as of 2026-04-27
