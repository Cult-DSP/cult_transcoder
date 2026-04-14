# mpeg-h-plan.md — Phase 6B (MPEG-H) Plan (Ittiam-first)

Status snapshot:

- Phases 1–6A complete (including Sony 360RA ADM conversion). MPEG-H (Phase 6B) is explicitly deferred. :contentReference[oaicite:0]{index=0}
- Phase 6B requirements: Ittiam `libmpeghdec` as default backend; Fraunhofer optional; new `transcoding/mpegh/` module; owner-confirmed `.mpf` or `.mhas` fixtures before implementation. :contentReference[oaicite:1]{index=1}
- Mode 2 (Sony 360RA “music format” MPEG-H) is a different ingestion modality: no ADM XML; metadata is MAEI; containers are `.mpf/.mhas/.m4f`; must not extend ADM resolver or `sony360ra_to_lusid.cpp`. :contentReference[oaicite:2]{index=2} :contentReference[oaicite:3]{index=3}

---

## 0) Goal

Add MPEG-H ingestion and transcoding into canonical LUSID, using the Ittiam MPEG-H decoder library as the default backend, without disturbing the already-complete ADM pipeline and Sony 360RA ADM conversion.

Primary outputs:

- `--in-format mpegh_*` → `--out-format lusid_json`
- report JSON (v0.1) emitted as usual (lossLedger included)

Non-goals (for first MPEG-H milestone):

- perfect feature coverage for all MPEG-H interactive/personalization metadata
- full authoring/encoding of MPEG-H bitstreams (decode/transcode first)

---

## 1) Architectural invariants (do not break)

1. Do not attempt to “route MPEG-H through ADM.”
   - Mode 2 has no ADM XML and metadata is MAEI, so the conversion path must live in a new `transcoding/mpegh/` module. :contentReference[oaicite:4]{index=4} :contentReference[oaicite:5]{index=5}
2. Do not extend the ADM profile resolver or `sony360ra_to_lusid.cpp` to handle MPEG-H.
   - Explicitly forbidden by design doc. :contentReference[oaicite:6]{index=6}
3. Preserve existing CLI contract defaults (atomic output, fail-report behavior, default report path).
   - Still required by the pinned contracts. :contentReference[oaicite:7]{index=7} :contentReference[oaicite:8]{index=8}
4. Maintain build and binary path invariants for Spatial Root pipeline.
   - Binary path and wrapper rules must stay consistent. :contentReference[oaicite:9]{index=9}

---

## 2) Inputs we must support (containers and detection)

From the design doc Mode 2 notes:

- containers: `.mpf` / `.mhas` / `.m4f` (ISO BMFF / MHAS stream forms) :contentReference[oaicite:10]{index=10}
- metadata: MAEI binary format (MPEG-H Audio Scene Information) :contentReference[oaicite:11]{index=11}

Plan:

- Introduce new `--in-format` values:
  - `mpegh_mhas` (standalone stream)
  - `mpegh_mpf` (ISO BMFF container if Ittiam supports it directly)
  - optionally: `mpegh_auto` (detect by magic bytes / extension) in a later step

Detection:

- Must be container-level, not ADM name-string matching. :contentReference[oaicite:12]{index=12}
- Implement simple detection in `src/transcoder.cpp`:
  - if `--in-format` explicitly set, trust it
  - if future auto-detect exists, detect by file extension + header signature

---

## 3) Dependencies and build strategy (Ittiam-first)

Pinned Phase 6B dependency plan:

- Default backend: Ittiam `libmpeghdec` :contentReference[oaicite:13]{index=13}
- Optional backend: Fraunhofer `mpeghdec` :contentReference[oaicite:14]{index=14}
- Build flags:
  - `WITH_MPEGH_ITTIAM=ON` (default)
  - `WITH_MPEGH_FRAUNHOFER=OFF` (default) :contentReference[oaicite:15]{index=15}

Concrete build integration plan:

- Add new third-party folder:
  - `thirdparty/ittiam-libmpeghdec/` (as submodule or pinned vendor drop, consistent with project policy)
