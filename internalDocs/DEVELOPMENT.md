# CULT Development History

This document is the historical development record for `cult_transcoder`.

It consolidates the durable development history that had been spread across:

- `internalDocs/resamplingPlan.md`
- `internalDocs/CHANGELOG.md`
- milestone and execution notes in `internalDocs/AGENTS-CULT.md`

Use this file to understand how the module evolved, what was implemented when, which decisions were transitional versus durable, and which items were intentionally moved to future work.

## Documentation Role

- `internalDocs/DEVELOPMENT.md` is the historical dev log and milestone record.
- `internalDocs/AUTHORING.md` is the canonical internal authoring contract plus validation record.
- `internalDocs/AGENTS-CULT.md` remains the execution-safe operational guide for future coding work.
- `internalDocs/audit.md` remains the integration-facing record for SpatialSeed wiring and non-regression evidence.

Rule of thumb:

- update `DEVELOPMENT.md` when a change materially affects project history, architecture evolution, or milestone status
- update `AUTHORING.md` when a stable authoring contract or validation finding changes
- update `AGENTS-CULT.md` only when future implementation guidance or execution constraints change

## Historical Summary

`cult_transcoder` started as an ADM XML/WAV to LUSID Scene JSON transcoder with a parity-critical ingest path that had to match the existing Python oracle. The major 2026 development push extended the module in two new directions without regressing ingest:

- export-side LUSID -> ADM authoring via `adm-author`
- source-side ADM WAV -> LUSID package generation via `package-adm-wav`

The guiding constraint throughout this work was that LUSID remained canonical and the existing `adm_xml` / `adm_wav -> lusid_json` path could not be destabilized.

## Core Development Principles

Several principles were reinforced repeatedly during implementation:

- preserve ingest/parity behavior unless a change is explicitly intended and revalidated
- keep authoring work separate from packaging work even when they share ADM/LUSID concepts
- use small, self-contained dependencies instead of host-OS or app-level media stacks
- keep deterministic ordering and mapping policy inside CULT rather than inheriting it from external libraries
- report losses, approximations, and validation failures explicitly

## Timeline

### 2026-03-31: Export-Side Authoring Direction Established

This was the contract-setting phase for LUSID -> ADM work.

Implemented or documented:

- export-side ADM authoring policies were written down
- authoring was framed as a separate command and API surface rather than an extension of ingest
- `adm-author` CLI entry point and stub implementation were added
- LUSID reader and WAV inspection scaffolding were added for authoring
- initial adm-author validation tests were added

Historical importance:

- this phase separated stable ingest concerns from new export concerns
- it established that authoring would be compatibility-first, especially for Logic Pro Atmos

### 2026-04-01: Resampling and Validation Layer Implemented

This phase implemented the normalization layer described in the original resampling plan.

Implemented:

- r8brain-backed mono WAV normalization for `adm-author`
- strict frame-count validation after normalization
- scene-duration validation against audio-derived duration
- report schema support for authoring resample and validation details
- CMake wiring for the vendored r8brain dependency

Resampling architecture decisions preserved from the original plan:

- resampling exists only for export-side authoring
- it normalizes supported mono source WAVs to 48 kHz float32
- it must not alter LUSID semantics or act as a general DSP subsystem
- source WAVs must never be overwritten
- only one trailing-sample EOF mismatch is tolerated, and that tolerance must be reported

Historical importance:

- this phase made `adm-author` viable for real source packages instead of only idealized 48 kHz assets
- it also established the pattern of explicit, structured validation rather than silent repair

### 2026-04-15: Internal Audio and Scene-Type Cleanup

Implemented:

- extracted `LusidNode`, `LusidFrame`, and `LusidScene` into `lusid_scene.hpp`
- collapsed overlapping normalization/result structs into a single `NormalizeResult` flow

Historical importance:

- reduced type duplication before the larger authoring and packaging push
- simplified how normalization and reporting shared data

### 2026-04-26: Module Reorganization and First Complete Authoring Slice

This was the main structural cleanup phase.

Implemented:

- introduced source module ownership boundaries under:
  - `src/authoring/`
  - `src/parsing/`
  - `src/reporting/`
- updated ownership docs and design references
- completed the first full deterministic ADM authoring slice
- added integration coverage for:
  - deterministic LUSID -> ADM mapping
  - authored XML generation
  - embedded BW64 `axml` parity with sidecar XML
- moved parsing and reporting ownership out of root-level transitional files
- removed obsolete transitional shims after references were updated
- shared CLI helper logic across `transcode` and `adm-author` without changing the external CLI contract

