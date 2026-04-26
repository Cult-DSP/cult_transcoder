# `src/authoring`

Owner: ADM authoring/export path (`adm-author`) only.

## Scope

- LUSID-to-ADM authoring orchestration.
- Deterministic authoring ordering policy.
- ADM XML + BW64 output generation.
- Authoring-side audio normalization/validation integration.

## Non-goals

- No changes to ingest/parity-critical `adm_xml -> lusid_json` behavior.
- No Python-oracle parity logic.

## Migration status

Completed in this slice:

- `src/adm_author.cpp` → `src/authoring/adm_author.cpp`
- `src/adm_writer.cpp` → `src/authoring/adm_writer.cpp`
- `src/adm_writer.hpp` → `src/authoring/adm_writer.hpp`

Transitional compatibility shims remain at old paths under `src/` and should be removed in a follow-up cleanup once all references are confirmed migrated.
