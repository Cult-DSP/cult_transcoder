# cult_transcoder — SpatialSeed Usage Audit

**Date:** 2026-04-26  
**Scope:** which submodule code is actively called by SpatialSeed, which is planned, and which is not used.

---

## Summary

| Category | Verdict |
|---|---|
| `cult-transcoder transcode` binary (ADM XML → LUSID JSON) | **NOT called** by SpatialSeed today |
| `cult-transcoder adm-author` binary (LUSID → ADM export) | **Built/tested, NOT called by SpatialSeed yet** — planned replacement for `adm_bw64.py` |
| `src/adm_to_lusid.cpp` (ingest/parity path) | **NOT called** by SpatialSeed directly |
| `src/authoring/adm_author.cpp` + audio stack | **Built/tested, NOT called by SpatialSeed yet** |
| Python `adm_bw64.py` (Stage 9B) | **Active** — currently owns ADM XML + WAV interleaving |
| LUSID Python oracle (`LUSID/src/xml_etree_parser.py`) | **NOT called** by SpatialSeed pipeline — only used by parity tests inside cult_transcoder |

---

## 0. Refactor Gate Evidence

The April 2026 source re-organization is complete inside `cult_transcoder`:

- `src/authoring/` owns `adm-author` orchestration and ADM/BW64 writing.
- `src/parsing/` owns LUSID scene parsing.
- `src/reporting/` owns report schema and JSON serialization.
- Root-level transitional shims for moved files were removed after project references were updated.

Removed shim paths:

- `src/adm_author.cpp`
- `src/adm_writer.cpp`
- `src/adm_writer.hpp`
- `src/lusid_reader.cpp`
- `src/lusid_reader.hpp`
- `src/report.cpp`
- `src/cult_report.hpp`

Non-regression evidence:

- `cmake --build build && ctest --test-dir build --output-on-failure`
- Result: 70/70 tests passed, including ingest/parity and authoring mapping/integration coverage.

Manual Logic Pro Atmos import validation is intentionally outside this refactor gate and remains pending.

---

## 1. What SpatialSeed Actually Calls Today

SpatialSeed's pipeline (`src/pipeline.py`) is entirely Python. It **does not invoke the cult-transcoder binary at any point.** No `subprocess` call to `cult-transcoder` exists anywhere in `src/`.

### Stage 9A — LUSID Package (`src/export/lusid_package.py`)
- Pure Python. Copies WAVs, writes `containsAudio.json` and `mir_summary.json`.
- Uses `soundfile` for RMS computation only.
- No cult_transcoder involvement.

### Stage 9B — ADM/BW64 Export (`src/export/adm_bw64.py`)
- Pure Python. Active when `export_adm=True`.
- Generates ADM XML via `xml.etree.ElementTree` — a hand-rolled LUSID→ADM XML generator that duplicates the logic CULT is designed to own.
- Interleaves mono WAVs into a multichannel WAV using `soundfile` + numpy.
- Produces a sidecar `.xml` (not embedded BW64). No `axml`/`chna` chunks.
- **This is the overlap point.** `adm_bw64.py` is a Python stub doing what `cult-transcoder adm-author` is designed to do permanently in C++.

### LUSID Scene Writing (`src/export/lusid_writer.py`)
- Pure Python. Writes `scene.lusid.json` against LUSID v0.5 schema.
- No cult_transcoder involvement.

---

## 2. What Is Planned to Use cult_transcoder

### `cult-transcoder adm-author` → replaces `src/export/adm_bw64.py`
After Logic Pro Atmos manual validation, SpatialSeed's Stage 9B should be replaced with a subprocess call to:
```
cult_transcoder/build/cult-transcoder adm-author \
  --lusid-package <lusid_package_dir> \
  --out-xml export.adm.xml \
  --out-wav export.wav \
  --report export.adm.report.json
```
The Python `adm_bw64.py` would then be retired or kept only as a fallback.

**What is still needed before this wiring can happen:**
- Manual Logic Pro Atmos import validation of representative `export.wav` files.
- SpatialSeed pipeline call-site update and error/report surfacing.
- Re-test the fresh `chna` + timing-clamped authored BW64 in Logic Pro before enabling by default.

### `cult-transcoder transcode` (ADM XML → LUSID JSON)
Not currently part of the SpatialSeed pipeline. SpatialSeed generates LUSID from scratch (Python pipeline); it does not ingest ADM XML. This path serves parity testing and future use cases (e.g. importing an existing ADM mix for re-spatialisation), but has no wiring in `src/pipeline.py`.

---

## 3. What Is Not Used (and Why)

### ~~`transcoding/adm/adm_to_lusid.cpp`~~, ~~`transcoding/lusid/lusid_validate.cpp`~~, ~~`transcoding/lusid/lusid_writer.cpp`~~
**Deleted 2026-04-15.** All three were copyright-header-only skeletons, never compiled, never called by either project. `transcoding/lusid/` directory removed entirely.

### `src/authoring/`, `src/parsing/lusid_reader.cpp`, `src/reporting/`, `src/audio/wav_io.cpp`, `src/audio/normalize_audio.cpp`, `src/audio/resampler_r8brain.cpp`
Compiled, reachable through the `adm-author` CLI/API, and covered by automated validation/mapping/integration tests. They are not yet called by SpatialSeed's Python pipeline.

### LUSID Python oracle (`LUSID/src/xml_etree_parser.py`)
Only used inside cult_transcoder's parity test suite (`tests/parity/`). SpatialSeed's pipeline has no import of `LUSID.src.xml_etree_parser`.

---

## 4. Overlap / Tech Debt

| Item | Location | Status |
|---|---|---|
| ADM XML generation | `src/export/adm_bw64.py` (Python) + `src/authoring/adm_writer.cpp` (C++) | Duplicate — Python version is active in SpatialSeed, C++ is the target |
| WAV interleaving | `src/export/adm_bw64.py` via `soundfile` + `src/authoring/adm_writer.cpp` via `libbw64` | Duplicate until SpatialSeed is rewired |
| BW64 embedding (`axml`/`chna`) | C++ authoring path embeds `axml` and a populated `chna`; Python only writes sidecar XML | Needs fresh Logic validation |
| `containsAudio.json` | Still generated by `lusid_package.py` | AGENTS-CULT §4 marks it deprecated; not yet removed from pipeline |

---

## 5. Wire-Up Checklist (when Steps 3–6 complete)

- [ ] Add a `_call_cult_transcoder_adm_author()` helper in `src/export/adm_bw64.py` (or a new `src/export/cult_export.py`) that resolves the binary path relative to `__file__` → `../../cult_transcoder/build/cult-transcoder`
- [ ] Replace `ADMBw64Exporter.export_adm_bw64()` call in `pipeline.py` Stage 9B with the cult binary call
- [ ] Pass `--report` path through and surface errors in the pipeline logger
- [ ] Remove `soundfile` dependency from `adm_bw64.py` once interleaving moves to C++
- [ ] Update `internalDocsMD/agents.md` Stage 9 description to reflect the new wiring
- [ ] Record Logic Pro Atmos import validation results before enabling by default
