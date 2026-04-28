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

#include "packaging/adm_package.hpp"

#include "adm_reader.hpp"
#include "adm_profile_resolver.hpp"
#include "adm_to_lusid.hpp"
#include "sony360ra_to_lusid.hpp"

#include <bw64/bw64.hpp>
#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace cult {
// File-local helpers live in an anonymous namespace to avoid exporting
// implementation details or colliding with similar helpers in other modules.
namespace {

struct WavSourceInfo {
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;
    uint64_t frameCount = 0;
};

uint16_t readU16(std::ifstream& f) {
    std::array<unsigned char, 2> b{};
    f.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint16_t>(b[0] | (b[1] << 8));
}

uint32_t readU32(std::ifstream& f) {
    std::array<unsigned char, 4> b{};
    f.read(reinterpret_cast<char*>(b.data()), b.size());
    return static_cast<uint32_t>(b[0]) |
           (static_cast<uint32_t>(b[1]) << 8) |
           (static_cast<uint32_t>(b[2]) << 16) |
           (static_cast<uint32_t>(b[3]) << 24);
}

bool readTag(std::ifstream& f, char out[4]) {
    f.read(out, 4);
    return f.gcount() == 4;
}

bool readWavSourceInfo(const std::string& path, WavSourceInfo& info, std::string& error) {
    info = WavSourceInfo{};
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        error = "Failed to open WAV: '" + path + "'";
        return false;
    }

    char riff[4];
    if (!readTag(f, riff)) {
        error = "Invalid WAV: missing RIFF/BW64 header";
        return false;
    }
    const std::string riffId(riff, 4);
    if (riffId != "RIFF" && riffId != "BW64" && riffId != "RF64") {
        error = "Invalid WAV: missing RIFF/BW64/RF64 header";
        return false;
    }
    (void)readU32(f);
    char wave[4];
    if (!readTag(f, wave) || std::string(wave, 4) != "WAVE") {
        error = "Invalid WAV: missing WAVE header";
        return false;
    }

    bool foundFmt = false;
    bool foundData = false;
    while (f && !(foundFmt && foundData)) {
        char chunkId[4];
        if (!readTag(f, chunkId)) break;
        const uint32_t chunkSize = readU32(f);
        const std::string id(chunkId, 4);
        const std::streamoff payloadOffset = f.tellg();

        if (id == "fmt ") {
            info.audioFormat = readU16(f);
            info.channels = readU16(f);
            info.sampleRate = readU32(f);
            (void)readU32(f);
            info.blockAlign = readU16(f);
            info.bitsPerSample = readU16(f);
            if (chunkSize > 16) {
                f.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
            }
            foundFmt = true;
        } else if (id == "data") {
            info.dataOffset = static_cast<uint64_t>(payloadOffset);
            info.dataSize = chunkSize;
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
            foundData = true;
        } else {
            f.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }

        if (chunkSize % 2 == 1) {
            f.seekg(1, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData) {
        error = "Invalid WAV: missing fmt or data chunk";
        return false;
    }
    if (info.channels == 0 || info.blockAlign == 0) {
        error = "Invalid WAV: bad format data";
        return false;
    }
    if (info.audioFormat != 1 && info.audioFormat != 3) {
        error = "Unsupported WAV format for package splitting";
        return false;
    }
    if (info.bitsPerSample != 16 && info.bitsPerSample != 24 && info.bitsPerSample != 32) {
        error = "Unsupported WAV bit depth for package splitting";
        return false;
    }
    info.frameCount = info.dataSize / info.blockAlign;
    return true;
}

std::vector<std::string> collectUniqueNodeIds(const LusidScene& scene) {
    std::vector<std::string> ids;
    std::set<std::string> seen;
    for (const auto& frame : scene.frames) {
        for (const auto& node : frame.nodes) {
            if (seen.insert(node.id).second) {
                ids.push_back(node.id);
            }
        }
    }
    return ids;
}

int bedIndex(const std::string& id) {
    if (id.size() > 2 && id.substr(id.size() - 2) == ".1") {
        try {
            const int parsed = std::stoi(id.substr(0, id.size() - 2));
            if (parsed > 0 && parsed <= 10) return parsed;
        } catch (...) {
        }
    }
    return 999999;
}

std::vector<std::string> canonicalNodeOrder(const LusidScene& scene) {
    auto ids = collectUniqueNodeIds(scene);
    std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
        const int bedA = bedIndex(a);
        const int bedB = bedIndex(b);
        if (bedA != bedB) return bedA < bedB;
        return a < b;
    });
    return ids;
}

