# CULT Transcoder Changelog

All notable changes to cult_transcoder are documented here. This file is scoped to this submodule only.

## 2026-04-26

- Added a pinned DRY + module re-organization task list to `internalDocsMD/AGENTS-CULT.md`, with required markdown update targets per task.
- Scaffolded new source module folders: `src/authoring/`, `src/parsing/`, and `src/reporting/`, each with ownership `README.md`.
- Updated `README.md` with a source-module layout section and migration status note.
- Updated `internalDocsMD/DESIGN-DOC-V1-CULT.MD` with explicit module boundary guidance for the new folders.
- Completed the first automated ADM authoring validation slice:
  - implemented deterministic ADM XML generation in `src/authoring/adm_writer.cpp`
  - added `tests/test_lusid_to_adm_mapping.cpp`
  - added `tests/test_adm_author_integration.cpp`
  - verified BW64 `axml` metadata matches the authored XML sidecar
- Validation evidence: `ctest --test-dir build --output-on-failure` passes 70/70 tests, including ingest/parity coverage.
- Manual Logic Pro Atmos import validation remains pending and must be recorded before calling the authoring path fully accepted.
- Moved LUSID scene parsing ownership into `src/parsing/`:
  - `src/parsing/lusid_reader.cpp`
  - `src/parsing/lusid_reader.hpp`
- Moved report schema/serialization ownership into `src/reporting/`:
  - `src/reporting/report.cpp`
  - `src/reporting/cult_report.hpp`
- Reduced duplicated CLI helper logic in `src/main.cpp` by sharing argument-value parsing, required-flag validation, atomic report writing, and error printing across `transcode` and `adm-author` without changing the CLI contract.
- Removed obsolete root-level transitional shims after updating project references:
  - `src/adm_author.cpp`
  - `src/adm_writer.cpp`
  - `src/adm_writer.hpp`
  - `src/lusid_reader.cpp`
  - `src/lusid_reader.hpp`
  - `src/report.cpp`
  - `src/cult_report.hpp`
- Final refactor gate evidence: `cmake --build build && ctest --test-dir build --output-on-failure` passes 70/70 after shim removal, including parity and authoring tests.

## 2026-03-31

- Documented export-side ADM authoring policies (resampling, duration validation, ordering, motion policy).
- Added a pointer to current authoring docs in OVERVIEW.
- Fixed OVERVIEW heading formatting.
- Added adm-author CLI entry point and stub implementation.
- Added LUSID reader and WAV inspection scaffolding for authoring.
- Added adm-author validation tests (stub-path coverage).

## 2026-04-01

- Implemented r8brain-backed mono WAV normalization for adm-author.
- Added strict frame-count validation and scene-duration check in adm-author.
- Extended report schema with authoring resample + validation sections.
- Wired r8brain include path and resampler source into CMake targets.

## 2026-04-15

- Extracted LUSID scene model (`LusidNode`, `LusidFrame`, `LusidScene`) into `lusid_scene.hpp`.
- Collapsed three normalisation struct variants (`NormalizedWavInfo` in `normalize_audio.hpp`, `AuthoringResampleEntry` in `cult_report.hpp`, and `NormalizeResult` in `resampler.hpp`) into a single `NormalizeResult` type to remove redundancy and eliminate copy loops.
