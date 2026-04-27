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
- Dolby/Logic Atmos-compatible authoring reserves `AO_1001` and `AP_00011001` for one multi-channel `Master` direct-speaker bed object.
- Bed channels `1.1` through `10.1` map to `AC_00011001` through `AC_0001100A`, `AT_00011001_01` through `AT_0001100A_01`, and `ATU_00000001` through `ATU_0000000A`.
- Object audio objects start at `AO_100B`. Object packs start at `AP_00031002`, while object channel/stream/track-format IDs follow the PCM track index (`AC_0003100B`, `AS_0003100B`, `AT_0003100B_01`, ...).

## 2. Metadata Hierarchy Naming

- The root `audioProgramme` and `audioContent` nodes are mandatory in ADM but not strictly semantically matched back to LUSID.
- We allocate default, generic names such as `"CULT Authoring Programme"` and `"CULT Authoring Content"`.
- If future LUSID JSON schemas gain programme metadata fields, this will be mapped, but v0.1 mapping stays purely functional with these default names.

## 3. BW64 Buffer Interlacing

- **Dependency Scope:** CULT does **not** rely on `libsndfile` like the `spatialroot` renderer (`spatialroot_spatial_render`), because it avoids OS-locked dependencies.
- **Implementation:** We will use `libbw64` explicitly (`bw64::Bw64Writer`).
- **Interlacing Approach:** Instead of loading full length audio into memory, which crashes on long sessions, we will:
  1.  Open all Normalized (48 kHz float32) mono WAV files via our self-contained RIFF `wav_io.cpp` reader.
  2.  Open the final output `export.wav.tmp` via `bw64::Bw64Writer`.
  3.  Read fixed-sized block chunks from all mono files.
  4.  Interleave these streams into a contiguous float vector `[ch0_samp0, ch1_samp0, ch2_samp0...]`.
  5.  Pass the interleaved vector to `bw64Writer->write()`.
  6.  Finally, embed the constructed `pugixml` document into the `axml` chunk of the file before closing.

## 4. Logic Pro Import Finding (April 2026)

Manual validation with the authored `sourceData/lusid_package` output showed that the generated BW64 was readable by CULT and probeable as audio, but did not open in Logic Pro.

Observed facts:

- The authored file is a valid 78-channel PCM WAV/BW64 payload according to `ffprobe`.
- CULT can round-trip the embedded `axml` chunk from the authored `.wav`.
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
- `adm_writer.cpp` now models the Atmos bed as one `Master` direct-speaker object/pack (`AO_1001` / `AP_00011001`) with multiple bed `audioTrackUIDRef`s instead of ten separate bed objects.
- Integration tests verify `chna.numTracks`, `chna.numUids`, and representative `AudioId` mappings.

Superseded validation candidate, before timing clamp:

- `build/authoring_validation/lusid_package_logic_chna.adm.wav`
- `build/authoring_validation/lusid_package_logic_chna.adm.xml`
- `build/authoring_validation/lusid_package_logic_chna.adm.report.json`

Superseded validation candidate, before Atmos bed model:

- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.wav`
- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.xml`
- `build/authoring_validation/lusid_package_logic_chna_clamped.adm.report.json`

Superseded validation candidate, same bed model but confusing `.adm.wav` filename:

- `build/authoring_validation/lusid_package_logic_bedmodel.adm.wav`
- `build/authoring_validation/lusid_package_logic_bedmodel.adm.xml`
- `build/authoring_validation/lusid_package_logic_bedmodel.adm.report.json`

Current validation candidate:

- `exported/lusid_package_logic_bedmodel.wav`
- `exported/lusid_package_logic_bedmodel.adm.xml`
- `exported/lusid_package_logic_bedmodel.report.json`

## 5. Original Dolby/Logic BWF Comparison (April 2026)

Reference source:

- Original BWF: `/Users/lucian/projects/spatialroot/sourceData/CANYON-ATMOS-LFE.wav`
- Extracted XML: `build/authoring_validation/original_canyon.axml.xml`
- Extracted chunks:
  - `build/authoring_validation/original_canyon.chna.bin`
  - `build/authoring_validation/original_canyon.dbmd.bin`

Extraction command:

```bash
build/cult-transcoder transcode \
  --in /Users/lucian/projects/spatialroot/sourceData/CANYON-ATMOS-LFE.wav \
  --in-format adm_wav \
  --out build/authoring_validation/original_canyon.lusid.json \
  --out-format lusid_json \
  --report build/authoring_validation/original_canyon.report.json
```

### 5.1 Container Differences

The original Dolby/Logic-authored file is not named `.adm.wav`; it is a normal `.wav` ADM BWF. The extension is therefore unlikely to be the blocker.

Original chunk order:

```text
RIFF/WAVE
JUNK 64
fmt  16
data 3528737698
axml 29821509
chna 4044
dbmd 706
```

Current CULT candidate chunk order:

```text
RIFF/WAVE
JUNK 40
fmt  16
chna 3124
axml 659059
data 1100664396
```

Findings:

- Both files are `RIFF/WAVE`, not forced `BW64` or `RF64`. The top-level FourCC is therefore not the likely Logic failure.
- Both files use classic PCM `fmt ` size 16, not WAVE_FORMAT_EXTENSIBLE. The `fmt ` shape is therefore not the likely Logic failure.
- The original has no `bext` chunk in the observed top-level chunk list, so missing `bext` is not currently the lead suspect.
- The original places `axml` and `chna` after `data`; CULT currently writes them before `data`. EBU/libbw64 documentation permits ADM linkage through `axml` + `chna`, but Logic may be following Dolby ADM BWF conventions more strictly.
- The original contains a `dbmd` chunk. The first printable strings found in it are:
  - `Created using Dolby equipment`
  - `Logic Pro with Dolby monitoring`
- CULT does not currently preserve or synthesize `dbmd`. This is now a high-priority suspect for Logic compatibility.

### 5.2 Track and Metadata Count Differences

Original BWF:

- Audio: 101 channels, 48 kHz, 24-bit, 242.624979 seconds.
- `chna`: 101 tracks, 101 UIDs.
- Extracted LUSID: 23,050 frames, 101 unique nodes:
  - 9 direct speakers
  - 1 LFE
  - 91 audio objects

Current source package / CULT-authored candidate:

- Audio: 78 channels, 48 kHz, 24-bit, 97.993625 seconds.
- `chna`: 78 tracks, 78 UIDs.
- Package LUSID: 36 frames, 78 unique nodes:
  - 9 direct speakers
  - 1 LFE
  - 68 audio objects

Finding:

- The package being reauthored is not equivalent to the original BWF. It is shorter and has 23 fewer audio object tracks. Even if the container were perfect, this file is a reduced derivative rather than a faithful re-author of `CANYON-ATMOS-LFE.wav`.
- This is not sufficient to explain the Logic import failure. A reduced 78-channel package should still be authorable as fresh ADM BWF source material if its ADM profile and container metadata are internally valid. Treat missing/silent-track pruning as a round-trip fidelity issue, not the primary import blocker.

### 5.3 Bed Modeling Difference

The original BWF models the bed as one `audioObject` named `Master`:

```xml
<audioObject audioObjectID="AO_1001" audioObjectName="Master" ...>
  <audioPackFormatIDRef>AP_00011001</audioPackFormatIDRef>
  <audioTrackUIDRef>ATU_00000001</audioTrackUIDRef>
  ...
  <audioTrackUIDRef>ATU_0000000A</audioTrackUIDRef>
</audioObject>
```

The referenced pack `AP_00011001` is a single `DirectSpeakers` pack with ten channel refs:

```xml
<audioPackFormat audioPackFormatID="AP_00011001" audioPackFormatName="Master" typeLabel="0001" typeDefinition="DirectSpeakers">
  <audioChannelFormatIDRef>AC_00011001</audioChannelFormatIDRef>
  ...
  <audioChannelFormatIDRef>AC_0001100a</audioChannelFormatIDRef>
</audioPackFormat>
```

Its channel labels are room-centric Dolby/Atmos labels:

```xml
<speakerLabel>RC_L</speakerLabel>
<speakerLabel>RC_R</speakerLabel>
<speakerLabel>RC_C</speakerLabel>
<speakerLabel>RC_LFE</speakerLabel>
<speakerLabel>RC_Lss</speakerLabel>
<speakerLabel>RC_Rss</speakerLabel>
<speakerLabel>RC_Lrs</speakerLabel>
<speakerLabel>RC_Rrs</speakerLabel>
<speakerLabel>RC_Lts</speakerLabel>
<speakerLabel>RC_Rts</speakerLabel>
```

Earlier CULT output modeled each bed channel as its own `audioObject` and pack (`AP_00010001`, `AP_00010002`, ...). That is valid generic ADM-shaped metadata, but it does not match the Dolby/Logic Atmos bed profile.

Implemented fix:

- Current CULT output now emits one `audioObject audioObjectID="AO_1001" audioObjectName="Master"`.
- The `Master` object references one bed pack, `AP_00011001`.
- The bed object references all ten bed UIDs, `ATU_00000001` through `ATU_0000000A`.
- The bed pack references the ten room-centric channel formats, `AC_00011001` through `AC_0001100A`.
- Bed channels now use room-centric speaker labels `RC_L`, `RC_R`, `RC_C`, `RC_LFE`, `RC_Lss`, `RC_Rss`, `RC_Lrs`, `RC_Rrs`, `RC_Lts`, and `RC_Rts`.

### 5.4 Object and ID Differences

Original object IDs:

- Bed object is `AO_1001`.
- Object tracks start at `AO_100B`, because `AO_1001` plus ten bed UIDs/channels occupy the first Atmos bed allocation.
- Object packs start at `AP_00031002`; the bed pack is `AP_00011001`.

Earlier CULT object IDs:

- Bed channels occupy separate objects `AO_1001` through `AO_100A`.
- Object tracks start at `AO_100B` only if there are exactly ten bed objects before them, but the bed is not represented as a single Atmos bed object.
- Object packs currently start at `AP_00031001`.