- Add CMake options:
  - `option(WITH_MPEGH_ITTIAM "Enable Ittiam MPEG-H decoder backend" ON)`
  - `option(WITH_MPEGH_FRAUNHOFER "Enable Fraunhofer MPEG-H decoder backend" OFF)`
- Compile-time gating:
  - if neither backend enabled and a user requests `--in-format mpegh_*`, return non-zero with fail-report (status: fail)

---

## 4) New module layout (must be separate from ADM)

Create:

transcoding/mpegh/
mpegh_reader.hpp
mpegh_reader.cpp
mpegh_to_lusid.hpp
mpegh_to_lusid.cpp
mpegh_detect.hpp # (optional) container sniffing helpers

Reason:

- Mode 2 conversion path is architecturally different (MAEI→LUSID mapping). :contentReference[oaicite:16]{index=16}

---

## 5) Data pipeline for MPEG-H → LUSID (decode-first)

### 5.1 Stage A: Parse container and decode audio (PCM) + extract MAEI metadata

Using Ittiam decoder:

- Read input stream/container
- Decode:
  - PCM channels/objects/HOA streams as accessible
  - MAEI metadata required to reconstruct object positions/gains/presets (minimum subset)

Output from Stage A:

- `MpeghDecodeResult`:
  - `sampleRate`
  - `durationSec` (if derivable)
  - `objects[]`: per-object audio buffers (or references) + metadata handles
  - `beds[]`: channel-based elements (if present)
  - raw metadata blob(s) if needed for debugging

### 5.2 Stage B: Build canonical LUSID scene

Mapping target (minimum viable):

- Represent each MPEG-H audio object as a LUSID `audio_object` node:
  - stable node id assignment (deterministic per object index)
  - `cart` positions derived from MAEI positional metadata
  - `frames[]`:
    - Phase 1 of MPEG-H: static single frame at t=0 if metadata is static
    - if time-varying positions exist: create frames at change times

Beds / channels:

- If MPEG-H contains channel-based bed(s), map each channel to `direct_speaker` nodes:
  - stable ordering and ids
  - note: channel layout may be signaled flexibly in MPEG-H; if we cannot resolve speaker geometry, report loss and fallback to generic mapping

HOA:

- If MPEG-H carries HOA elements:
  - for now: either
    - (A) map as `audio_object` set with note of approximation, or
    - (B) emit report loss entry and skip HOA until supported
  - this decision must be explicit in lossLedger

### 5.3 Stage C: Write LUSID JSON + report (unchanged rules)

- atomic write `<out>.tmp` then rename
- report default `<out>.report.json` (unchanged)

---

## 6) Output audio (stems) policy for first milestone

MPEG-H decode outputs PCM. The current Spatial Root pipeline expects mono WAV stems + scene mapping, but the immediate requirement for Phase 6B is scene transcoding.

Plan:

- First MPEG-H milestone: generate `scene.lusid.json` only (no stem packaging), and document that the realtime engine pipeline will still need an audio packaging step (future phase).
- In report, include warnings that “audio payload extraction/stem packaging is not yet implemented.”

Rationale:

- DEV plan does not currently specify packaging for MPEG-H; only indicates library infrastructure and the new module. :contentReference[oaicite:17]{index=17}

---

## 7) Test plan (mandatory before “enabled”)

Pinned prerequisite:

- Owner-confirmed `.mpf` or `.mhas` fixtures before implementation. :contentReference[oaicite:18]{index=18}
  Design doc also treats this as a hard blocker for Mode 2. :contentReference[oaicite:19]{index=19}

### 7.1 Fixtures

Add:

- `tests/mpegh/fixtures/`
  - `sony_360ra_mode2_min.mhas` (or `.mpf`) — smallest known-good file
  - `mpegh_bed_only.mhas` (if available)
  - `mpegh_objects_only.mhas` (if available)

### 7.2 Structural invariants tests

- LUSID output has valid top-level fields, monotonic frames, timeUnit seconds
- node count matches expected objects/beds for each fixture
- deterministic stable ordering across runs

