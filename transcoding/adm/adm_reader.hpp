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
// transcoding/adm/adm_reader.hpp — ADM WAV Ingestion API (Phase 3)
//
// Provides extractAxmlFromWav(): opens a BW64/RF64/WAV file, reads the
// embedded <axml> chunk, writes a debug copy to the fixed artifact path
// (processedData/currentMetaData.xml), and returns the raw XML string for
// in-memory parsing by adm_to_lusid.
//
// Design decisions (AGENTS-CULT §8, DEV-PLAN Phase 3):
//   - Path of debug artifact is HARDCODED to "processedData/currentMetaData.xml"
//     relative to the working directory (i.e. the spatialroot repo root).
//   - Debug XML is ALWAYS written (enabled by default per D2).
//   - Parsing happens directly from the returned buffer — no re-read.
//   - libbw64 is a git submodule at cult_transcoder/thirdparty/libbw64.
//     It is header-only (Apache-2.0). Never use spatialroot's copy.
//
// Error handling:
//   - Returns empty string + populates errors on failure.
//   - Debug artifact write failure is a warning, not a hard error —
//     the XML string is still returned for in-memory parsing.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

namespace cult {

// ---------------------------------------------------------------------------
// AxmlResult — returned by extractAxmlFromWav()
// ---------------------------------------------------------------------------
struct AxmlResult {
    bool        success = false;
    std::string xmlData;              // raw ADM XML string (empty on failure)
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ---------------------------------------------------------------------------
// extractAxmlFromWav()
//
// Opens wavPath as a BW64/RF64/WAV file, extracts the embedded axml chunk,
// writes the debug artifact to debugXmlPath, and returns the XML in
// result.xmlData for immediate in-memory parsing.
//
// debugXmlPath defaults to "processedData/currentMetaData.xml" (D2).
// Callers should not override this unless testing.
// ---------------------------------------------------------------------------
AxmlResult extractAxmlFromWav(
    const std::string& wavPath,
    const std::string& debugXmlPath = "processedData/currentMetaData.xml"
);

} // namespace cult
