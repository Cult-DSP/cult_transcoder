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

## CLI Reference

All commands write a JSON report. If `--report` is omitted, CULT chooses a default report path for that command. `--stdout-report` prints the same report JSON to stdout while still writing the report file.

Exit codes: `0` success · `1` processing failure · `2` arg/usage error.

### `transcode`

Convert ADM metadata into LUSID Scene JSON.

```
cult-transcoder transcode \
  --in <path> \
  --in-format <adm_xml|adm_wav> \
  --out <scene.lusid.json> \
  --out-format lusid_json \
  [--report <path>] \
  [--stdout-report] \
  [--lfe-mode <hardcoded|speaker-label>]
```

Options:

| Option | Required | Description |
| --- | --- | --- |
| `--in <path>` | yes | Source ADM XML or ADM WAV/BWF file. |
| `--in-format <adm_xml|adm_wav>` | yes | Input format. `adm_wav` extracts embedded `axml`. |
| `--out <path>` | yes | Output `scene.lusid.json`. |
| `--out-format lusid_json` | yes | Current supported output format. |
| `--report <path>` | no | Report path. Default: `<out>.report.json`. |
| `--stdout-report` | no | Also print report JSON to stdout. |
| `--lfe-mode <hardcoded|speaker-label>` | no | LFE detection mode. Default: `hardcoded`, where the fourth direct speaker is LFE. |

### `adm-author`

Author LUSID package material into Logic-compatible ADM BWF/WAV plus sidecar ADM XML.

```
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>]

# alternate input
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>]
```

`adm-author` normalizes mono stems to 48 kHz and uses the normalized audio length as the ADM duration. A one-frame end-of-file mismatch is tolerated by authoring to the shortest stem length and ignoring the trailing sample from longer stems; larger frame-count mismatches fail with per-file details in the report.

Options:

| Option | Required | Description |
| --- | --- | --- |
| `--lusid <scene.lusid.json>` | yes, unless `--lusid-package` is used | Explicit LUSID scene file. Must be paired with `--wav-dir`. |
| `--wav-dir <path>` | yes, unless `--lusid-package` is used | Directory containing mono WAV stems named by node ID, with `4.1` resolved as `LFE.wav`. |
| `--lusid-package <path>` | yes, unless `--lusid`/`--wav-dir` are used | Package directory containing `scene.lusid.json` and stems. Mutually exclusive with `--lusid`/`--wav-dir`. |
| `--out-xml <path>` | yes | Authored ADM XML sidecar. |
| `--out-wav <path>` | yes | Authored ADM BWF/WAV with embedded `axml` and `chna`. |
| `--report <path>` | no | Report path. Default: `<out-wav>.report.json`, or `<out-xml>.report.json` if no WAV path is available. |
| `--stdout-report` | no | Also print report JSON to stdout. |
| `--quiet` | no | Disable stderr progress bars. |
| `--dbmd-source <source.wav|dbmd.bin>` | no | Experimental Dolby-tool compatibility option. Copies `dbmd` from an existing ADM WAV, or treats a non-WAV file as raw `dbmd` payload. |

### `package-adm-wav`

Generate a self-contained LUSID package from an existing ADM BWF/WAV source.

```
cult-transcoder package-adm-wav \
  --in <source.wav> \
  --out-package <package-dir> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--lfe-mode hardcoded|speaker-label]
```

`package-adm-wav` is separate from authoring. It extracts embedded ADM XML, converts metadata to LUSID, splits interleaved audio into mono float32 stems, and writes `scene.lusid.json`, `channel_order.txt`, stems, and `scene_report.json`. Stereo-pair reconstruction is not implemented yet; object tracks are preserved as mono stems.

Options:

| Option | Required | Description |
| --- | --- | --- |
| `--in <source.wav>` | yes | Source ADM WAV/BWF with embedded `axml`. |
| `--out-package <package-dir>` | yes | Destination package directory. Existing package output is replaced after temp output succeeds. |
| `--report <path>` | no | Report path. Default: `<package-dir>/scene_report.json`. A package-local `scene_report.json` is also written. |
| `--stdout-report` | no | Also print report JSON to stdout. |
| `--quiet` | no | Disable stderr progress bars. |
| `--lfe-mode <hardcoded|speaker-label>` | no | LFE detection mode. Default: `hardcoded`. |

## Public C++ API

The stable public entry point is [src/cult_transcoder.hpp](src/cult_transcoder.hpp).

Primary calls:

| Function | Request | Result | Purpose |
| --- | --- | --- | --- |
| `cult::transcode()` | `TranscodeRequest` | `TranscodeResult` | ADM XML/WAV -> LUSID JSON. |
| `cult::admAuthor()` | `AdmAuthorRequest` | `AdmAuthorResult` | LUSID package/stems -> ADM XML + ADM BWF/WAV. |
| `cult::packageAdmWav()` | `PackageAdmWavRequest` | `PackageAdmWavResult` | ADM BWF/WAV -> LUSID package. |

Long-running authoring and packaging phases can report progress through `ProgressCallback` in [src/progress.hpp](src/progress.hpp). CLI progress bars render to stderr and never contaminate `--stdout-report`.

Current progress phases:

| Phase | Used by | Meaning |
| --- | --- | --- |
| `metadata` | `adm-author`, `package-adm-wav` | Reading/converting scene metadata. |
| `inspect` | `adm-author` | Checking source stem files. |
| `normalize` | `adm-author` | Normalizing stems to 48 kHz float32. |
| `interleave` | `adm-author` | Writing interleaved ADM WAV audio. |
| `split` | `package-adm-wav` | Splitting interleaved ADM WAV audio into mono stems. |

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
