# `src/authoring`

Owner: ADM authoring/export path (`adm-author`) only.

## Scope

- LUSID-to-ADM authoring orchestration.
- Deterministic authoring ordering policy.
- ADM XML + BW64 output generation.
- BW64 `axml` embedding and `chna` track-to-ADM binding.
- ADM block timing clamped to the audio-derived authoring duration.
- Authoring-side audio normalization/validation integration.
- Authoring progress events via `AdmAuthorRequest::onProgress`.

## Non-goals

- No changes to ingest/parity-critical `adm_xml -> lusid_json` behavior.
- No ADM WAV -> LUSID package splitting; that belongs in `src/packaging`.
- No Python-oracle parity logic.

## Migration status

Completed in this slice:

- `src/adm_author.cpp` → `src/authoring/adm_author.cpp`
- `src/adm_writer.cpp` → `src/authoring/adm_writer.cpp`
- `src/adm_writer.hpp` → `src/authoring/adm_writer.hpp`

The previous root-level transitional shims have been removed. New code should include authoring headers through `authoring/...`.