std::string stemNameForNode(const std::string& id) {
    return id == "4.1" ? "LFE.wav" : id + ".wav";
}

void writeFloat32MonoHeader(std::ofstream& f, uint32_t sampleRate, uint64_t frames) {
    const uint32_t dataBytes = static_cast<uint32_t>(frames * sizeof(float));
    const uint32_t riffSize = 36u + dataBytes;
    const uint16_t audioFormat = 3;
    const uint16_t channels = 1;
    const uint16_t bits = 32;
    const uint16_t blockAlign = channels * (bits / 8);
    const uint32_t byteRate = sampleRate * blockAlign;

    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&riffSize), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    const uint32_t fmtSize = 16;
    f.write(reinterpret_cast<const char*>(&fmtSize), 4);
    f.write(reinterpret_cast<const char*>(&audioFormat), 2);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    f.write(reinterpret_cast<const char*>(&sampleRate), 4);
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    f.write(reinterpret_cast<const char*>(&bits), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataBytes), 4);
}

float decodeSample(const char* p, uint16_t audioFormat, uint16_t bitsPerSample) {
    if (audioFormat == 3 && bitsPerSample == 32) {
        float value = 0.0f;
        std::memcpy(&value, p, sizeof(float));
        return value;
    }
    if (audioFormat == 1 && bitsPerSample == 16) {
        int16_t value = 0;
        std::memcpy(&value, p, sizeof(int16_t));
        return static_cast<float>(value) / 32768.0f;
    }
    if (audioFormat == 1 && bitsPerSample == 24) {
        const auto b0 = static_cast<unsigned char>(p[0]);
        const auto b1 = static_cast<unsigned char>(p[1]);
        const auto b2 = static_cast<unsigned char>(p[2]);
        int32_t value = (static_cast<int32_t>(b2) << 24) |
                        (static_cast<int32_t>(b1) << 16) |
                        (static_cast<int32_t>(b0) << 8);
        value >>= 8;
        return static_cast<float>(value) / 8388608.0f;
    }
    if (audioFormat == 1 && bitsPerSample == 32) {
        int32_t value = 0;
        std::memcpy(&value, p, sizeof(int32_t));
        return static_cast<float>(static_cast<double>(value) / 2147483648.0);
    }
    return 0.0f;
}

