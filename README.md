# cult_transcoder

C++17 CLI tool that transcodes ADM XML/WAV to LUSID Scene JSON, authors LUSID packages back to ADM BWF/WAV, and can package ADM BWF/WAV source material into self-contained LUSID packages.  
Submodule of [spatialroot](https://github.com/Cult-DSP/spatialroot), branch `devel`.

## Quick start

```bash
cmake -B build
cmake --build build --parallel
./build/cult-transcoder --version
```

## Run tests

```bash
cd build && ctest --output-on-failure
```

## Usage

```
cult-transcoder transcode \
  --in <path.xml>      --in-format  adm_xml   \
  --out <path.json>    --out-format lusid_json \
  [--report <path>]    [--stdout-report]
```

Export-side authoring command:

```
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet]

# alternate input
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet]
```

`adm-author` normalizes mono stems to 48 kHz and uses the normalized audio length as the ADM duration. A one-frame end-of-file mismatch is tolerated by authoring to the shortest stem length and ignoring the trailing sample from longer stems; larger frame-count mismatches fail with per-file details in the report.

Source package generation command:

```
cult-transcoder package-adm-wav \
  --in <source.wav> \
  --out-package <package-dir> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--lfe-mode hardcoded|speaker-label]
```

`package-adm-wav` is separate from authoring. It extracts embedded ADM XML, converts metadata to LUSID, splits interleaved audio into mono float32 stems, and writes `scene.lusid.json`, `channel_order.txt`, stems, and `scene_report.json`. Stereo-pair reconstruction is not implemented yet; object tracks are preserved as mono stems.

Long-running authoring and packaging phases render a progress bar to stderr. Use `--quiet` to disable it. Embedders can use the shared `ProgressCallback` API in `src/progress.hpp`.

Exit codes: `0` success · `1` transcode failure · `2` arg/usage error.  
A JSON report is always written next to the output file (or at `--report`).

## Phase status

| Phase | Description                       | Status      |
| ----- | --------------------------------- | ----------- |
| 1     | Repo skeleton, CLI, report schema | Complete |
| 2     | ADM XML -> LUSID JSON             | Complete |
| 3     | ADM WAV ingestion inside CULT     | Complete |
| 4     | Profile resolver + LFE detection  | Complete |
| 5     | LUSID -> Logic-compatible ADM WAV | Complete |
| 6     | ADM WAV -> LUSID package          | Complete |
| 7     | Stereo-pair reconstruction        | Future   |

See `internalDocsMD/DEV-PLAN-CULT.md` for details.

Note: `internalDocsMD/DEV-PLAN-CULT.md` is outdated for authoring. See
`internalDocsMD/admAuthoring.md` and `internalDocsMD/resamplingPlan.md` for the
current export-side ADM authoring plan.

## Docs

- `internalDocsMD/AGENTS-CULT.md` — execution-safe agent contract
- `internalDocsMD/DEV-PLAN-CULT.md` — phased development plan
- `internalDocsMD/DESIGN-DOC-V1-CULT.MD` — design reference

## Source module layout (April 2026 scaffolding)

To reduce duplication and make ownership explicit, source is organized into module folders:

- `src/authoring/` — LUSID → ADM export (`adm-author`) ownership.
- `src/parsing/` — scene/file parsing ownership.
- `src/reporting/` — report construction + write-policy ownership.

Current state:

- `src/authoring/` owns `adm-author` orchestration and ADM/BW64 writing.
- `src/packaging/` owns ADM BWF/WAV -> LUSID package generation and self-contained stem splitting.
- `src/parsing/` owns the LUSID scene reader (`src/parsing/lusid_reader.*`).
- `src/reporting/` owns report schema/serialization (`src/reporting/cult_report.hpp`, `src/reporting/report.cpp`).
- Root-level transitional shims from the move have been removed.
- Ingest/parity-critical behavior remains unchanged across the module re-org.

## License

Apache-2.0 © Cult-DSP
