# ADM Authoring Extension — Implementation Decisions

This document formally captures the design decisions for the `adm-author` pipeline (Steps 3 and 4) which handles LUSID → ADM export mapping and writing to a `.wav` BW64 container. It follows the execution-safe, contract-first design set throughout CULT.

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
   1. Open all Normalized (48 kHz float32) mono WAV files via our self-contained RIFF `wav_io.cpp` reader.
   2. Open the final output `export.adm.wav.tmp` via `bw64::Bw64Writer`.
   3. Read fixed-sized block chunks from all mono files.
   4. Interleave these streams into a contiguous float vector `[ch0_samp0, ch1_samp0, ch2_samp0...]`.
   5. Pass the interleaved vector to `bw64Writer->write()`.
   6. Finally, embed the constructed `pugixml` document into the `axml` chunk of the file before closing.
