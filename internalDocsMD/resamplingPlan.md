# CULT Resampler Implementation Plan

Status: working implementation plan for the new ADM authoring path  
Scope: export-side audio normalization only  
Last updated: 2026-03-31

## 1. Purpose

This document defines the resampling architecture for the new `adm-author` export path in `cult_transcoder`.

The resampler exists for one narrow job: normalize supported mono source WAVs to the fixed ADM authoring target of **48 kHz** before ADM XML writing and BW64 packaging. It is not a general audio-processing subsystem, not a renderer feature, and not part of the parity-critical `adm_xml -> lusid_json` ingest path.

This keeps the implementation aligned with CULT’s current dependency philosophy: external libraries should solve narrowly scoped infrastructure problems while CULT retains ownership of scene interpretation, canonical normalization, loss reporting, and export policy. The design doc also keeps the current v1 core intentionally small and explicitly separates new export-domain work from the parity-critical ADM ingest path. fileciteturn11file8 fileciteturn11file1 fileciteturn11file3

## 2. Architectural placement

### 2.1 What the resampler is allowed to do

The resampler is allowed to:

- inspect source WAV sample rates
- convert supported non-48 kHz mono WAVs to 48 kHz
- write normalized temporary WAV files for the authoring run
- report exactly what was converted
- feed normalized audio into the ADM/BW64 authoring pipeline

### 2.2 What the resampler is not allowed to do

The resampler must not:

- modify the existing `transcode` CLI behavior
- change the `adm_xml -> lusid_json` parity path
- alter LUSID scene semantics
- perform padding, truncation, time-stretching, fold-down, remixing, or loudness processing
- overwrite user source WAVs
- silently "fix" malformed inputs

### 2.3 Position in the new authoring pipeline

The authoring pipeline should be staged like this:

1. resolve input scene/package
2. scan referenced WAV files
3. normalize supported WAVs to internal authoring audio format
4. validate normalized audio set
5. map LUSID scene to ADM authoring model
6. write ADM XML
7. package BW64 with embedded ADM metadata
8. write final report

Resampling belongs only in stage 3.

## 3. Dependency choice: r8brain-free-src

### 3.1 Why this dependency

Use **`r8brain-free-src`** as the resampling backend.

Reasons:

- it is described by its upstream repository as a **header-only C++** sample-rate conversion library citeturn806194view0turn132331view1
- the upstream repository states that it is under the **MIT license** citeturn806194view0
- the upstream README says it is basically header-only and has no dependencies beyond the standard C++ library for modern compilers citeturn132331view2
- the core front-end is the `r8b::CDSPResampler` class, which handles arbitrary source and destination sample rates, including non-integer ratios citeturn132331view1turn806194view1

This fits CULT’s current guidance to prefer small, inspectable, cross-platform dependencies and to isolate external codec or DSP backends from the canonical scene layer. fileciteturn11file8

### 3.2 Why not FFmpeg for this pass

Do not use FFmpeg or `libswresample` as the first implementation choice.

The problem here is narrow: offline normalization of mono WAV inputs to 48 kHz for authoring. Pulling in a larger media framework would add more dependency surface than this job requires. CULT’s design doc already warns against letting external tooling redefine the transcoder’s core architecture. fileciteturn11file6

### 3.3 Precision and implementation note

The r8brain README notes that the library is centered on **double-precision** processing and that the main front-end class is `CDSPResampler`; it also exposes derived variants such as `CDSPResampler24` for 24-bit and 32-bit floating-point use cases. citeturn132331view2turn806194view1

For CULT v1 authoring, we should use a single conservative code path and not expose user-selectable resampler quality modes yet.

## 4. Build and vendoring model

### 4.1 Vendoring policy

Vendor `r8brain-free-src` locally inside CULT rather than making it a system dependency.

Recommended layout:

```text
cult_transcoder/
  third_party/
    r8brain-free-src/
  include/
    audio/
      resampler.hpp
      wav_io.hpp
  src/
    audio/
      resampler_r8brain.cpp
      wav_io.cpp
      normalize_audio.cpp
      validate_audio.cpp
```