bool splitInterleavedToMono(
        const std::string& inWavPath,
        const WavSourceInfo& info,
        const std::vector<std::string>& orderedIds,
        const fs::path& packageDir,
        const ProgressCallback& onProgress,
        std::string& error
) {
    if (orderedIds.size() != info.channels) {
        error = "ADM package split channel count does not match LUSID node count";
        return false;
    }
    if (info.frameCount > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max() - 36u) / sizeof(float)) {
        error = "Package stem is too large for RIFF float32 WAV output";
        return false;
    }

    std::ifstream in(inWavPath, std::ios::binary);
    if (!in.is_open()) {
        error = "Failed to open source WAV for splitting";
        return false;
    }
    in.seekg(static_cast<std::streamoff>(info.dataOffset), std::ios::beg);

    std::vector<std::ofstream> outs(orderedIds.size());
    for (size_t i = 0; i < orderedIds.size(); ++i) {
        const fs::path outPath = packageDir / stemNameForNode(orderedIds[i]);
        outs[i].open(outPath, std::ios::binary);
        if (!outs[i].is_open()) {
            error = "Failed to create package stem: " + outPath.string();
            return false;
        }
        writeFloat32MonoHeader(outs[i], info.sampleRate, info.frameCount);
    }

    const uint64_t framesPerChunk = 4096;
    std::vector<char> input(static_cast<size_t>(framesPerChunk * info.blockAlign));
    const size_t bytesPerSample = info.bitsPerSample / 8;
    uint64_t processed = 0;
    uint64_t lastReported = 0;

    while (processed < info.frameCount) {
        const uint64_t framesNow = std::min(framesPerChunk, info.frameCount - processed);
        const size_t bytesNow = static_cast<size_t>(framesNow * info.blockAlign);
        in.read(input.data(), static_cast<std::streamsize>(bytesNow));
        if (static_cast<size_t>(in.gcount()) != bytesNow) {
            error = "Short read while splitting source WAV";
            return false;
        }

        for (uint64_t frame = 0; frame < framesNow; ++frame) {
            const size_t frameOffset = static_cast<size_t>(frame * info.blockAlign);
            for (size_t ch = 0; ch < orderedIds.size(); ++ch) {
                const char* samplePtr = input.data() + frameOffset + ch * bytesPerSample;
                const float value = decodeSample(samplePtr, info.audioFormat, info.bitsPerSample);
                outs[ch].write(reinterpret_cast<const char*>(&value), sizeof(float));
            }
        }

        processed += framesNow;
        if (onProgress && (processed == info.frameCount || processed - lastReported >= info.sampleRate / 2)) {
            lastReported = processed;
            onProgress(ProgressEvent{"split", processed, info.frameCount, "writing mono stems"});
        }
    }

    for (auto& out : outs) {
        out.close();
        if (!out) {
            error = "Failed while writing package stems";
            return false;
        }
    }
    return true;
}

bool writeTextAtomic(const fs::path& path, const std::string& text) {
    const fs::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out.is_open()) return false;
        out << text;
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp);
        return false;
    }
    return true;
}

} // namespace

