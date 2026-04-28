# CULT Transcoder Data Flow

This document describes what CULT actually does when moving audio scene data
between ADM, ADM BWF/WAV, LUSID Scene JSON, LUSID packages, and authored ADM
outputs.

CULT has three major runtime flows:

- `transcode`: ADM XML or ADM WAV metadata to LUSID Scene JSON
- `package-adm-wav`: ADM BWF/WAV to a self-contained LUSID package
- `adm-author`: LUSID Scene JSON or LUSID package back to ADM XML plus ADM BWF/WAV

The ingest path is parity-sensitive. Generic ADM conversion is still based on
raw `pugixml` traversal and must preserve encounter order, frame ordering, node
IDs, time rounding, LFE behavior, and JSON output shape unless a tested behavior
change explicitly updates the contract.

## Shared Scene Model

The internal scene model is `LusidScene`:

- `version`: currently `"1.0"`
- `timeUnit`: normalized to `"seconds"` inside CULT
- `sampleRate`
- optional `duration`
- `metadata`
- ordered `frames`
- ordered `nodes` inside each frame

Supported audio node types are:

- `direct_speaker`
- `audio_object`
- `LFE`

Authoring-side parsing also recognizes non-audio node types such as
`spectral_features` and `agent_state`, but those nodes do not become authored
ADM audio tracks.

## Memory and Data Transfer Overview

CULT currently uses an in-memory metadata model and chunked audio transfer.
Metadata is small enough for current workloads to keep as XML DOM, `LusidScene`,
and JSON text. Audio is treated differently: large sample payloads are streamed
or processed in fixed-size chunks wherever practical.

The major allocation and transfer points are:

| Data | Allocation / Owner | Transfer Pattern |
| ---- | ------------------ | ---------------- |
| ADM XML file | `pugi::xml_document` in `transcode()` for `adm_xml` | parsed once, borrowed by profile detection and conversion |
| ADM `axml` from WAV | `std::string` returned by `extractAxmlFromWav()` | copied from WAV chunk into memory, then parsed into `pugi::xml_document` |
| XML traversal results | vectors of `pugi::xml_node` handles | handles refer back to the owning `pugi::xml_document`; they do not copy XML subtrees |
| Generic direct speakers | `std::vector<SpeakerInfo>` inside conversion | small staging list used to preserve direct-speaker-first output |
| Generic object blocks | no full object-block staging vector | each block is converted directly into `timeToNodes` |
| `timeToNodes` | `std::map<double, std::vector<LusidNode>>` | owns pending LUSID nodes grouped by parsed frame time |
| `LusidScene` | returned in `ConversionResult` | owns final frames/nodes after moving node vectors out of `timeToNodes` |
| LUSID JSON output | `std::ostringstream` / `std::string` | full JSON string is materialized before file write |
| Package source audio | file stream plus chunk buffer | decoded in chunks and written to mono stem streams |
| Authoring normalized audio | temporary normalized WAV files | source stems are normalized to files, then read back for ADM BWF writing |
| Authored ADM XML | `pugi::xml_document` plus serialized XML string | XML DOM is serialized to a full string for sidecar and BWF `axml` embedding |

Important lifetime rule:

- any `pugi::xml_node` gathered during traversal is only a lightweight handle
  into the owning `pugi::xml_document`
- callers must keep the document alive until profile detection, conversion, and
  any node-handle traversal are complete

Current non-streaming metadata points:

- ADM XML is DOM-parsed before conversion
- LUSID JSON serialization builds a complete string before writing
- authored ADM XML is built as a DOM and serialized as a complete string

These are intentional for the current implementation. Streaming JSON and deeper
metadata streaming are deferred cleanup topics, not part of the sensitive
transcoding cleanup already completed.

## Flow 1: ADM XML to LUSID JSON

CLI shape:

```bash
cult-transcoder transcode \
  --in <input.xml> \
  --in-format adm_xml \
  --out <scene.lusid.json> \
  --out-format lusid_json
```

Actual steps:

1. `src/transcoder.cpp` validates input/output arguments.
2. It parses the ADM XML once into a `pugi::xml_document`.
3. `resolveAdmProfile(doc)` inspects the parsed document for known profile
   signals.
4. If Sony 360RA is detected, dispatch goes to
   `convertSony360RaToLusid(doc, lfeMode)`.
5. Otherwise generic ADM dispatch goes to
   `convertAdmDocumentToLusid(doc, lfeMode)` using the same parsed document.
