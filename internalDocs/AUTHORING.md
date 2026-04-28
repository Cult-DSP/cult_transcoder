# ADM Authoring Extension — Implementation Decisions

This document is the implementation and validation record for `adm-author` and closely related ADM package work. It captures the concrete decisions, Logic Pro findings, compatibility experiments, and roundtrip evidence behind the current code.

## Documentation Map

- `internalDocs/AUTHORING.md` is the canonical internal authoring document: contract snapshot, implementation record, manual validation log, compatibility hypotheses, and resolved investigations.
- `internalDocs/admAuthoring.md` now exists only as a legacy redirect to this file.
- `internalDocs/authoringTests.md` now exists only as a legacy redirect to this file.
- `src/authoring/README.md` is the code-owner map for the authoring module.
- `src/packaging/README.md` is the code-owner map for ADM WAV -> LUSID package generation.
- `internalDocs/audit.md` explains how CULT authoring relates to SpatialSeed pipeline wiring.

Rule of thumb: update this file for both stable contract changes and durable validation findings. Keep the smaller legacy files as pointers only unless a downstream workflow still requires them.

## Implementation Entry Points (April 2026 module move)

Authoring code now lives under `src/authoring/`.

- Entrypoint orchestration: `src/authoring/adm_author.cpp`
- Mapping + BW64 writer implementation: `src/authoring/adm_writer.cpp`
- Writer interface: `src/authoring/adm_writer.hpp`

Notes:

- Ingest/parity-critical `adm_xml -> lusid_json` paths are intentionally untouched by authoring compatibility work.
- ADM authoring and ADM WAV -> LUSID package generation are separate workflows even though they share ADM/LUSID concepts.

## 0. Stable Contract Snapshot

This section consolidates the durable contract material that previously lived in `admAuthoring.md`.

### 0.1 Purpose

Support LUSID -> authored ADM in `cult_transcoder` (C++), producing:

- `export.adm.xml`
- `export.wav`
- report JSON

The practical acceptance target for v1 is successful import into Logic Pro Atmos plus successful conversion through the Dolby Atmos Conversion Tool, even if Dolby still shows an unsupported-master approval warning.

### 0.2 Architectural Framing

- This feature is an export-side extension layered on top of the canonical LUSID scene model.
- It must not redefine or regress the parity-critical `adm_xml` / `adm_wav -> lusid_json` ingest path.
- CULT owns scene interpretation, mapping policy, deterministic ordering, and loss reporting.
- Authoring compatibility fixes and ADM WAV -> LUSID package generation are related but separate workflows.

### 0.3 Input / Output Contract

Accepted inputs:

1. LUSID package folder containing `scene.lusid.json` and referenced WAV assets.
2. Explicit `scene.lusid.json` plus WAV directory path.

LUSID scene contract:

- accepted scene metadata is LUSID Scene v1.0 (`version: "1.0"`)
- the canonical schema is `LUSID/SCHEMA/lusid_scene_v1.0.schema.json` in the SpatialSeed workspace
- `duration`, when present, is top-level seconds and must match normalized audio duration
- authoring accepts frame timestamps in `seconds`/`s`, `milliseconds`/`ms`, or `samples`/`samp`; sample timestamps require `sampleRate`
- authoring converts accepted frame timestamps to seconds before ADM block generation
- authored ADM timing stays in plain wallclock form and now emits at least 5 fractional digits, extending up to 9 digits when needed to preserve sample-spaced object block timing
- unsupported metadata child nodes such as `spectral_features` and `agent_state` are ignored by ADM authoring

Current WAV assumptions:

- authored source assets are mono WAVs
- authoring normalizes to mono 48 kHz float32 before interleaving
- required assets must exist and unsupported layouts fail explicitly
- one trailing-sample EOF spread is tolerated by truncating to the shortest normalized length
- larger frame mismatches are hard failures

Outputs:

- `export.adm.xml`
- `export.wav`
- report JSON

Atomicity:

- all three outputs must be written via temp + rename

### 0.4 CLI Contract

Implemented `adm-author` command:

```bash
build/cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>] \
  [--metadata-post-data]

# alternate input
build/cult-transcoder adm-author \
  --lusid-package <path> \
  --out-xml <export.adm.xml> \
  --out-wav <export.wav> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--dbmd-source <source.wav|dbmd.bin>] \
  [--metadata-post-data]
```

Exit codes:

- `0`: success, final outputs exist
- `1`: authoring failure
- `2`: argument/usage error