PackageAdmWavResult packageAdmWav(const PackageAdmWavRequest& req) {
    PackageAdmWavResult result;
    Report& report = result.report;
    report.args.inPath = req.inWavPath;
    report.args.inFormat = "adm_wav";
    report.args.outPath = req.outPackageDir;
    report.args.outFormat = "lusid_package";
    report.args.reportPath = req.reportPath;
    report.args.stdoutReport = req.stdoutReport;
    report.args.lfeMode = req.lfeMode == LfeMode::SpeakerLabel ? "speaker-label" : "hardcoded";

    if (req.inWavPath.empty()) {
        report.errors.push_back("package-adm-wav: --in is required");
        return result;
    }
    if (req.outPackageDir.empty()) {
        report.errors.push_back("package-adm-wav: --out-package is required");
        return result;
    }
    if (!fs::exists(req.inWavPath)) {
        report.errors.push_back("package-adm-wav: input file not found: " + req.inWavPath);
        return result;
    }

    if (req.onProgress) {
        req.onProgress(ProgressEvent{"metadata", 0, 1, "extracting ADM XML"});
    }

    auto axmlResult = extractAxmlFromWav(req.inWavPath);
    for (const auto& warning : axmlResult.warnings) {
        report.warnings.push_back(warning);
    }
    if (!axmlResult.success) {
        for (const auto& error : axmlResult.errors) {
            report.errors.push_back(error);
        }
        return result;
    }

    pugi::xml_document doc;
    auto parseResult = doc.load_string(axmlResult.xmlData.c_str());
    if (!parseResult) {
        report.errors.push_back("package-adm-wav: failed to parse embedded axml");
        return result;
    }

    ProfileResult profile = resolveAdmProfile(doc);
    for (const auto& warning : profile.warnings) {
        report.warnings.push_back(warning);
    }

    ConversionResult conversion;
    if (profile.profile == AdmProfile::Sony360RA) {
        conversion = convertSony360RaToLusid(doc, req.lfeMode);
    } else {
        conversion = convertAdmToLusidFromBuffer(axmlResult.xmlData, req.lfeMode);
    }
    for (const auto& warning : conversion.warnings) {
        report.warnings.push_back(warning);
    }
    if (!conversion.success) {
        for (const auto& error : conversion.errors) {
            report.errors.push_back(error);
        }
        return result;
    }

    if (req.onProgress) {
        req.onProgress(ProgressEvent{"metadata", 1, 1, "converted ADM to LUSID"});
    }

    WavSourceInfo wavInfo;
    std::string wavError;
    if (!readWavSourceInfo(req.inWavPath, wavInfo, wavError)) {
        report.errors.push_back("package-adm-wav: " + wavError);
        return result;
    }

    auto orderedIds = canonicalNodeOrder(conversion.scene);
    try {
        auto bw64File = bw64::readFile(req.inWavPath);
        if (bw64File->chnaChunk()) {
            const auto audioIds = bw64File->chnaChunk()->audioIds();
            if (audioIds.size() != orderedIds.size()) {
                report.warnings.push_back("package-adm-wav: chna UID count does not match LUSID node count; using canonical LUSID order");
            } else {
                std::vector<uint16_t> trackIndices;
                trackIndices.reserve(audioIds.size());
                for (const auto& audioId : audioIds) {
                    trackIndices.push_back(audioId.trackIndex());
                }
                std::sort(trackIndices.begin(), trackIndices.end());
                for (size_t i = 0; i < trackIndices.size(); ++i) {
                    if (trackIndices[i] != i + 1) {
                        report.warnings.push_back("package-adm-wav: chna track indices are not contiguous; using canonical LUSID order");
                        break;
                    }
                }
            }
        } else {
            report.warnings.push_back("package-adm-wav: source has no chna chunk; using canonical LUSID order");
        }
    } catch (const std::exception& e) {
        report.warnings.push_back(std::string("package-adm-wav: unable to inspect chna chunk: ") + e.what());
    }

    if (orderedIds.size() != wavInfo.channels) {
        report.errors.push_back("package-adm-wav: source channel count does not match converted LUSID node count");
        return result;
    }

    const fs::path packageDir(req.outPackageDir);
    const fs::path tmpDir(req.outPackageDir + ".tmp");
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);
    if (ec) {
        report.errors.push_back("package-adm-wav: failed to create temp package directory: " + ec.message());
        return result;
    }

    if (!writeLusidScene(conversion.scene, (tmpDir / "scene.lusid.json").string())) {
        report.errors.push_back("package-adm-wav: failed to write scene.lusid.json");
        fs::remove_all(tmpDir);
        return result;
    }

    std::ostringstream order;
    for (const auto& id : orderedIds) {
        order << id << "\n";
    }
    if (!writeTextAtomic(tmpDir / "channel_order.txt", order.str())) {
        report.errors.push_back("package-adm-wav: failed to write channel_order.txt");
        fs::remove_all(tmpDir);
        return result;
    }

    std::string splitError;
    if (!splitInterleavedToMono(req.inWavPath, wavInfo, orderedIds, tmpDir, req.onProgress, splitError)) {
        report.errors.push_back("package-adm-wav: " + splitError);
        fs::remove_all(tmpDir);
        return result;
    }

    report.status = "success";
    report.summary.timeUnit = "seconds";
    report.summary.numFrames = conversion.numFrames;
    report.summary.sampleRate = conversion.scene.sampleRate > 0 ? conversion.scene.sampleRate : static_cast<int>(wavInfo.sampleRate);
    report.summary.durationSec = static_cast<double>(wavInfo.frameCount) / static_cast<double>(wavInfo.sampleRate);
    report.summary.countsByNodeType = conversion.countsByNodeType;

    // Keep package-local report for users who inspect package folders directly.
    report.args.reportPath = req.reportPath.empty() ? (packageDir / "scene_report.json").string() : req.reportPath;
    if (!report.writeTo((tmpDir / "scene_report.json").string())) {
        report.errors.push_back("package-adm-wav: failed to write package scene_report.json");
        report.status = "fail";
        fs::remove_all(tmpDir);
        return result;
    }

    fs::remove_all(packageDir, ec);
    fs::rename(tmpDir, packageDir, ec);
    if (ec) {
        report.errors.push_back("package-adm-wav: failed to finalize package directory: " + ec.message());
        report.status = "fail";
        fs::remove_all(tmpDir);
        return result;
    }

    result.success = true;
    return result;
}

} // namespace cult
