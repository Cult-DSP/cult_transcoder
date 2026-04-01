## ADM Authoring Plan (cult_transcoder)

Last verified against the submodule code: 2026-03-31.
`internalDocsMD/DEV-PLAN-CULT.md` is outdated and must not be used as a source of truth.

### 0. Purpose

Implement LUSID -> ADM authoring in `cult_transcoder` (C++), producing:

- `export.adm.xml` (ADM XML)
- `export.adm.wav` (BW64 with embedded ADM metadata)

The Spatial Root runtime is C++ only. Python is not used in production. The
Spatial Root C++ app calls `cult-transcoder` and the spatial engine directly.

### 1. Scope and Constraints

- Target DAW: Logic Pro Atmos (must-pass).
- Libraries: use `libadm` for ADM XML authoring and `libbw64` for BW64 packaging.
- Input: either a LUSID package folder OR `scene.lusid.json` + WAVs directory.
- Output: ADM XML + ADM BW64 (both written by `cult-transcoder`).
- Preserve existing ordering and ID behavior from LUSID.
- No Python dependencies in runtime path.

### 2. Input/Output Contract

**Accepted inputs** (both must be supported):

1. LUSID package folder:
   - contains `scene.lusid.json`
   - contains mono WAVs at package root

2. Explicit input files:
   - `scene.lusid.json`
   - WAVs directory path

**Outputs**:

- `export.adm.xml`
- `export.adm.wav` (BW64)
- report JSON (existing report schema, extended as needed)

### 3. Proposed CLI Contract (New Subcommand)

Add a new CLI subcommand for ADM authoring:

```
cult-transcoder adm-author
  --lusid <scene.lusid.json>
  --wav-dir <path>
  --out-xml <export.adm.xml>
  --out-wav <export.adm.wav>
  [--report <path>] [--stdout-report]

# Alternate input:
cult-transcoder adm-author
  --lusid-package <path>
  --out-xml <export.adm.xml>
  --out-wav <export.adm.wav>
  [--report <path>] [--stdout-report]
```

**Exit codes**:

- `0`: success, outputs exist
- `1`: authoring failure, no output
- `2`: arg/usage error

**Atomicity**:

- `export.adm.xml` and `export.adm.wav` must be written via temp + rename.
- Report already uses temp + rename in `main.cpp`.

### 4. LUSID -> ADM Mapping Policy

**Sources**:

- `direct_speaker` nodes map to ADM beds/channels.
- `audio_object` nodes map to ADM objects.
- `LFE` node must map to the LFE channel in the ADM bed.

**Ordering**:

- Preserve LUSID frame order and node order.
- Beds first, objects after.

**Timebase**:

- LUSID uses `timeUnit: "seconds"`. Convert to ADM timecode/frames using
  the sample rate from LUSID or default 48000.

**Channel mapping**:

- Use existing LUSID IDs to map to ADM channels. Maintain deterministic
  ordering: beds then objects.

### 5. Implementation Steps

1. Add new `adm-author` subcommand and argument parsing in `src/main.cpp`.
2. Extend `cult_transcoder.hpp` to support an ADM authoring request struct
   and result struct (do not change existing transcode API).
3. Create new implementation unit(s):
   - `src/lusid_to_adm.cpp` (LUSID JSON -> ADM model)
   - `src/adm_bw64.cpp` (ADM model -> BW64 + embedded XML)
4. Link in `libadm` and `libbw64` via CMake FetchContent.
5. Implement LUSID scene parser for C++ (if not already present):
   - Minimal JSON reader to parse `scene.lusid.json` fields needed for ADM.
6. Implement ADM XML writer using `libadm`.
7. Implement BW64 packaging using `libbw64`.
8. Extend report schema if needed (new fields for ADM export stats).
9. Add tests:
   - Unit tests for CLI arg validation
   - Mapping tests for LUSID -> ADM object and bed counts
   - Golden tests comparing ADM XML structure for a known LUSID scene
10. Add a small integration test that produces `export.adm.wav` from a sample
    LUSID package and validates headers + embedded XML.

### 6. Validation Plan (Logic Pro Atmos)

1. Import `export.adm.wav` into Logic Pro Atmos.
2. Verify bed channels + object tracks appear and are routed correctly.
3. Verify LFE is recognized and silent beds are accepted.
4. Confirm timing aligns with LUSID frame times.

### 7. Risks and Open Items

- LUSID -> ADM mapping semantics may need iteration to satisfy Logic Pro.
- libadm traversal order may differ from LUSID ordering; enforce explicit
  ordering in the writer.
- JSON parsing for LUSID requires careful handling of frames and node order.

### 8. Deliverables

- `cult-transcoder adm-author` command implemented and documented.
- ADM XML writer and BW64 packager integrated.
- Tests covering mapping correctness and output validity.
- Updated docs (this file + AGENTS-CULT.md) to reflect new CLI contract.
