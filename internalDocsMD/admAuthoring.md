# ADM Authoring Plan (cult_transcoder)

Last verified against the submodule code: 2026-03-31.  
This plan supersedes the earlier rough agent plan by explicitly treating LUSID → ADM as a new export-side extension rather than an implicit continuation of the current v1 ingest baseline.

`internalDocsMD/DEV-PLAN-CULT.md` is outdated and must not be used as a source of truth.

## 0. Purpose

Implement LUSID → authored ADM in `cult_transcoder` (C++), producing:

- `export.adm.xml`
- `export.adm.wav` (BW64 with embedded ADM metadata)
- report JSON

The practical first-pass acceptance target is successful import into Logic Pro Atmos.

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

### Out of scope for first pass

- modification of the current parity-critical ingest path
- generic ADM authoring completeness beyond the supported first-pass subset
- renderer-side bass management or venue-specific playback policy
- silent fallback behavior for unsupported semantics
- claiming lossless support for semantics not actually represented in authored output

## 3. Current Repo Reality (Do Not Ignore)

Observed today:

- CLI currently supports only `adm_xml -> lusid_json`.
- Existing parity depends on pugixml traversal matching the Python oracle.
- `libadm` is not part of the current parity-critical path.
- No verified LUSID → ADM export path currently exists in the repo.

Implication:

The authoring implementation must be added without disturbing the existing ingest/parity baseline.

## 4. Input / Output Contract

### Accepted inputs

Support both:

1. LUSID package folder:
   - contains `scene.lusid.json`
   - contains referenced WAV assets

2. Explicit input files:
   - `scene.lusid.json`
   - WAV directory path

### First-pass WAV assumptions

Unless explicitly expanded later, the first implementation should require:

- mono WAV assets for authored sources
- normalized audio format for authoring: mono, 48 kHz, float32
- explicit resampling for non-48 kHz mono WAVs (see resampling plan)
- consistent duration by strict equal frame counts after normalization
- explicit failure on missing assets or unsupported asset layouts

Resampling, padding, truncation, or interleaved-channel splitting must not occur silently. Resampling is allowed only via the dedicated authoring normalization stage and must be reported explicitly.

### containsAudio usage

`containsAudio.json` is deprecated for the authoring path. The authoring resolver must fall back to deterministic id-to-filename mapping for WAV resolution.

### Outputs

- `export.adm.xml`
- `export.adm.wav`
- report JSON

### Atomicity

All three outputs must be written via temp + rename.

## 5. Proposed CLI Contract

Add a new CLI subcommand:

```bash
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] [--stdout-report]

# Alternate input:
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] [--stdout-report]
```

### Exit codes

- `0`: success, outputs exist
- `1`: authoring failure, no final XML/WAV outputs
- `2`: arg/usage error

### Contract rules

- Keep existing `transcode` command unchanged.
- `adm-author` must not overload the meaning of the existing ingest command.
- Best-effort fail report still applies on errors.

## 6. Authoring Mapping Policy (Must Be Written Before Full Implementation)

This is the critical missing layer. Do not begin broad implementation without making this explicit.

### 6.1 Supported LUSID node types

First pass should explicitly define support for:

- `direct_speaker`
- `audio_object`
- `LFE`

Any other node type must be ignored with explicit loss-ledger entries. These are not hard failures unless they prevent authoring.

### 6.2 Bed / object / LFE policy

Pinned first-pass direction:

- `direct_speaker` nodes map to authored ADM bed/direct-speaker structures.
- `audio_object` nodes map to authored ADM objects.
- `LFE` maps inside the bed structure, not as a free object.
- If target-profile constraints require stricter placement or ordering, document and test that explicitly.

### 6.3 Ordering policy

Determinism is required.

Pinned first-pass default:

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
- Validate motion policy with manual Logic Pro import fixtures before calling it final.

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