6. The resulting `LusidScene` is serialized with `lusidSceneToJson()`.
7. The output JSON is written, and the report records warnings, summary counts,
   frame count, sample rate, duration, and node type counts.

Important ownership rule:

- profile detection and generic conversion share the same parsed
  `pugi::xml_document`
- do not reintroduce a second parse in `transcode()` unless a tested behavior
  change requires it

## Flow 2: ADM WAV to LUSID JSON

CLI shape:

```bash
cult-transcoder transcode \
  --in <input.wav> \
  --in-format adm_wav \
  --out <scene.lusid.json> \
  --out-format lusid_json
```

Actual steps:

1. `extractAxmlFromWav()` reads the embedded `axml` chunk from the ADM BWF/WAV.
2. `src/transcoder.cpp` parses that extracted XML buffer once into a
   `pugi::xml_document`.
3. Profile detection runs on the parsed document.
4. Sony 360RA dispatch uses `convertSony360RaToLusid(doc, lfeMode)`.
5. Generic ADM dispatch uses `convertAdmDocumentToLusid(doc, lfeMode)`.
6. The output LUSID JSON and report are written just like the ADM XML path.

The audio samples in the source WAV are not decoded by `transcode`. This command
converts ADM metadata to LUSID Scene JSON only.

Memory behavior:

- `extractAxmlFromWav()` materializes the embedded ADM XML chunk as a
  `std::string`
- that string is parsed into a `pugi::xml_document`
- the XML string remains owned by the extraction result until the transcode
  branch exits, but generic conversion reads from the parsed document
- source WAV audio samples are not allocated, decoded, or copied in this flow

## Generic ADM Metadata Conversion

Generic ADM conversion lives in `src/transcoding/adm/adm_to_lusid.cpp`.

The converter searches for `ebuCoreMain`, extracts global metadata from
`Technical`, then walks all descendant `audioChannelFormat` elements in document
encounter order.

### Direct Speakers

Direct speakers are collected first:

1. Find `audioChannelFormat` elements where `typeDefinition="DirectSpeakers"`.
2. Use the first child `audioBlockFormat`.
3. Read Cartesian `position coordinate="X"`, `Y`, and `Z`.
4. Preserve `speakerLabel` and channel ID when present.
5. Emit all direct speakers into the `time=0.0` LUSID frame.

LFE behavior:

- default mode is hardcoded parity behavior: the fourth direct speaker
  encountered, 1-based, becomes node type `LFE`
- `--lfe-mode speaker-label` instead promotes labels `LFE` or `LFE1`
- `LFE` nodes do not carry `cart`
- non-LFE direct speakers carry `cart`

Direct speaker IDs are assigned as:

```text
1.1, 2.1, 3.1, ...
```

### Object Positional Frames

Generic ADM object motion is represented as discrete LUSID frames.

For every `audioChannelFormat` where `typeDefinition="Objects"`:

1. The converter skips channels with no `audioBlockFormat` children.
2. It assigns one object group ID for that channel, after all direct speaker
   IDs.
3. It iterates that channel's `audioBlockFormat` children in XML order.
4. For each block, it reads:
   - `rtime`, defaulting to `00:00:00.00000`
   - Cartesian `position` coordinates `X`, `Y`, and `Z`
5. It parses `rtime` to seconds.
6. It writes a LUSID `audio_object` node directly into `timeToNodes[timeSec]`.

The current implementation no longer builds a full intermediate
`audioObjects` vector. Object block nodes are written directly into the
time-keyed map after direct speakers are collected. This reduces peak memory
and removes an extra pass while preserving parity behavior.

Memory behavior for generic object frames:

- `channelFormats` is a vector of `pugi::xml_node` handles, not copied XML
- `directSpeakers` stores only the small static bed metadata needed to emit the
  `time=0.0` frame first
- each object `audioBlockFormat` is read, converted to a `LusidNode`, and pushed
  into `timeToNodes[timeSec]`
- when final frames are built, each `timeToNodes` node vector is moved into a
  `LusidFrame`, avoiding a second node-vector copy at that boundary
- the removed `audioObjects` staging vector means object block positions are no
  longer duplicated before they become LUSID nodes

The resulting structure is:

```text
ADM audioBlockFormat rtime  ->  LUSID frame.time
ADM block X/Y/Z             ->  LUSID node.cart
ADM object channel order    ->  LUSID node id group order
```

Example shape:

```json
{
  "time": 1.5,
  "nodes": [
    {
      "id": "11.1",
      "type": "audio_object",
      "cart": [0.0, 1.0, 0.0]
    }
  ]
}
```