Validation gate:

- `ctest --test-dir build --output-on-failure` passed `70/70`

Historical importance:

- this was the point where authoring stopped being a side experiment and became a structured first-class subsystem
- it also made future maintenance clearer by giving each source area an explicit owner boundary

### 2026-04-27: Packaging, Progress Reporting, and Dolby/Logic Validation Phase

This was the feature-completion phase for v1 authoring and packaging.

Implemented:

- added `package-adm-wav` for ADM BWF/WAV -> LUSID package generation
- added `src/packaging/` ownership and dedicated tests
- added shared `ProgressCallback` / `ProgressEvent` API in `src/progress.hpp`
- wired CLI progress bars for `adm-author` and `package-adm-wav`
- documented stereo-pair reconstruction as future work
- recorded successful Logic Pro validation for `exported/lusid_package_logic_shaped.wav`
- added experimental `adm-author --dbmd-source <source.wav|dbmd.bin>`
- added explicit authored ADM `end` timing on `audioProgramme` and `audioObject`
- added experimental `adm-author --metadata-post-data`

Validation gate:

- `cmake --build build && ctest --test-dir build --output-on-failure` passed `73/73`

Historical importance:

- this was the point where the export-side authoring path became practical enough to call a functional v1 target
- packaging and progress reporting were intentionally kept separate from authoring fixes, which prevented conceptual drift

### 2026-04-28: LUSID Scene v1.0 Contract Update

Implemented:

- updated CULT-generated LUSID scenes to emit `version: "1.0"`
- updated parity fixtures and authoring/package tests from v0.5 scene headers to v1.0
- made the authoring LUSID reader require v1.0 input scenes
- added authoring support for v1.0 `timeUnit` aliases by converting `seconds`, `milliseconds`, and `samples` frame timestamps to internal seconds
- kept ADM ingest/parity ordering and node mapping behavior unchanged apart from the schema version field

Historical importance:

- this aligns CULT with the SpatialSeed-local schema-only LUSID repo at `LUSID/SCHEMA/lusid_scene_v1.0.schema.json`
- it preserves the existing CULT responsibilities: source-format conversion, package creation, validation, and ADM authoring stay in CULT rather than moving into LUSID

### 2026-04-28: ADM Transcoding Memory and Organization Cleanup

Implemented:

- removed duplicate XML parsing in the generic ADM transcode path by sharing the already-loaded `pugi::xml_document` between profile detection and generic conversion
- kept Sony 360RA dispatch behavior on the same parsed document path
- reduced generic ADM conversion staging by writing object block nodes directly into `timeToNodes` after direct speakers are collected
- preserved the existing channel-format encounter order, direct-speaker first ordering, object group numbering, frame ordering, LFE behavior, time rounding, reports, and LUSID JSON output shape
- removed the stale post-implementation planning stub from `adm_profile_resolver.cpp`

Historical importance:

- this lowered memory pressure and removed redundant parse work in the sensitive ingest path without changing the public CLI/API contract
- it records that future changes to this path should be parity-tested before altering node ordering, frame counts, profile warnings, or time encoding behavior

### 2026-04-28: Reporting Helper Dedup Cleanup

Implemented:

- extracted reporting JSON formatting helpers from `src/reporting/report.cpp` into `src/reporting/reportingHelper.hpp`
- aligned `src/reporting/` with the same local helper-header structure already used by `src/authoring/`, `src/parsing/`, and `src/packaging/`
- kept report serialization behavior and output shape unchanged while reducing one remaining pocket of folder-local helper duplication

Historical importance:

- this completed the reporting-side helper cleanup called out in the post-v1 authoring/package follow-up work
- it keeps module ownership patterns more consistent before the larger `transcoding/` relocation and other low-risk maintainability passes

### 2026-04-28: Transcoding Tree Relocation

Implemented:

- moved the ingest-side `transcoding/adm/` sources into `src/transcoding/adm/`
- updated all source includes to resolve through `src/`-rooted `transcoding/...` paths instead of a dedicated top-level include directory
- added `src/transcoding/transcoding.md` to make ingest ownership explicit alongside `authoring`, `parsing`, `reporting`, and `packaging`
- updated build wiring and internal docs to reflect the new source-tree location

Historical importance:

- this completes the remaining top-level source-tree migration work called out in the post-v1 cleanup plan
- it reduces path drift between module ownership docs and the actual code layout without changing ingest behavior or public CLI/API contracts

