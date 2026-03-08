// Copyright 2026 Cult-DSP
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

// ---------------------------------------------------------------------------
// transcoding/adm/sony360ra_to_lusid.hpp — Sony 360RA ADM → LUSID converter
//
// Phase 6A: converts a Sony 360RA ADM XML document (bwfmetaedit export,
// ITU-R BS.2076-2, conformance_point_document wrapper) to a LUSID Scene JSON.
//
// Design decisions (AGENTS-CULT §15, all owner-confirmed 2026-03-07):
//
//   Frame strategy: Option A — single static frame at t=0. The first
//     audioBlockFormat with gain > 0 is used per object. Sony's gain-mute
//     pattern (periodic 0.01024s silence blocks at section boundaries) is
//     discarded — it is not keyframe animation.
//
//   Gain field: NOT written to LUSID nodes. Renderer assumes gain=1.0.
//     ⚠ Future bug risk: if a 360RA source has meaningful per-object gain
//     variation, the renderer will play all objects at equal level.
//     See AGENTS-CULT §15.5.
//
//   Container audioObject: skipped. Any audioObject with <audioObjectIDRef>
//     children is a container and is not converted. Only leaf objects
//     (those with <audioTrackUIDRef>) produce LUSID nodes.
//     See AGENTS-CULT §15.6 for future container metadata consideration.
//
//   Node IDs: "1.1" through "13.1" in leaf audioObject encounter order.
//
//   Polar → Cartesian:
//     az_rad = azimuth  * π/180   (ADM: CW from front, +90 = right)
//     el_rad = elevation * π/180   (ADM: 0 = horizontal, +90 = up)
//     x = distance * sin(az_rad) * cos(el_rad)   // right  = +x
//     y = distance * cos(az_rad) * cos(el_rad)   // front  = +y
//     z = distance * sin(el_rad)                  // up     = +z
//
//   Time format: HH:MM:SS.fffffS<sampleRate> — sscanf stops at 'S',
//     so no custom time parser is needed.
//
//   Dispatch: called by transcoder.cpp when resolveAdmProfile() returns
//     Sony360RA. No new CLI flags.
//
//   --lfe-mode: irrelevant for 360RA (no DirectSpeakers/LFE). If supplied,
//     a warning is added to the result; the flag is otherwise ignored.
//
// XML root path navigated by this converter:
//   conformance_point_document/File/aXML/format/audioFormatExtended
//   (the conformance_point_document wrapper is a bwfmetaedit artifact)
//
// References:
//   AGENTS-CULT §15 (full design spec)
//   DESIGN-DOC-V1-CULT.MD "Sony 360 Reality Audio: Two Ingestion Modes"
//   ITU-R BS.2076-2 (Audio Definition Model)
// ---------------------------------------------------------------------------

#include "adm_to_lusid.hpp"   // ConversionResult, LusidScene, LfeMode

// Forward-declare pugixml to keep this header lightweight.
namespace pugi { class xml_document; }

namespace cult {

// ---------------------------------------------------------------------------
// convertSony360RaToLusid()
//
// Converts a Sony 360RA ADM XML document to a LUSID scene.
//
// Parameters:
//   doc     — a pugi::xml_document already loaded and parsed (either from
//             a .xml file or from a BW64 axml chunk buffer). The document
//             must contain the conformance_point_document wrapper structure.
//             If the wrapper is absent, the converter falls back to searching
//             for audioFormatExtended directly under any root element.
//
//   lfeMode — passed through from TranscodeRequest for report transparency.
//             Has no effect on conversion (360RA has no LFE/DirectSpeakers).
//             If lfeMode != Hardcoded, a warning is added to the result:
//             "lfe-mode flag has no effect on Sony360RA input".
//
// Returns:
//   ConversionResult with:
//     success = true  — scene populated, numFrames = 1 (static single frame)
//     success = false — errors[] populated (e.g. no audioFormatExtended found,
//                       no leaf audioObjects found)
//     warnings[]      — profile detection confirmation, lfe-mode note if needed
//
// Thread safety: not thread-safe (pugixml nodes are not thread-safe).
// ---------------------------------------------------------------------------
ConversionResult convertSony360RaToLusid(
    const pugi::xml_document& doc,
    LfeMode lfeMode = LfeMode::Hardcoded
);

} // namespace cult