Frame behavior:

- `timeToNodes` is a `std::map<double, vector<LusidNode>>`, so frames are emitted
  in ascending time order
- multiple object blocks with the same parsed `rtime` become multiple nodes in
  the same LUSID frame
- node order inside a frame follows conversion insertion order
- frame times are rounded to 6 decimal places when written into `LusidScene`
- no interpolation is performed during ingest
- duration attributes on ADM object blocks are not represented as LUSID frame
  durations in this path; the next authored/exported duration is derived later
  if the scene is passed to `adm-author`

## Sony 360RA ADM Conversion

Sony 360RA dispatch lives in `src/transcoding/adm/sony360ra_to_lusid.cpp`.

Profile detection recognizes Sony 360RA by profile signals such as `360RA` in
ADM attributes. Once detected, the generic ADM converter is bypassed.

Sony 360RA conversion differs from generic ADM:

- it navigates to `audioFormatExtended` in the Sony/bwfmetaedit wrapper
- it skips container `audioObject` elements
- it iterates leaf `audioObject` elements in document encounter order
- it resolves each leaf object through pack and channel references
- it reads the first active, non-muted, non-zero-distance `audioBlockFormat`
- it converts ADM polar coordinates to LUSID Cartesian coordinates
- it emits a single LUSID frame at `time=0.0`

Sony 360RA currently treats object positions as static for LUSID output. The
gain-mute section-boundary pattern is discarded, and gain is not written into
LUSID.

Sony 360RA output:

- all nodes are `audio_object`
- node IDs are `1.1`, `2.1`, etc. in leaf object encounter order
- metadata `sourceFormat` is `Sony360RA-ADM`
- `--lfe-mode` has no effect because the format does not carry DirectSpeakers
  or LFE channels in this converter

## Flow 3: ADM BWF/WAV to LUSID Package

CLI shape:

```bash
cult-transcoder package-adm-wav \
  --in <input.wav> \
  --out-package <package-dir>
```

Actual steps:

1. Extract embedded `axml` from the ADM BWF/WAV.
2. Parse the XML once into a `pugi::xml_document`.
3. Run profile detection on that parsed document.
4. Convert ADM metadata to a `LusidScene` using the same parsed document.
5. Inspect the source WAV container and audio format.
6. Determine canonical node order from the converted LUSID scene.
7. Write `scene.lusid.json`.
8. Write `channel_order.txt`.
9. Split interleaved source audio into package-local mono float32 WAV stems.
10. Write `scene_report.json`.
11. Rename the temporary package directory into place.

Package channel/stem order is derived from LUSID node IDs:

- bed-like IDs `1.1` through `10.1` sort first
- remaining IDs sort lexically
- node `4.1` maps to `LFE.wav`
- other nodes map to `<id>.wav`

The package path keeps metadata conversion and audio splitting separate:

- metadata comes from ADM `axml`
- audio samples come from the source WAV `data` chunk
- stems are decoded to mono float32 WAV files

Important ownership rule:

- `package-adm-wav` keeps one parsed `pugi::xml_document` alive for both
  profile detection and generic ADM conversion
- do not reintroduce `convertAdmToLusidFromBuffer()` on this path unless a
  tested behavior change requires reparsing

If `chna` metadata is missing or does not match expectations, CULT warns and
uses canonical LUSID order.

Memory behavior:

- metadata conversion owns a full `LusidScene` so package writing can produce
  `scene.lusid.json`, `channel_order.txt`, report summaries, and stem order
- package audio splitting does not load the whole source WAV into memory
- `splitInterleavedToMono()` uses a fixed chunk buffer of up to 4096 source
  frames at a time
- each decoded sample is transferred from the interleaved chunk buffer to the
  appropriate mono output stream as float32
- all mono stem files are open during splitting so each chunk can be deinterleaved
  across channels in one pass
- package finalization happens through a temporary directory followed by rename

Implementation note:

- Sony 360RA package conversion reuses the parsed document
- generic package conversion now follows the same single-parse metadata path as
  `transcode`: extract `axml`, parse once, detect profile, and convert from the
  already-loaded document

## Flow 4: LUSID JSON or Package to ADM XML and ADM BWF/WAV

CLI shape:

```bash
cult-transcoder adm-author \
  --lusid <scene.lusid.json> \
  --wav-dir <mono-stems-dir> \
  --out-xml <output.xml> \
  --out-wav <output.wav>
```