Contract rules:

- keep existing `transcode` command semantics unchanged
- do not overload ingest behavior to mean authoring
- emit best-effort fail reports on errors

### 0.5 Mapping Policy

Supported node types:

- `direct_speaker`
- `audio_object`
- `LFE`

Mapping rules:

- `direct_speaker` nodes map to the authored bed/direct-speaker structure
- `audio_object` nodes map to authored ADM objects
- `LFE` maps inside the bed, not as a free object
- Logic-compatible output uses one `Master` bed object/pack
- deterministic ordering is required: bed first in fixed template order, then objects in ascending LUSID id/group order, then motion blocks by ascending frame time
- object authoring uses step-hold `audioBlockFormat` timing for v1

Loss policy:

- unsupported node types, metadata, or motion semantics must be reported explicitly
- missing WAVs, unsupported formats, normalization failure, or duration/frame mismatches beyond the one-sample EOF tolerance are hard failures

### 0.6 Progress Contract

Both `adm-author` and `package-adm-wav` expose `ProgressCallback` through `src/progress.hpp`.

- CLI progress bars render to stderr and can be disabled with `--quiet`
- API callers use `AdmAuthorRequest::onProgress` or `PackageAdmWavRequest::onProgress`
- current phases include `metadata`, `inspect`, `normalize`, `interleave`, and `split`

Future long-running transcoding work should reuse this callback contract instead of inventing command-specific progress paths.

### 0.7 Implementation Status

Implemented:

- separate `AdmAuthorRequest` / `AdmAuthorResult` API
- `adm-author` CLI parsing and dispatch
- LUSID package / explicit scene+WAV resolution
- validation before output writing
- deterministic ADM XML generation through CULT-owned mapping rules
- ADM BWF/WAV writing through `libbw64`
- Logic-shaped bed modeling, `chna`, and embedded `axml`
- separate `package-adm-wav` path for ADM WAV -> LUSID package generation
- `package-adm-wav` now shares one parsed ADM `pugi::xml_document` between profile detection and generic metadata conversion, matching the `transcode` single-parse metadata path

Open items:

- stereo-pair reconstruction from adjacent mono L/R ADM tracks is not implemented
- Dolby-approved-master recognition remains future work and is not a v1 blocker

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

### 5.6 Logic-Shaped AXML Experiment

Logic result:

- `exported/lusid_package_logic_bedmodel_postdata.wav` did not import.
- `exported/lusid_package_logic_bedmodel_postdata_dbmd.wav` did not import.

Conclusion:

- The failure is no longer explained by filename, bed-object shape, chunk order, or merely appending the original `dbmd` payload.
- The remaining lead is stricter Dolby/Logic interpretation of the ADM XML profile inside `axml`.

Observed AXML differences from the original Logic-authored BWF:

- Original time attributes use plain ADM time strings such as `00:00:00.00000`; CULT previously emitted sample-rate suffixed values such as `00:00:00.00000S48000`.
- Original direct-speaker bed blocks include `speakerLabel`, a `cartesian` element, and room-centric `position` elements. CULT previously emitted only `speakerLabel`.
- Original object blocks include a `cartesian` element and `jumpPosition interpolationLength="0"` child. CULT previously represented `cartesian` as an attribute and omitted `jumpPosition`.
- Original `audioContent` ends with `<dialogue mixedContentKind="0">2</dialogue>`. CULT previously omitted this marker.
- Original AXML groups all `audioObject` elements first, then all packs, then all channels, then streams, track formats, and track UIDs. CULT previously interleaved object, pack, and channel elements per object.

Implemented follow-up:

- `AdmWriter::formatAdmTime()` now emits plain `HH:MM:SS.fffff...` wallclock time strings for authored ADM XML, keeping the original 5-digit shape for exact 5-decimal values and extending to 9 fractional digits when sample-spaced object timing needs more precision.
- Direct-speaker bed blocks now include `cartesian` and room-centric positions matching the original bed layout.
- Object blocks now emit `cartesian` as a child element and include `<jumpPosition interpolationLength="0">1</jumpPosition>`.
- `audioContent` now emits the original-style `dialogue` marker.
- XML generation now groups elements in the Logic-observed order: content refs, audio objects, packs, channels, streams, track formats, track UIDs.

### 5.6.1 Time Compatibility Audit Follow-Up

Audit result:

- LUSID v1 millisecond and sample timestamp inputs were already normalizing correctly through `convertSceneTimeToSeconds()`.
- Shared ADM time parsing in `adm_helpers::parseTimecodeToSeconds()` already accepted both plain wallclock values and sample-rate-suffixed fixture strings well enough for the current ingest/parity path.
- The real mismatch was authoring precision: plain 5-decimal output preserved 48 kHz frame ordering, but it lost sample-resolution fidelity on authored object-block roundtrip.

Implemented follow-up:

- `AdmWriter::formatAdmTime()` now keeps plain wallclock ADM timing but extends fractional precision up to 9 digits when needed, while retaining 5-digit output for exact 5-decimal values such as `00:00:00.00500`.
- Added coverage for millisecond input normalization, sample-spaced authored object blocks, sample-resolution authoring roundtrip, and sample-rate-suffixed ADM helper parsing.

Current validation candidate for this experiment:

- `exported/lusid_package_logic_shaped.wav`
- `exported/lusid_package_logic_shaped.adm.xml`
- `exported/lusid_package_logic_shaped.report.json`

### 5.7 Reference Notes

Local references supplied:

- `internalDocs/references/bwf-ref.pdf`
- `internalDocs/references/axml-reading.pdf`
- `internalDocs/references/ebuNotes.md`

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

### 5.8 Revised Compatibility Hypotheses

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

### 5.9 Logic Validation Result and Current State

Logic result:

- `exported/lusid_package_logic_shaped.wav` imported successfully in Logic Pro.

Conclusion:

- The blocker was the authored ADM XML profile shape, not the `.wav` extension, the lack of a copied `dbmd` payload, or pre/post-data placement of `axml` and `chna`.
- The current authoring implementation should keep the Logic-shaped ADM XML conventions listed in section 5.6.
- Future compatibility work should treat `dbmd` preservation/synthesis as optional metadata hardening, not as the known primary import fix.

### 5.10 Dolby Atmos Conversion Tool Result

Manual validation with the Dolby Atmos Conversion Tool produced a stricter result than Logic Pro import.

Observed messages:

- On import: `Opening an unsupported master. This file is invalid or was not created by Dolby approved software. It may not open or play back correctly.`
- Earlier conversion attempt displayed: `Conversion output error: The time entered for the FFOA must be in a range between the start time and end time. Enter a new time greater or equal to 00:00:00:00 and less than 00:00:00:00.`
- Later validation showed the FFOA message was not blocking: the Dolby Atmos Conversion Tool successfully converted the file, and the converted file opened in Logic.

Interpretation:

- Logic-compatible ADM BWF output can also be usable by the Dolby Atmos Conversion Tool.
- The remaining import warning indicates the authored file is not recognized as a Dolby-approved master. This appears to be a provenance/certification warning rather than a conversion blocker.
- The FFOA message should not be treated as the current blocker because conversion succeeded and the converted output opened in Logic.

Follow-up:

- If Dolby-approved-master recognition becomes a target, compare accepted Dolby/Logic source BWF metadata against CULT output for private/provenance fields.
- Continue to preserve the distinction between conversion usability and Dolby approval status.
- Treat Dolby-approved-master recognition as future work for now. It is not a v1 blocker for CULT authoring as long as Logic import and Dolby-tool conversion remain successful.

Implemented experiment:

- `adm-author` now supports `--dbmd-source <source.wav|dbmd.bin>` as an experimental path for copying a Dolby `dbmd` payload into authored output.
- Authored ADM XML now writes an explicit nonzero `end` attribute alongside `start` and `duration` on `audioProgramme` and `audioObject` elements.
- `adm-author` now supports `--metadata-post-data` as an experimental path for rewriting authored WAV chunk order so `axml`, `chna`, and optional `dbmd` appear after `data`, matching the observed Dolby/Logic source layout.
- Automated tests verify `end`/`duration` XML output, `dbmd` chunk emission, and post-data chunk ordering.

Candidate:

- `exported/eden_dolby_candidate.wav`
- `exported/eden_dolby_candidate.adm.xml`
- `exported/eden_dolby_candidate.report.json`
- `exported/eden_dolby_postdata_candidate.wav`
- `exported/eden_dolby_postdata_candidate.adm.xml`
- `exported/eden_dolby_postdata_candidate.report.json`

These candidates use `dbmd` extracted from `/Users/lucian/projects/spatialroot/sourceData/EDEN-ATMOS-MIX-LFE.wav`. The post-data candidate additionally writes the observed source-style order `JUNK`, `fmt `, `data`, `axml`, `chna`, `dbmd`.

