# CULT Transcoder Changelog

All notable changes to cult_transcoder are documented here. This file is scoped to this submodule only.

## 2026-04-26

- Added a pinned DRY + module re-organization task list to `internalDocsMD/AGENTS-CULT.md`, with required markdown update targets per task.
- Scaffolded new source module folders: `src/authoring/`, `src/parsing/`, and `src/reporting/`, each with ownership `README.md`.
- Updated `README.md` with a source-module layout section and migration status note.
- Updated `internalDocsMD/DESIGN-DOC-V1-CULT.MD` with explicit module boundary guidance for the new folders.

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