This keeps the dependency inspectable and pinned to a known revision, which is consistent with current CULT dependency philosophy. fileciteturn11file8

### 4.2 CMake integration

Because the library is basically header-only for modern compilers, the initial CMake integration should be minimal:

- add the vendored folder to include paths
- do not add a new shared library target
- do not introduce package-manager or system install requirements
- wrap all r8brain use behind one internal source file and one public internal header

Example shape:

```cmake
# third_party include
 target_include_directories(cult_transcoder PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/third_party/r8brain-free-src
 )
```

### 4.3 FFT options

The upstream README documents optional `R8B_PFFFT` and `R8B_PFFFT_DOUBLE` modes that require compiling additional supplied C files if enabled. citeturn132331view3

For CULT v1, do **not** enable those optional FFT modes initially.

Initial rule:

- use the default header-only path first
- do not define `R8B_PFFFT` or `R8B_PFFFT_DOUBLE`
- do not compile extra FFT implementation files in the first pass

This keeps the build simpler and lowers cross-platform risk.

### 4.4 Credit note

The upstream README asks downstream users to credit the creator in documentation. That request should be followed in an internal dependency note or acknowledgements section if you vendor the library. citeturn132331view2

## 5. Internal audio normalization contract

### 5.1 Fixed normalized-audio format

For the `adm-author` path, normalized temporary audio files use a fixed internal format:

- **mono**
- **48,000 Hz**
- **float32 WAV**

This is the normalized working format for authoring.

### 5.2 Final authored output rate

The final authored ADM/BW64 output contract remains fixed at **48 kHz**.

### 5.3 What gets normalized

Normalize only files that require it.

Rules:

- if the input WAV is already mono and 48 kHz, do not resample it
- if the input WAV is mono but not 48 kHz, resample it offline to mono 48 kHz float32
- if the input WAV is not mono, fail in v1
- do not resample already-compliant files just to create identical processing history

### 5.4 Source file safety

Never overwrite source WAVs.

All normalized outputs must be written to a temporary working directory for the authoring run.

## 6. Supported and unsupported inputs

### 6.1 Supported in v1

- readable mono WAV files
- 48 kHz mono WAVs
- non-48 kHz mono WAVs that can be normalized to 48 kHz

### 6.2 Unsupported in v1

- multichannel WAV inputs
- missing referenced WAVs
- unreadable or malformed WAVs
- normalization failures
- any need for automatic fold-down, padding, truncation, or time-stretching

These conditions are hard validation failures, not loss-ledger events.

## 7. Runtime architecture inside CULT

### 7.1 Wrapper boundary

Do not expose r8brain classes outside a single internal wrapper layer.

Create a small internal API:

```cpp
struct AudioFileInfo {
  std::filesystem::path path;
  int channels;
  int sampleRate;
  uint64_t frameCount;
  std::string subtype;
};

struct NormalizeRequest {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
  int sourceSampleRate;
  int targetSampleRate; // always 48000 in v1
  uint64_t sourceFrameCount;
};

struct NormalizeResult {
  std::filesystem::path sourcePath;
  std::filesystem::path normalizedPath;
  int sourceSampleRate;
  int targetSampleRate;
  uint64_t sourceFrameCount;
  uint64_t normalizedFrameCount;
  bool resampled;
};
```

And a wrapper surface like:

```cpp
NormalizeResult normalizeMonoWavTo48kFloat32(const NormalizeRequest& request);
```

### 7.2 r8brain usage pattern

The upstream API centers on `r8b::CDSPResampler`, which performs sample-rate conversion between arbitrary source and destination rates. It can return the input buffer unchanged when the rates are equal, and it owns the internal output buffer exposed by `process()`. citeturn806194view1

Inside CULT:

- create a resampler object inside the wrapper for each file conversion
- read source samples into internal buffers
- convert to double for processing if needed
- feed blocks to the r8brain resampler
- collect output samples
- write normalized float32 WAV output

