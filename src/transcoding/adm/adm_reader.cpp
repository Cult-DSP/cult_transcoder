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

// ---------------------------------------------------------------------------
// src/transcoding/adm/adm_reader.cpp — ADM WAV Ingestion (Phase 3)
//
// Implements extractAxmlFromWav() declared in adm_reader.hpp.
//
// Pipeline (AGENTS-CULT §8, DEV-PLAN Phase 3):
//   1. Open WAV as a BW64/RF64 container via libbw64 (git submodule,
//      thirdparty/libbw64, Apache-2.0, header-only).
//   2. Extract the raw axml chunk bytes.
//   3. Write bytes verbatim to the debug artifact path
//      (default: processedData/currentMetaData.xml, hardcoded per D2).
//      A warning is emitted if the write fails but processing continues.
//   4. Return the XML string to the caller (transcoder.cpp) for immediate
//      in-memory parsing by convertAdmToLusid() — no disk re-read.
//
// This replaces spatialroot's spatialroot_adm_extract binary + extractMetaData()
// Python wrapper entirely. Those are now deprecated (AGENTS.md updated).
//
// References:
//   - AGENTS-CULT §8 (Phase 3 Ingestion)
//   - DEV-PLAN-CULT Phase 3
// ---------------------------------------------------------------------------

#include "transcoding/adm/adm_reader.hpp"

#include <bw64/bw64.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace cult {

AxmlResult extractAxmlFromWav(
    const std::string& wavPath,
    const std::string& debugXmlPath
) {
    AxmlResult result;

    // --- Validate input path ---
    if (wavPath.empty()) {
        result.errors.push_back("extractAxmlFromWav: wavPath is empty");
        return result;
    }
    if (!std::filesystem::exists(wavPath)) {
        result.errors.push_back("Input WAV file not found: '" + wavPath + "'");
        return result;
    }

    // --- Open BW64/RF64/WAV container ---
    std::shared_ptr<bw64::Bw64Reader> reader;
    try {
        reader = bw64::readFile(wavPath);
    } catch (const std::exception& e) {
        result.errors.push_back(
            "Failed to open BW64 file '" + wavPath + "': " + e.what());
        return result;
    }

    // --- Extract axml chunk ---
    auto axmlChunk = reader->axmlChunk();
    if (!axmlChunk) {
        result.errors.push_back(
            "No axml chunk found in '" + wavPath + "'. "
            "Is this a valid ADM BW64 file?");
        return result;
    }

    result.xmlData = axmlChunk->data();

    if (result.xmlData.empty()) {
        result.errors.push_back(
            "axml chunk is present but empty in '" + wavPath + "'");
        return result;
    }

    // --- Write debug XML artifact (D2 — always written by default) ---
    // Path is hardcoded to processedData/currentMetaData.xml relative to CWD
    // (the spatialroot repo root). This preserves the artifact that downstream
    // tools and humans rely on for debugging. Failure to write is a warning,
    // not a hard error — in-memory parsing proceeds regardless.
    if (!debugXmlPath.empty()) {
        try {
            std::filesystem::path xmlOut(debugXmlPath);
            // Create parent directory if it does not exist
            if (xmlOut.has_parent_path()) {
                std::filesystem::create_directories(xmlOut.parent_path());
            }
            std::ofstream outFile(xmlOut, std::ios::binary);
            if (!outFile) {
                result.warnings.push_back(
                    "Could not open debug XML artifact for writing: '" +
                    debugXmlPath + "' — in-memory parsing will still proceed.");
            } else {
                outFile.write(result.xmlData.data(),
                              static_cast<std::streamsize>(result.xmlData.size()));
                if (!outFile) {
                    result.warnings.push_back(
                        "Write failed for debug XML artifact: '" +
                        debugXmlPath + "' — in-memory parsing will still proceed.");
                }
            }
        } catch (const std::exception& e) {
            result.warnings.push_back(
                std::string("Exception writing debug XML artifact: ") + e.what() +
                " — in-memory parsing will still proceed.");
        }
    }

    result.success = true;
    return result;
}

} // namespace cult