Current CULT object IDs:

- Bed object: `AO_1001`
- First object: `AO_100B`
- First object pack: `AP_00031002`
- First object channel/stream/track format: `AC_0003100B`, `AS_0003100B`, `AT_0003100B_01`
- First object track UID: `ATU_0000000B`

Finding:

- EBU ID rules allow custom IDs above the common-definition range, and both uppercase/lowercase hex must be accepted when reading. The mismatch is probably not merely letter case.
- The more important mismatch was semantic: Dolby/Logic appears to expect one bed object/pack with multiple bed UIDs, not ten independent direct-speaker bed objects. CULT now follows that shape for Atmos-style beds.

### 5.5 Current Bed-Model Candidate Verification

Candidate:

- `exported/lusid_package_logic_bedmodel.wav`

Authoring report:

- Status: `pass`
- Audio: 78 channels, 48 kHz, 24-bit, 97.9936 seconds.
- Package metadata: 36 LUSID frames, 78 unique nodes:
  - 9 direct speakers
  - 1 LFE
  - 68 audio objects
- Warning: one trailing sample was ignored from the longer object WAV files under the existing EOF tolerance.

Container chunk order remains:

```text
RIFF/WAVE
JUNK 40
fmt  16
chna 3124
axml 657821
data 1100664396
```

Representative `chna` entries now match the original Dolby/Logic bed allocation pattern:

```text
1   ATU_00000001  AT_00011001_01  AP_00011001
2   ATU_00000002  AT_00011002_01  AP_00011001
...
10  ATU_0000000A  AT_0001100A_01  AP_00011001
11  ATU_0000000B  AT_0003100B_01  AP_00031002
12  ATU_0000000C  AT_0003100C_01  AP_00031003
```

This removes the strongest metadata-shape mismatch found so far. If Logic still rejects this candidate, the remaining lead suspects are the missing Dolby `dbmd` chunk and/or Logic sensitivity to the source chunk order (`data`, then `axml`, `chna`, `dbmd`).

Logic result:

- `exported/lusid_package_logic_bedmodel.wav` still failed Logic import.
- This means the previous per-bed-object modeling error was real but not the only compatibility blocker.

Additional diagnostic candidates:

- `exported/lusid_package_logic_bedmodel_postdata.wav`
  - Same authored audio/XML/CHNA payload.
  - Chunk order changed to `JUNK`, `fmt `, `data`, `axml`, `chna`.
- `exported/lusid_package_logic_bedmodel_postdata_dbmd.wav`
  - Same post-data chunk order.
  - Adds the extracted original Dolby/Logic `dbmd` chunk payload after `chna`.

These are validation artifacts, not final implementation yet. They are intended to isolate whether Logic is rejecting the pre-data metadata chunk order, the lack of `dbmd`, or both.

### 5.6 Reference Notes

Local references supplied:

- `internalDocsMD/references/bwf-ref.pdf`
- `internalDocsMD/references/axml-reading.pdf`
- `internalDocsMD/references/ebuNotes.md`

External EBU ADM guideline pages used for this analysis:

- EBU ADM "Use of IDs": https://adm.ebu.io/reference/excursions/use_of_ids.html
- EBU ADM "BW64 and ADM": https://adm.ebu.io/reference/excursions/bw64_and_adm.html
- EBU ADM "CHNA Chunk": https://adm.ebu.io/reference/excursions/chna_chunk.html
- EBU ADM "Timing": https://adm.ebu.io/reference/excursions/timing.html

Relevant reference conclusions:

- EBU describes `axml` as the ADM XML metadata chunk and `chna` as the track allocation chunk that links audio tracks to `audioTrackUID`, `audioTrackFormatID`, and `audioPackFormatID`.
- The `chna` `trackIndex` is 1-based and corresponds directly to interleaved track order in the `data` chunk.
- ID shapes are fixed: `ATU_zzzzzzzz`, `AP_yyyyxxxx`, `AC_yyyyxxxx`, `AS_yyyyxxxx`, and `AT_yyyyxxxx_zz`.
- Timing should not run past file/object duration, and successive `audioBlockFormat` entries should be chronological and contiguous.

### 5.7 Revised Compatibility Hypotheses

Highest priority:

1. Test `exported/lusid_package_logic_bedmodel.wav` in Logic.
2. Preserve/synthesize Dolby `dbmd` metadata, or at least determine whether Logic rejects ADM BWF without it.
3. Decide whether chunk order must follow the source file pattern (`data`, then `axml`, `chna`, `dbmd`) for Logic import compatibility.
4. Re-author from the full original extracted LUSID/audio set only if the goal is a faithful round trip. For fresh source authoring, the reduced 78-channel package remains a valid target now that the Atmos bed profile shape is corrected.

Lower priority:

- File extension: the known source is `.wav`; `.adm.wav` is not required.
- Top-level FourCC: the known source is `RIFF`, not `BW64`.
- `fmt ` extensible: the known source uses classic PCM `fmt ` size 16.
- `bext`: no top-level `bext` was found in the source file's chunk list.