or:

```bash
cult-transcoder adm-author \
  --lusid-package <package-dir> \
  --out-xml <output.xml> \
  --out-wav <output.wav>
```

Actual steps:

1. Resolve the scene path and WAV directory.
2. Read LUSID Scene JSON with `readLusidScene()`.
3. Validate LUSID v1.0 structure and supported audio nodes.
4. Normalize accepted `timeUnit` values to internal seconds:
   - `seconds` / `s`
   - `milliseconds` / `ms`
   - `samples` / `samp`
5. Collect required node IDs.
6. Locate matching mono WAV stems.
7. Normalize audio to 48 kHz float32 for authoring.
8. Validate normalized frame counts and scene duration.
9. Generate ADM XML from the LUSID scene.
10. Interleave normalized mono WAVs into ADM BWF/WAV.
11. Embed `axml` and `chna`.

Authoring is the reverse direction from ingest, but it is not a byte-for-byte
inverse. It writes a Logic-compatible ADM shape from LUSID's canonical scene
model.

### Authoring Object Motion

`AdmWriter` builds a timeline per LUSID node ID:

1. Iterate all LUSID frames.
2. Collect supported audio nodes by `id`.
3. Store each node pointer and its frame time.
4. Sort each node timeline by time.
5. Sort node IDs with beds first, then objects.

For `audio_object` nodes, authoring writes one ADM `audioBlockFormat` per LUSID
frame occurrence:

- `rtime` is the LUSID frame time formatted as ADM time
- `duration` is the time until the next frame for that node
- the final block duration extends to total authored audio duration
- blocks starting at or after total duration are skipped
- zero-duration blocks are skipped
- `cartesian` is set to `1`
- `X`, `Y`, and `Z` come from the LUSID node `cart`
- `jumpPosition interpolationLength="0"` is written, expressing step-hold motion

This means LUSID frames are interpreted as position changes that hold until the
next position for the same object.

### Authoring Direct Speakers and LFE

Beds are recognized by IDs `1.1` through `10.1`.

Authoring writes:

- one master bed object when bed IDs are present
- deterministic room-centric DirectSpeakers channel names and speaker labels
- bed channel positions from CULT's fixed bed table
- LFE as bed index `4.1`, written as the room-centric LFE bed channel

The authoring path uses normalized WAV frame count as canonical duration. Scene
duration must match the normalized audio duration unless the only mismatch is
the tolerated one trailing frame case.

Memory behavior:

- `readLusidScene()` materializes the LUSID JSON into a `LusidScene`
- `AdmWriter` builds per-node timelines as vectors of pointers into the
  existing `LusidScene` nodes plus parallel vectors of frame times
- those timeline pointers are non-owning; the source `LusidScene` must remain
  alive while ADM XML is generated
- generated ADM XML is held in a `pugi::xml_document`, then serialized to a full
  XML string
- normalized audio is transferred through temporary WAV files instead of one
  large in-memory audio buffer
- ADM BWF audio interleaving reads normalized mono sources in chunks and writes
  the interleaved output progressively
- optional metadata post-data rewriting copies existing WAV chunks through a
  fixed-size buffer rather than loading the whole authored WAV

## Reports, Warnings, and Losses

Every major command fills a `Report`:

- `status`
- input/output args
- warnings
- errors
- summary time unit, frame count, sample rate, duration, node type counts
- authoring validation details when authoring
- loss ledger entries for tolerated lossy behavior

On failures, the CLI writes a best-effort fail report when it has enough path
information.

## Invariants

These are the fragile parts to preserve:

- generic ADM ingest uses `pugixml`, not `libadm`
- profile detection does not own conversion semantics
- generic ADM profile detection and conversion should share a parsed document
- direct speakers are emitted before generic ADM objects
- generic ADM object frame times come from `audioBlockFormat/@rtime`
- generic ADM object nodes are written directly into `timeToNodes`
- generic ADM object block metadata is not duplicated in a full staging vector
- frames are emitted in ascending time order
- hardcoded LFE default is the fourth DirectSpeaker encountered
- Sony 360RA dispatch bypasses generic ADM conversion
- Sony 360RA emits a single static frame from first active object positions
- authoring converts LUSID time units to seconds before ADM writing
- authoring object block durations are derived from the next LUSID frame for
  the same object, not from ingest-side ADM duration attributes

Deferred follow-ups:

- streaming JSON output
- richer object metadata and gain handling
- stereo-pair reconstruction in LUSID packages
