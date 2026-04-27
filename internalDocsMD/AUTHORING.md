# ADM Authoring Extension — Implementation Decisions

This document formally captures the design decisions for the `adm-author` pipeline (Steps 3 and 4) which handles LUSID → ADM export mapping and writing to a `.wav` BW64 container. It follows the execution-safe, contract-first design set throughout CULT.

## Implementation Entry Points (April 2026 module move)

Authoring code now lives under `src/authoring/`.

- Entrypoint orchestration: `src/authoring/adm_author.cpp`
- Mapping + BW64 writer implementation: `src/authoring/adm_writer.cpp`
- Writer interface: `src/authoring/adm_writer.hpp`

Notes:

- This move is structural only; behavior and validation semantics remain unchanged.
- Ingest/parity-critical `adm_xml -> lusid_json` paths are intentionally untouched.

## 1. ADM ID Generation Strategy (ITU-R BS.2076)

- Generating canonical ADM XML inherently demands an explicit hierarchical identifier scheme (`AC_...`, `AT_...`, `AP_...`).
- We will strictly follow the standard `ITU-R BS.2076` hex mapping.
- Objects will start sequentially from `1001` (e.g. `AO_1001`).
- The sequential increment will deterministically pair with LUSID IDs in order (direct speakers / beds like `1.1` to `10.1`, followed by objects sorted by their string IDs).

## 2. Metadata Hierarchy Naming

- The root `audioProgramme` and `audioContent` nodes are mandatory in ADM but not strictly semantically matched back to LUSID.
- We allocate default, generic names such as `"CULT Authoring Programme"` and `"CULT Authoring Content"`.
- If future LUSID JSON schemas gain programme metadata fields, this will be mapped, but v0.1 mapping stays purely functional with these default names.

## 3. BW64 Buffer Interlacing

- **Dependency Scope:** CULT does **not** rely on `libsndfile` like the `spatialroot` renderer (`spatialroot_spatial_render`), because it avoids OS-locked dependencies.
- **Implementation:** We will use `libbw64` explicitly (`bw64::Bw64Writer`).
- **Interlacing Approach:** Instead of loading full length audio into memory, which crashes on long sessions, we will:
  1.  Open all Normalized (48 kHz float32) mono WAV files via our self-contained RIFF `wav_io.cpp` reader.
  2.  Open the final output `export.adm.wav.tmp` via `bw64::Bw64Writer`.
  3.  Read fixed-sized block chunks from all mono files.
  4.  Interleave these streams into a contiguous float vector `[ch0_samp0, ch1_samp0, ch2_samp0...]`.
  5.  Pass the interleaved vector to `bw64Writer->write()`.
  6.  Finally, embed the constructed `pugixml` document into the `axml` chunk of the file before closing.

## 4. Logic Pro Import Finding (April 2026)

Manual validation with the authored `sourceData/lusid_package` output showed that the generated BW64 was readable by CULT and probeable as audio, but did not open in Logic Pro.

Observed facts:

- The authored file is a valid 78-channel PCM WAV/BW64 payload according to `ffprobe`.
- CULT can round-trip the embedded `axml` chunk from the authored `.adm.wav`.
- The authored sidecar XML and embedded `axml` match.
- The first Logic validation candidate lacked a populated BW64 `chna` chunk.
- The source package's final metadata frame can land fractionally beyond the audio-derived ADM duration after the one-frame EOF tolerance is applied.

Likely cause:

- `axml` alone is not enough for strict ADM/BW64 consumers. DAWs such as Logic need `chna` entries to bind each PCM track index to the matching ADM `audioTrackUID`, `audioTrackFormatID`, and `audioPackFormatID`.
- ADM IDs need to be aligned with BS.2076-style ID shapes that fit the fixed-width `chna` fields:
  - `ATU_00000001`
  - `AT_00031001_01`
  - `AP_00031001`
- ADM block timing must not extend past the programme/audio duration or emit zero-duration tail blocks.

Implemented follow-up:

- `adm_writer.cpp` now generates deterministic BS.2076-compatible ID shapes for pack, channel, stream, track format, and track UID references.
- The BW64 output now includes a populated `bw64::ChnaChunk` before the data chunk.
- `audioBlockFormat` timing is clamped to the audio-derived ADM duration and tail blocks at or beyond the duration are skipped.
- Integration tests verify `chna.numTracks`, `chna.numUids`, and representative `AudioId` mappings.

Superseded validation candidate, before timing clamp:

- `build/authoring_validation/lusid_package_logic_chna.adm.wav`
- `build/authoring_validation/lusid_package_logic_chna.adm.xml`
- `build/authoring_validation/lusid_package_logic_chna.adm.report.json`

Current validation candidate:

- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.wav`
- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.xml`
- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.report.json`