Validation errors (missing WAVs, unsupported formats, normalization failure, post-normalization duration mismatch) are hard failures and must appear in `errors[]`, not `lossLedger`.

## 7. Implementation Steps

### Step 1: Doc-first contract update

Update, in the same change set if contract behavior changes:

- `internalDocsMD/AGENTS-CULT.md`
- design doc language around export-side authoring extension
- coupled LUSID / toolchain docs if the toolchain contract changes

### Step 2: Add separate request/result API

Extend `cult_transcoder.hpp` with dedicated structures such as:

- `AdmAuthorRequest`
- `AdmAuthorResult`

Do not overload the existing transcode API semantically.

### Step 3: Add new CLI subcommand in `src/main.cpp`

Implement `adm-author` argument parsing and dispatch.

### Step 4: Add a LUSID reader / resolver for authoring

Create a minimal, explicit reader for:

- `scene.lusid.json`
- package path resolution
- asset lookup and validation

### Step 5: Implement validation layer before writing output

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
- do not tolerate mismatched frame counts across normalized WAVs

Fail early with report output.

### Step 6: Build an internal authoring model

Create a clean intermediate representation between LUSID and `libadm`.

This model should own:

- source ordering
- entity typing
- timing/motion interpretation
- deterministic IDs
- mapping-loss bookkeeping

Do not let `libadm` become the implicit mapping policy.

### Step 7: Write authored ADM XML

Use `libadm` at this stage only.

Requirements:

- preserve explicit ordering from the internal authoring model
- write XML via temp + rename
- avoid hidden library defaults becoming contract behavior

### Step 8: Package BW64 output

Use `libbw64` to package audio + embedded metadata.

Requirements:

- write BW64 via temp + rename
- validate expected metadata embedding
- keep authored XML and packaged BW64 generation logically separate so either can be debugged

### Step 9: Extend report generation

Preserve existing required report fields.

Add authoring-specific summary fields as needed, such as:

- authored bed count
- authored object count
- authored channel count
- wav file count resolved
- authoring warnings / approximations

Validation and normalization reporting:

- resampling must be reported explicitly (source rate, target rate, file list)
- post-normalization duration mismatches are hard errors in `errors[]`
- include per-file expected vs actual frame counts in a structured report section

### Step 10: Add tests

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

### Manual validation

Import `export.adm.wav` into Logic Pro Atmos and verify:

- bed channels are recognized correctly
- objects appear correctly
- LFE is recognized in the intended structure
- timing aligns with LUSID frame times within the documented authoring policy
- no obvious import rejection due to malformed metadata or unsupported structure

Manual validation status must be stated explicitly in the PR.

## 9. Risks and Open Items

### Highest-risk items

1. **Mapping policy ambiguity**  
   The major risk is implementing XML/BW64 output before fully defining the supported LUSID → ADM subset.

2. **Over-broad use of libadm**  
   `libadm` should be confined to the export-writing layer, not allowed to redefine CULT’s canonical semantics.

3. **Ordering drift**  
   Deterministic ordering must come from CULT’s own mapping policy, not from incidental library traversal.

4. **Target mismatch**  
   “Generic ADM” is not the same thing as “must-pass in Logic Pro Atmos.” The first implementation should optimize for the latter.

5. **Silent reductions**  
   Unsupported motion or metadata must not be silently dropped without a loss-ledger entry.

### Open items that should be resolved in writing

- exact supported motion subset for first pass
- exact WAV duration policy
- exact sample-rate policy
- exact default report path for `adm-author`
- exact deterministic ADM ID policy

## 10. Deliverables

- `cult-transcoder adm-author` command implemented and documented
- dedicated authoring API surface added
- explicit mapping-policy documentation committed
- authored ADM XML writer integrated
- authored BW64 packager integrated
- tests covering validation, mapping, and integration
- updated docs reflecting the new export-side contract
- explicit statement of Logic Pro Atmos validation status
