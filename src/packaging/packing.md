# `src/packaging`

Owner: ADM BWF/WAV -> LUSID package generation (`package-adm-wav`) only.

## Scope

- Extract embedded ADM XML from source ADM BWF/WAV files.
- Convert ADM metadata into LUSID scene JSON using the existing ingest mapping.
- Split interleaved source audio into mono float32 WAV stems with a self-contained streaming reader.
- Write package outputs: `scene.lusid.json`, stems, `channel_order.txt`, and `scene_report.json`.
- Emit package progress events via `PackageAdmWavRequest::onProgress`.

## Non-goals

- No Logic-compatible ADM authoring fixes; those belong in `src/authoring`.
- No ffmpeg, libsndfile, or host-project audio utility dependency.
- No automatic stereo-pair reconstruction yet. Preserve mono tracks unless a future explicit pairing policy is implemented.