Because v1 only accepts mono WAV sources, one file equals one channel stream, which keeps the wrapper logic simple.

### 7.3 One-shot vs block processing

Even though the source files are offline assets, implement normalization as **block-based** reading and writing rather than reading every file into a single giant buffer.

This keeps memory usage bounded and makes the wrapper reusable for larger files.

Recommended internal block size:

- start with a fixed block size such as 4096 or 8192 input samples
- keep it implementation-private
- do not expose it as a CLI option in v1

## 8. WAV I/O architecture

### 8.1 Separate WAV I/O from resampling

The wrapper should not own WAV parsing details directly.

Split responsibilities:

- `wav_io.cpp`: inspect WAV metadata, read mono samples, write normalized float32 WAVs
- `resampler_r8brain.cpp`: sample-rate conversion only
- `normalize_audio.cpp`: orchestration logic for scan -> normalize -> report

### 8.2 Required WAV metadata scan

Before any authoring writes begin, inspect every referenced file for:

- readability
- channel count
- sample rate
- frame count
- subtype / format metadata

This scan produces the initial input validation set.

## 9. Duration and frame-count policy

### 9.1 Authoritative duration source

For v1 ADM authoring, the authoritative duration source is the **normalized audio set**, not scene metadata.

Use normalized WAV frame count as the true duration basis for packaging and object block timing.

If `scene.lusid.json` carries duration metadata, validate it against the resolved normalized audio duration and fail if it disagrees materially. This keeps export timing grounded in the actual audio essence.

### 9.2 Strict frame-count policy

After normalization, all input files must have the same final frame count.

Policy:

- no implicit tolerance
- no automatic padding
- no automatic truncation
- compare integer frame counts, not floating-point seconds
- fail authoring if the normalized frame counts differ

### 9.3 Resampling scope and frame agreement

Only files with non-48 kHz sample rates should be resampled.

Already-compliant mono 48 kHz files remain untouched. After normalization, compare the normalized output set as a whole.

### 9.4 Reporting frame-count mismatch

If frame counts mismatch after normalization:

- add a concise human-readable summary to `errors[]`
- include a structured machine-readable validation section with per-file frame counts
- do **not** use `lossLedger[]` for this condition

This is an input validation failure, not a semantic mapping loss.

The current report schema already requires `status`, `errors[]`, `warnings[]`, summary metadata, and `lossLedger[]`, and it requires a report even on failure. fileciteturn11file0 fileciteturn11file6

Recommended extension block:

```json
{
  "authoringValidation": {
    "expectedFrames": 480000,
    "files": [
      { "path": "a.wav", "frames": 480000, "ok": true },
      { "path": "b.wav", "frames": 479998, "ok": false }
    ]
  }
}
```

## 10. CLI and pipeline behavior

### 10.1 Subcommand placement

Resampling belongs only to the new `adm-author` subcommand.

It must not alter the stable existing `transcode` CLI contract, which currently covers only `adm_xml -> lusid_json`. fileciteturn11file9

### 10.2 No silent mode changes

If resampling becomes part of the authoring contract, the following docs must be updated in the same change set if the CLI contract or toolchain behavior changes:

- `spatialroot/AGENTS.md`
- `LUSID/LUSID_AGENTS.md`
- toolchain docs if contract behavior changes

That requirement comes from the current AGENTS documentation-coupling rules. fileciteturn11file4

## 11. Temporary file strategy

### 11.1 Working directory

Each authoring run should create a temporary working directory, for example:

```text
<out-wav>.work/
  normalized_audio/
    object_001.wav
    object_002.wav
```

or a system temp directory with a stable naming prefix.

### 11.2 Cleanup policy

- on success: remove the temporary directory unless a future debug-retain flag is enabled
- on failure: keep temp files only if needed for debug or if a debug-retain mode is added later
- do not leave partially written final outputs in place

### 11.3 Atomicity

The design doc requires stable report generation behavior and atomic output behavior as hard requirements. The current CLI already writes the report atomically and expects best-effort fail reports on error. fileciteturn11file6 fileciteturn11file3