Validation result:

- Dolby Atmos Conversion Tool converted the file successfully.
- The converted output opened in Logic.
- The unsupported-master warning remains and should be tracked separately from structural conversion success.
- The post-data candidate did not clear the unsupported-master warning.
- Current conclusion: the remaining warning is most likely tied to Dolby-approved provenance/private metadata rather than basic ADM BWF structure.
- This should now be treated as future work unless Dolby-approved-master recognition becomes a product requirement.

## 6. ADM WAV -> LUSID Package Generation

This is a separate workflow from ADM authoring fixes.

- `adm-author` consumes a LUSID package or explicit `scene.lusid.json` + mono WAV directory and produces authored ADM XML plus an ADM BWF/WAV.
- `package-adm-wav` consumes an existing ADM BWF/WAV and produces a LUSID package directory.

The two flows share some ADM/LUSID mapping code, but they solve different problems and should not be conflated during debugging.

### 6.1 Package Command

Implemented command:

```bash
build/cult-transcoder package-adm-wav \
  --in <source.wav> \
  --out-package <package-dir> \
  [--report <path>] [--stdout-report] [--quiet] \
  [--lfe-mode hardcoded|speaker-label]
```

Package outputs:

- `scene.lusid.json`
- one mono float32 WAV stem per generated LUSID node
- `channel_order.txt`
- `scene_report.json`

The command is self-contained. It extracts embedded ADM XML through CULT/libbw64, converts ADM metadata to LUSID, and splits the interleaved audio data with an internal streaming RIFF/BW64 reader. It does not require ffmpeg, libsndfile, or host-project audio utilities.

### 6.2 Stem Splitting Policy

Current package-generation behavior:

- Split the interleaved ADM WAV data into mono 48 kHz float32 stems.
- Name stems by the generated LUSID node IDs. `4.1` is written as `LFE.wav`.
- Write `channel_order.txt` so the package records the exact generated node order.
- Use existing LFE detection policy from ingest (`hardcoded` by default, `speaker-label` opt-in).
- Validate `chna` presence/count/contiguity where possible and warn if CULT falls back to canonical LUSID order.

Open issue:

- CULT does not currently reconstruct stereo objects from adjacent mono L/R stems. ADM object tracks are packaged as mono LUSID objects. Stereo-pair inference is future work and must be explicit because false pairing would be worse than preserving mono tracks.

### 6.3 Progress Reporting

Both `adm-author` and `package-adm-wav` now expose `ProgressCallback` in the public API.

CLI behavior:

- Progress bars are written to stderr.
- JSON reports still go to the report file and optional stdout, so progress does not contaminate `--stdout-report`.
- Use `--quiet` to disable progress output.

API behavior:

- Include `src/progress.hpp`.
- Set `AdmAuthorRequest::onProgress` or `PackageAdmWavRequest::onProgress`.
- Each callback receives `ProgressEvent { phase, current, total, detail }`.
- Current phases include `metadata`, `inspect`, `normalize`, `interleave`, and `split`.

Future transcoding tasks should reuse this callback contract instead of inventing command-specific progress channels.

### 6.4 EDEN Roundtrip Validation

Source:

- `/Users/lucian/projects/spatialroot/sourceData/EDEN-ATMOS-MIX-LFE.wav`

Validated flow:

1. `package-adm-wav` generated a self-contained LUSID package.
2. `adm-author` re-authored that package back to ADM BWF/WAV.
3. Both commands completed successfully with progress output enabled.

Generated validation artifacts:

- `exported/eden_roundtrip_package_cpp/`
- `exported/eden_roundtrip_package_cpp.report.json`
- `exported/eden_roundtrip_reauthored.wav`
- `exported/eden_roundtrip_reauthored.adm.xml`
- `exported/eden_roundtrip_reauthored.report.json`

Observed facts:

- Package generation wrote 46 mono float32 stems plus `scene.lusid.json`, `channel_order.txt`, and `scene_report.json`.
- `channel_order.txt` contained 46 generated node IDs/stems.
- Package report status was `success`.
- Re-author report status was `pass`.
- Re-authored output duration was 98 seconds at 48 kHz.
- Re-authored summary counted 9 direct speakers, 1 LFE, and 36 audio objects.

The generated files are validation artifacts only. The useful findings are captured here, so the artifacts may be deleted when no longer needed for manual DAW inspection.