### 2026-04-28: Package Metadata Single-Parse Cleanup

Implemented:

- updated `src/packaging/adm_package.cpp` so generic `package-adm-wav` conversion reuses the already-loaded `pugi::xml_document`
- removed the extra generic-ADM reparse through `convertAdmToLusidFromBuffer()` after profile detection on the package path
- kept Sony 360RA package conversion on the existing parsed-document dispatch path

Historical importance:

- this aligns `package-adm-wav` metadata handling with the earlier single-parse cleanup already applied to `transcode`
- it removes redundant parse work in the package-generation path without changing package output shape, stem ordering, or CLI/API behavior

### 2026-04-28: ADM Helper Consolidation

Implemented:

- consolidated shared ADM document-root lookup and `Technical`-section lookup into `src/transcoding/adm/admHelper.hpp`
- updated both generic ADM and Sony 360RA converters to use the shared helper path instead of keeping parallel local lookup functions
- added focused helper tests for recursive descendant traversal order, wrapper/root lookup, and ADM timecode parsing

Historical importance:

- this closes the remaining low-risk ADM helper duplication called out in the post-v1 cleanup work
- it keeps parity-sensitive ingest behavior anchored to explicit helper coverage instead of relying only on broader end-to-end tests

### 2026-04-28: Time Compatibility Audit and Narrow Authoring Fix

Implemented:

- audited the three time paths together: LUSID v1 frame-time normalization, authored ADM time serialization, and shared ADM time parsing
- added coverage for millisecond input normalization, sample-rate-suffixed ADM helper parsing, sample-spaced authored object blocks, and authored ADM roundtrip timing
- kept `convertSceneTimeToSeconds()` unchanged because the accepted `seconds` / `milliseconds` / `samples` input handling was already correct
- kept shared ADM ingest parsing unchanged because the existing helper already accepted the fixture and authored wallclock forms under test
- updated `AdmWriter::formatAdmTime()` to keep plain wallclock ADM timing while extending fractional precision up to 9 digits when sample-spaced object blocks need more than the original 5-decimal shape

Validation gate:

- `ctest --test-dir build --output-on-failure` passed `81/81`

Historical importance:

- this resolved the main remaining timing risk without changing parity-critical ingest ordering or package-generation behavior
- it records that the real issue was authoring precision loss on sample-spaced object-block roundtrip, not LUSID v1 unit normalization or shared ADM helper parsing

### 2026-04-28: Metadata Streaming Phase 1

Implemented:

- added shared ostream-based serializers for LUSID Scene JSON and report JSON
- kept `lusidSceneToJson()` and `Report::toJson()` as compatibility wrappers over the shared stream-writing logic
- changed `writeLusidScene()`, `Report::writeTo()`, and report stdout printing to stream directly to their destination streams instead of building a full string first
- preserved existing JSON shape, field ordering, indentation, escaping, temp+rename behavior, and public API signatures
- left authored ADM XML materialization unchanged because the XML string is still reused for the sidecar XML file and BW64 `axml` embedding

Validation gate:

- focused report/package/parity compatibility checks passed
- `ctest --test-dir build --output-on-failure` passed after the change

Historical importance:

- this is the first low-risk metadata streaming pass after the post-v1 cleanup wave
- it narrows the remaining non-streaming metadata boundary to authored ADM XML and on-demand string compatibility helpers rather than JSON/report file output

### 2026-04-28: Metadata Streaming Phase 2 (Authoring Sidecar XML)

Implemented:

- refactored `AdmWriter` to build one authored `pugi::xml_document` and stream the sidecar `.adm.xml` file directly from that DOM
- kept one serialized `xmlString` for the existing BW64 `axml` chunk path and `--metadata-post-data` rewrite path
- preserved authored XML content, ordering, formatting, sidecar/embedded parity, and public CLI/API behavior

Validation gate:

- `ctest --test-dir build --output-on-failure` passed `83/83`

Historical importance:

- this extends the metadata-streaming work into the authoring path without disturbing the BW64 embedding contract
- it records the remaining future-work fork explicitly: either keep one compatibility XML string for `AxmlChunk` / post-data rewrite, or pursue a deeper redesign of authoring XML emission around dual-sink output or different `axml` chunk expectations

## Resampling History

The original `resamplingPlan.md` captured a narrow and important design boundary that should remain part of project history even after the standalone plan is retired.

### What the resampler was for

The resampler exists for one job only:

- normalize supported mono WAV assets to 48 kHz float32 for the `adm-author` path

