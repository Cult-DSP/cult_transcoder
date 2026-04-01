# CULT Transcoder Changelog

All notable changes to cult_transcoder are documented here. This file is scoped to this submodule only.

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
