# `src/transcoding`

Owner: ingest-side source-format conversion into LUSID.

## Scope

- Own the parity-critical ADM ingest path used by `transcode`.
- Keep ADM profile detection and profile-specific conversion under subfolders such as `adm/`.
- Preserve encounter ordering, timing behavior, and other ingest invariants across cleanup work.

## Non-goals

- No LUSID-to-ADM authoring; that belongs in `src/authoring/`.
- No package splitting or package writing; that belongs in `src/packaging/`.
- No report schema ownership; that belongs in `src/reporting/`.

## Migration status

Completed in this slice:

- `transcoding/adm/*` -> `src/transcoding/adm/*`

New code should include ingest helpers and converters through `transcoding/...` paths rooted at `src/`.