It was never intended to be:

- a renderer feature
- a generic audio-processing subsystem
- part of the parity-critical ingest path
- a place for remixing, padding, loudness processing, or semantic repair

### Dependency choice

The chosen backend was `r8brain-free-src`, vendored locally and wrapped behind CULT-owned code.

Why that mattered:

- it aligned with the self-contained dependency philosophy of the module
- it avoided introducing larger frameworks like FFmpeg for a narrowly scoped offline normalization task
- it kept normalization as infrastructure rather than policy

### Normalization contract

The development contract established during this phase was:

- mono WAV inputs only in v1
- normalize to 48 kHz float32
- do not resample already-compliant files unnecessarily
- fail on unsupported multichannel source assets
- validate normalized frame counts after normalization
- tolerate only a one-sample EOF spread by truncating to the shortest length

This contract remains important historical context because later authoring validation, reporting, and package roundtrips all depended on it.

## Refactor and Module-Reorganization History

The April 2026 refactor sequence matters because it changed where responsibility lives without changing ingest behavior.

Completed sequence:

1. Created source module layout scaffolding.
2. Moved authoring implementation behind `src/authoring/`.
3. Moved LUSID scene parsing behind `src/parsing/`.
4. Moved report construction and serialization behind `src/reporting/`.
5. Reduced duplicated CLI and pipeline orchestration logic.
6. Enforced regression gates for each reorganization slice.
7. Completed pending authoring mapping and integration validation after structure stabilization.

Historical outcome:

- parity-critical ingest behavior remained stable
- ownership boundaries became explicit
- authoring and packaging work became easier to evolve independently

## Validation and Milestone History

### Automated Validation

Major validation milestones:

- authoring stub-path validation landed during the early export-side scaffolding phase
- deterministic ADM mapping and BW64 integration tests landed during the April 26 slice
- package generation and progress-callback coverage landed during the April 27 slice

Key test gates:

- `70/70` after the first complete authoring/refactor slice
- `73/73` after package-generation and Dolby-tool experiment support landed

### Manual Validation

Important manual outcomes:

- Logic Pro import passed for the Logic-shaped authoring candidate
- Dolby Atmos Conversion Tool accepted conversion of the authored candidate
- the Dolby tool still reported that the file was not a Dolby-approved master

Historical conclusion:

- Logic compatibility is the meaningful v1 success criterion that was achieved
- Dolby approved-master recognition was investigated, but not solved, and is now explicitly future work

## Dolby Approval Investigation History

The Dolby approval-warning work deserves its own historical note because it consumed multiple rounds of structural experiments.

Implemented experiments:

- copied source `dbmd` into authored output via `--dbmd-source`
- added explicit nonzero authored ADM `end` timing
- rewrote authored metadata ordering to post-data layout via `--metadata-post-data`

Observed outcome:

- these experiments improved fidelity to observed Dolby/Logic-authored files
- they did not remove the unsupported-master warning
- the Dolby tool still converted the file successfully, and the converted output opened in Logic

Historical conclusion:

- Dolby-tool conversion usability is good enough for v1
- Dolby-approved-master recognition is now future work
- the most likely remaining cause is provenance/private Dolby metadata rather than basic ADM BWF structure

## Packaging History

`package-adm-wav` was introduced as a distinct workflow rather than a side effect of authoring work.

Its historical role:

- ingest an existing ADM BWF/WAV
- extract embedded ADM XML
- convert metadata to LUSID
- split interleaved source audio into mono float32 stems
- write a self-contained LUSID package

Important historical design choice:

- package generation does not reconstruct stereo pairs from adjacent mono object tracks
- instead, it preserves mono tracks and records generated channel order explicitly

This was intentionally left as future work because false stereo inference would be worse than preserving accurate mono provenance.

## Current Historical State

As of the latest verified development slice captured here:

- ingest/parity behavior remains protected
- export-side ADM authoring is functionally complete for v1 goals
- ADM WAV -> LUSID package generation is implemented and tested
- shared progress reporting exists for long-running transcoding work
- stereo-pair reconstruction remains future work
- Dolby-approved-master recognition remains future work

## Source Notes for This Consolidation

This historical document was consolidated from:

- `internalDocs/resamplingPlan.md`
- `internalDocs/CHANGELOG.md`
- implementation and milestone notes retained in `internalDocs/AGENTS-CULT.md`

Once this file is accepted as the canonical historical development record, the standalone resampling-plan and changelog docs can be deleted or reduced to redirect stubs.