### 7.3 Metadata mapping tests

- For at least one fixture: verify known object positions map to expected `cart` (within epsilon)
- If metadata indicates time-varying positions: verify frame count and times

### 7.4 Failure-mode tests

- if `WITH_MPEGH_ITTIAM=OFF` and user requests `--in-format mpegh_*` → fail-report + non-zero exit
- malformed file: fail-report + no output

---

## 8) CLI and transcoder dispatch changes

Update `src/transcoder.cpp` dispatch table:

- Existing:
  - `adm_xml`, `adm_wav` paths (already stable)
- Add:
  - `mpegh_mhas`, `mpegh_mpf` (or `mpegh_auto` later)

Dispatch rule:

- MPEG-H formats route directly into `transcoding/mpegh/*` pipeline and must not call ADM or Sony 360RA converters. :contentReference[oaicite:20]{index=20}

---

## 9) Reporting and loss ledger rules for MPEG-H

Report must include:

- args: include `inFormat=mpegh_*`, backend selection (`ittiam` vs `fraunhofer`), and any detected container info
- lossLedger:
  - record every unimplemented/approximated mapping
  - examples:
    - “HOA skipped”
    - “bed layout unresolved; mapped to generic direct_speaker ids”
    - “interactive presets not represented in LUSID”

---

## 10) Documentation updates required (same PR as implementation)

When Phase 6B starts and/or lands:

- Update `internalDocsMD/AGENTS-CULT.md` status line to reflect Phase 6B work beginning/completion. :contentReference[oaicite:21]{index=21}
- Update `internalDocsMD/DEV-PLAN-CULT.md` to move Phase 6B from deferred to active, and list work items. :contentReference[oaicite:22]{index=22}
- Update `internalDocsMD/DESIGN-DOC-V1-CULT.MD` with the MAEI→LUSID mapping subsection (required by Mode 2 gating). :contentReference[oaicite:23]{index=23}
- Do not change existing ADM/Sony360RA modules to “help” Mode 2. :contentReference[oaicite:24]{index=24}

---

## 11) Work breakdown (recommended sequence)

### 11.1 Phase 6B-0: Fixtures and backend selection

- Confirm at least one `.mhas` or `.mpf` fixture exists and is committed to tests
- Enable `WITH_MPEGH_ITTIAM=ON` by default (already pinned) :contentReference[oaicite:25]{index=25}

### 11.2 Phase 6B-1: Build integration

- Add thirdparty Ittiam decoder and wire CMake option
- Add compilation guards and a clear fail-report when disabled

### 11.3 Phase 6B-2: Minimal reader + decode

- Implement `mpegh_reader` to open stream/container and call Ittiam decode API
- Extract basic stream properties: sampleRate, channel/object counts, duration if available

### 11.4 Phase 6B-3: Minimal MAEI→LUSID scene

- Static single frame at t=0
- One node per object, with cart positions
- Emit lossLedger entries for any metadata not represented

### 11.5 Phase 6B-4: Tests

- Structural invariants on fixture
- Known-position mapping test (at least one object)
- Failure tests

### 11.6 Phase 6B-5: Integration + docs

- Add `--in-format mpegh_*` wiring to CLI dispatch
- Update AGENTS/DEV-PLAN/DESIGN docs as required

---

## 12) “Where we are at” summary (for readers)

- CULT transcoder is stable and already handles:
  - ADM XML → LUSID (Phase 2)
  - ADM WAV (BW64 axml extraction) → LUSID (Phase 3)
  - profile detection + LFE mode flag (Phase 4)
  - GUI transcode tab (Phase 5)
  - Sony 360RA ADM-variant conversion (Phase 6A) :contentReference[oaicite:26]{index=26}
- MPEG-H support (Phase 6B) is the next major module:
  - requires MPEG-H decoder integration (Ittiam default)
  - requires fixture files
  - requires MAEI→LUSID mapping doc section
  - must live in new `transcoding/mpegh/` module, separate from ADM paths :contentReference[oaicite:27]{index=27} :contentReference[oaicite:28]{index=28}
