# cult_transcoder

C++17 CLI tool that transcodes ADM XML to LUSID Scene JSON.  
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

Planned export-side authoring command:

```
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] [--stdout-report]

# alternate input
cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.adm.wav> \
  [--report <path>] [--stdout-report]
```

Exit codes: `0` success · `1` transcode failure · `2` arg/usage error.  
A JSON report is always written next to the output file (or at `--report`).

## Phase status

| Phase | Description                       | Status      |
| ----- | --------------------------------- | ----------- |
| 1     | Repo skeleton, CLI, report schema | ✅ Complete |
| 2     | ADM XML → LUSID JSON (libadm)     | ⏳ Pending  |
| 3     | ADM WAV ingestion inside CULT     | ⏳ Pending  |
| 4     | Profile resolver + LFE detection  | ⏳ Pending  |
| 5     | GUI transcoding tab               | ⏳ Pending  |
| 6     | MPEG-H / Sony 360RA support       | ⏳ Pending  |

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
- `src/parsing/` owns the LUSID scene reader (`src/parsing/lusid_reader.*`).
- `src/reporting/` owns report schema/serialization (`src/reporting/cult_report.hpp`, `src/reporting/report.cpp`).
- Root-level transitional shims from the move have been removed.
- Ingest/parity-critical behavior remains unchanged across the module re-org.

## License

Apache-2.0 © Cult-DSP