The resampler stage must preserve that discipline:

- temporary normalized files are disposable work artifacts
- final ADM XML and final BW64 outputs must still use temp + rename
- fail reports must still be written best-effort

## 12. Implementation steps

### Step 1: Vendor and pin r8brain

- add `third_party/r8brain-free-src/`
- record pinned upstream revision in a dependency note
- add MIT license notice retention

### Step 2: Add WAV inspection and write helpers

Implement:

- `readWavInfo(path)`
- `readMonoBlock(path, offset, count)`
- `writeFloat32MonoWav(path, sampleRate, samples)` or block-based writer equivalent

### Step 3: Add resampler wrapper

Implement:

- `normalizeMonoWavTo48kFloat32()`
- isolate all r8brain includes to the wrapper implementation file where possible
- do not leak upstream types into authoring code

### Step 4: Add normalization orchestrator

Implement:

- `scanInputs()`
- `normalizeInputsTo48kTemp()`
- `validateNormalizedInputs()`

### Step 5: Extend report schema for authoring details

Add structured authoring-only fields such as:

- files normalized
- original and target sample rates
- original and normalized frame counts
- validation mismatch details

Do not misuse `lossLedger[]` for hard input validation failures.

### Step 6: Wire into `adm-author`

Flow:

- parse CLI
- resolve LUSID package or scene + wav-dir
- scan WAVs
- normalize when needed
- validate normalized frame counts
- continue to ADM mapping and BW64 packaging

### Step 7: Add tests

See Section 13.

## 13. Test plan

### 13.1 Unit tests

- metadata scan of mono 48 kHz WAV
- metadata scan of mono non-48 kHz WAV
- reject multichannel WAV
- normalize 44.1 kHz mono to 48 kHz
- normalize 96 kHz mono to 48 kHz
- preserve already-48 kHz mono without resampling

### 13.2 Validation tests

- fail on unreadable file
- fail on missing file
- fail on post-normalization frame-count mismatch
- fail when scene-declared duration disagrees with audio-derived duration

### 13.3 Integration tests

- package with all 48 kHz mono files and no normalization
- package with mixed 44.1/48/96 kHz mono files requiring partial normalization
- generate report showing which files were normalized

### 13.4 Non-regression rule

No change in this work may regress the existing parity tests for the `adm_xml -> lusid_json` path. The current AGENTS file makes parity failures a hard stop for the Phase 2 baseline. fileciteturn11file3

## 14. Implementation choices deferred

These are explicitly out of scope for the first pass:

- multichannel resampling
- automatic fold-down
- automatic padding or truncation
- user-selectable resampler quality presets
- optional FFT backend switching
- real-time streaming resampling inside Spatial Root
- turning the resampler into a general CULT DSP subsystem

## 15. Recommended first-pass repo edits

### New files

```text
include/audio/resampler.hpp
include/audio/wav_io.hpp
src/audio/resampler_r8brain.cpp
src/audio/wav_io.cpp
src/audio/normalize_audio.cpp
src/audio/validate_audio.cpp
tests/test_resampler.cpp
tests/test_authoring_audio_validation.cpp
third_party/r8brain-free-src/
```

### Updated files

```text
CMakeLists.txt
include/cult_transcoder.hpp
src/main.cpp
src/report.cpp
src/adm_author/...   # once authoring files exist
internalDocsMD/AGENTS-CULT.md
internalDocsMD/...   # any coupled docs required by contract changes
```

## 16. Final recommendation

Implement `r8brain-free-src` as a **private, vendored, header-only resampling backend** behind a thin CULT wrapper. Use it only in the new `adm-author` path to normalize supported mono WAV inputs to **48 kHz float32 temp files**, validate strict equal frame counts after normalization, and preserve the current report/failure discipline.

That gives CULT a practical authoring-side normalization stage without broadening the transcoder into a general audio-processing framework or disturbing the existing parity-critical ingest contract. fileciteturn11file8 fileciteturn11file3
