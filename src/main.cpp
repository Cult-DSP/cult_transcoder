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
// main.cpp — CLI entry point for cult-transcoder
//
// CLI contract (AGENTS §2):
//
//   cult-transcoder transcode
//       --in <path>         input file path
//       --in-format <fmt>   input format  (currently: adm_xml)
//       --out <path>        output file path
//       --out-format <fmt>  output format (currently: lusid_json)
//       [--report <path>]   report file path (default: <out>.report.json)
//       [--stdout-report]   also print report JSON to stdout
//
// Exit codes:
//   0   success — output exists, report exists
//   1   failure — output must NOT exist, report written best-effort
//   2   usage / argument error — report written best-effort
//
// Atomic output rule (§2):
//   Outputs are written to temp paths then renamed.  On failure the temp
//   files are removed so no partial artifacts are left.
// ---------------------------------------------------------------------------

#include "cult_transcoder.hpp"
#include "reporting/cult_report.hpp"
#include "cult_version.hpp"
#include "transcoding/adm/adm_to_lusid.hpp"  // LfeMode

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static int cmdTranscode(const std::vector<std::string>& argv);
static int cmdAdmAuthor(const std::vector<std::string>& argv);
static int cmdPackageAdmWav(const std::vector<std::string>& argv);
static void printUsage(const std::string& progName);
static std::string defaultReportPath(const std::string& outPath);
static std::string defaultAuthorReportPath(const std::string& outWavPath,
                                           const std::string& outXmlPath);
static std::string defaultPackageReportPath(const std::string& outPackageDir);
static std::string nextArg(const std::vector<std::string>& argv,
                           size_t& index,
                           const std::string& name);
static bool requireFlag(const std::string& val, const std::string& name);
static cult::LfeMode parseLfeModeOrExit(const std::string& val);
static void writeAtomicReport(const cult::Report& report, const std::string& reportPath);
static void printErrors(const cult::Report& report);

class CliProgress {
public:
    explicit CliProgress(bool enabled) : enabled_(enabled) {}
    ~CliProgress() {
        if (enabled_ && lineOpen_) {
            std::cerr << "\n";
        }
    }

    cult::ProgressCallback callback() {
        return [this](const cult::ProgressEvent& event) { render(event); };
    }

private:
    void render(const cult::ProgressEvent& event) {
        if (!enabled_) return;
        using clock = std::chrono::steady_clock;
        const auto now = clock::now();
        const bool complete = event.total > 0 && event.current >= event.total;
        const bool phaseChanged = event.phase != lastPhase_;
        if (!phaseChanged && !complete && now - lastRender_ < std::chrono::milliseconds(100)) {
            return;
        }
        lastRender_ = now;
        lastPhase_ = event.phase;

        constexpr int width = 24;
        double ratio = 0.0;
        if (event.total > 0) {
            ratio = static_cast<double>(event.current) / static_cast<double>(event.total);
            if (ratio < 0.0) ratio = 0.0;
            if (ratio > 1.0) ratio = 1.0;
        }
        const int filled = static_cast<int>(ratio * width);

        std::ostringstream bar;
        for (int i = 0; i < width; ++i) {
            bar << (i < filled ? '#' : '-');
        }

        std::cerr << "\r[cult-transcoder] " << std::left << std::setw(10) << event.phase
                  << " [" << bar.str() << "] "
                  << std::right << std::setw(3) << static_cast<int>(ratio * 100.0) << "%";
        if (!event.detail.empty()) {
            std::cerr << " " << event.detail;
        }
        std::cerr << std::flush;
        lineOpen_ = true;
        if (complete) {
            std::cerr << "\n";
            lineOpen_ = false;
        }
    }

    bool enabled_ = false;
    bool lineOpen_ = false;
    std::string lastPhase_;
    std::chrono::steady_clock::time_point lastRender_{};
};

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    const std::string prog = args.empty() ? "cult-transcoder" : args[0];

    if (args.size() < 2) {
        printUsage(prog);
        return 2;
    }

    const std::string subcommand = args[1];

    if (subcommand == "transcode") {
        return cmdTranscode(args);
    }

    if (subcommand == "adm-author") {
        return cmdAdmAuthor(args);
    }

    if (subcommand == "package-adm-wav") {
        return cmdPackageAdmWav(args);
    }

    if (subcommand == "--version" || subcommand == "-v") {
        std::cout << cult::kToolName << " " << cult::kVersionString
                  << " (report schema " << cult::kReportSchemaVersion << ")\n";
        return 0;
    }

    if (subcommand == "--help" || subcommand == "-h") {
        printUsage(prog);
        return 0;
    }

    std::cerr << "[cult-transcoder] Unknown subcommand: " << subcommand << "\n";
    printUsage(prog);
    return 2;
}

// ---------------------------------------------------------------------------
// cmdTranscode() — parse args and invoke the transcoder
// ---------------------------------------------------------------------------
static int cmdTranscode(const std::vector<std::string>& argv) {
    cult::TranscodeRequest req;

    // --- Parse ---
    for (size_t i = 2; i < argv.size(); ++i) {
        const std::string& flag = argv[i];

        if      (flag == "--in")           req.inPath      = nextArg(argv, i, "--in");
        else if (flag == "--in-format")    req.inFormat    = nextArg(argv, i, "--in-format");
        else if (flag == "--out")          req.outPath     = nextArg(argv, i, "--out");
        else if (flag == "--out-format")   req.outFormat   = nextArg(argv, i, "--out-format");
        else if (flag == "--report")       req.reportPath  = nextArg(argv, i, "--report");
        else if (flag == "--stdout-report") req.stdoutReport = true;
        else if (flag == "--lfe-mode") {
            const std::string val = nextArg(argv, i, "--lfe-mode");
            req.lfeMode = parseLfeModeOrExit(val);
        }
        else {
            std::cerr << "[cult-transcoder] ERROR: unknown flag: " << flag << "\n";
            printUsage(argv[0]);
            std::exit(2);
        }
    }

    // --- Validate required flags ---
    bool argError = false;
    argError = requireFlag(req.inPath,    "--in") || argError;
    argError = requireFlag(req.inFormat,  "--in-format") || argError;
    argError = requireFlag(req.outPath,   "--out") || argError;
    argError = requireFlag(req.outFormat, "--out-format") || argError;

    if (argError) {
        printUsage(argv[0]);
        // Write a best-effort fail report even for arg errors (§0.5)
        // We have no out path yet if --out was missing, so only write if we can.
        if (!req.outPath.empty() || !req.reportPath.empty()) {
            std::string rpath = req.reportPath.empty()
                                ? defaultReportPath(req.outPath)
                                : req.reportPath;
            cult::Report failReport;
            failReport.status = "fail";
            failReport.args.inPath       = req.inPath;
            failReport.args.inFormat     = req.inFormat;
            failReport.args.outPath      = req.outPath;
            failReport.args.outFormat    = req.outFormat;
            failReport.args.reportPath   = rpath;
            failReport.args.stdoutReport = req.stdoutReport;
            failReport.args.lfeMode      = (req.lfeMode == cult::LfeMode::SpeakerLabel)
                                               ? "speaker-label" : "hardcoded";
            failReport.errors.push_back("Argument error — see stderr for details");
            failReport.writeTo(rpath);
            if (req.stdoutReport) failReport.printToStdout();
        }
        return 2;
    }

    // --- Resolve default report path ---
    if (req.reportPath.empty()) {
        req.reportPath = defaultReportPath(req.outPath);
    }

    // --- Invoke transcoder ---
    cult::TranscodeResult result = cult::transcode(req);

    // --- Atomic output (§2) ---
    // Phase 1: there is no LUSID output yet, so only the report file is
    // written atomically.  Phase 2 will add the temp→rename for the LUSID
    // output file.
    writeAtomicReport(result.report, req.reportPath);

    if (req.stdoutReport) {
        result.report.printToStdout();
    }

    if (!result.success) {
        // Print errors to stderr as required by §2
        printErrors(result.report);
        // Remove any partial temp files (none in Phase 1, but be explicit)
        const std::string reportTmp = req.reportPath + ".tmp";
        if (fs::exists(reportTmp)) fs::remove(reportTmp);
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// cmdAdmAuthor() — parse args and invoke the ADM authoring path
// ---------------------------------------------------------------------------
static int cmdAdmAuthor(const std::vector<std::string>& argv) {
    cult::AdmAuthorRequest req;

    // --- Parse ---
    for (size_t i = 2; i < argv.size(); ++i) {
        const std::string& flag = argv[i];

        if      (flag == "--lusid")         req.lusidPath    = nextArg(argv, i, "--lusid");
        else if (flag == "--wav-dir")       req.wavDir       = nextArg(argv, i, "--wav-dir");
        else if (flag == "--lusid-package") req.lusidPackage = nextArg(argv, i, "--lusid-package");
        else if (flag == "--out-xml")       req.outXmlPath   = nextArg(argv, i, "--out-xml");
        else if (flag == "--out-wav")       req.outWavPath   = nextArg(argv, i, "--out-wav");
        else if (flag == "--dbmd-source")   req.dbmdSourcePath = nextArg(argv, i, "--dbmd-source");
        else if (flag == "--metadata-post-data") req.metadataPostData = true;
        else if (flag == "--report")        req.reportPath   = nextArg(argv, i, "--report");
        else if (flag == "--stdout-report") req.stdoutReport = true;
        else if (flag == "--quiet")         req.quiet        = true;
        else {
            std::cerr << "[cult-transcoder] ERROR: unknown flag: " << flag << "\n";
            printUsage(argv[0]);
            std::exit(2);
        }
    }

    // --- Validate required flags ---
    bool argError = false;

    const bool hasPackage = !req.lusidPackage.empty();
    if (!hasPackage) {
        argError = requireFlag(req.lusidPath, "--lusid") || argError;
        argError = requireFlag(req.wavDir, "--wav-dir") || argError;
    }
    argError = requireFlag(req.outXmlPath, "--out-xml") || argError;
    argError = requireFlag(req.outWavPath, "--out-wav") || argError;

    if (argError) {
        printUsage(argv[0]);
        // Best-effort fail report for arg errors.
        if (!req.outWavPath.empty() || !req.outXmlPath.empty() || !req.reportPath.empty()) {
            std::string rpath = req.reportPath.empty()
                                ? defaultAuthorReportPath(req.outWavPath, req.outXmlPath)
                                : req.reportPath;
            cult::Report failReport;
            failReport.status = "fail";
            failReport.args.inPath      = req.lusidPackage.empty() ? req.lusidPath : req.lusidPackage;
            failReport.args.inFormat    = "lusid_json";
            failReport.args.outPath     = req.outWavPath.empty() ? req.outXmlPath : req.outWavPath;
            failReport.args.outFormat   = "adm_bw64";
            failReport.args.reportPath  = rpath;
            failReport.args.stdoutReport = req.stdoutReport;
            failReport.errors.push_back("Argument error — see stderr for details");
            failReport.writeTo(rpath);
            if (req.stdoutReport) failReport.printToStdout();
        }
        return 2;
    }

    // --- Resolve default report path ---
    if (req.reportPath.empty()) {
        req.reportPath = defaultAuthorReportPath(req.outWavPath, req.outXmlPath);
    }

    // --- Invoke authoring ---
    CliProgress progress(!req.quiet);
    req.onProgress = progress.callback();
    cult::AdmAuthorResult result = cult::admAuthor(req);

    // --- Atomic report write ---
    writeAtomicReport(result.report, req.reportPath);

    if (req.stdoutReport) {
        result.report.printToStdout();
    }

    if (!result.success) {
        printErrors(result.report);
        const std::string reportTmp = req.reportPath + ".tmp";
        if (fs::exists(reportTmp)) fs::remove(reportTmp);
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// cmdPackageAdmWav() — split an ADM WAV into a LUSID package
// ---------------------------------------------------------------------------
static int cmdPackageAdmWav(const std::vector<std::string>& argv) {
    cult::PackageAdmWavRequest req;

    for (size_t i = 2; i < argv.size(); ++i) {
        const std::string& flag = argv[i];

        if      (flag == "--in")             req.inWavPath     = nextArg(argv, i, "--in");
        else if (flag == "--out-package")    req.outPackageDir = nextArg(argv, i, "--out-package");
        else if (flag == "--report")         req.reportPath    = nextArg(argv, i, "--report");
        else if (flag == "--stdout-report")  req.stdoutReport  = true;
        else if (flag == "--quiet")          req.quiet         = true;
        else if (flag == "--lfe-mode")       req.lfeMode       = parseLfeModeOrExit(nextArg(argv, i, "--lfe-mode"));
        else {
            std::cerr << "[cult-transcoder] ERROR: unknown flag: " << flag << "\n";
            printUsage(argv[0]);
            std::exit(2);
        }
    }

    bool argError = false;
    argError = requireFlag(req.inWavPath, "--in") || argError;
    argError = requireFlag(req.outPackageDir, "--out-package") || argError;

    if (argError) {
        printUsage(argv[0]);
        if (!req.outPackageDir.empty() || !req.reportPath.empty()) {
            const std::string rpath = req.reportPath.empty()
                                      ? defaultPackageReportPath(req.outPackageDir)
                                      : req.reportPath;
            cult::Report failReport;
            failReport.status = "fail";
            failReport.args.inPath = req.inWavPath;
            failReport.args.inFormat = "adm_wav";
            failReport.args.outPath = req.outPackageDir;
            failReport.args.outFormat = "lusid_package";
            failReport.args.reportPath = rpath;
            failReport.args.stdoutReport = req.stdoutReport;
            failReport.args.lfeMode = req.lfeMode == cult::LfeMode::SpeakerLabel ? "speaker-label" : "hardcoded";
            failReport.errors.push_back("Argument error — see stderr for details");
            failReport.writeTo(rpath);
            if (req.stdoutReport) failReport.printToStdout();
        }
        return 2;
    }

    if (req.reportPath.empty()) {
        req.reportPath = defaultPackageReportPath(req.outPackageDir);
    }

    CliProgress progress(!req.quiet);
    req.onProgress = progress.callback();
    cult::PackageAdmWavResult result = cult::packageAdmWav(req);

    writeAtomicReport(result.report, req.reportPath);

    if (req.stdoutReport) {
        result.report.printToStdout();
    }

    if (!result.success) {
        printErrors(result.report);
        const std::string reportTmp = req.reportPath + ".tmp";
        if (fs::exists(reportTmp)) fs::remove(reportTmp);
        return 1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string defaultReportPath(const std::string& outPath) {
    // Contract §2: default report path is "<out>.report.json"
    return outPath.empty() ? "cult-transcoder.report.json"
                           : outPath + ".report.json";
}

static std::string defaultAuthorReportPath(const std::string& outWavPath,
                                           const std::string& outXmlPath) {
    if (!outWavPath.empty()) return outWavPath + ".report.json";
    if (!outXmlPath.empty()) return outXmlPath + ".report.json";
    return "cult-transcoder.report.json";
}

static std::string defaultPackageReportPath(const std::string& outPackageDir) {
    if (!outPackageDir.empty()) {
        return (fs::path(outPackageDir) / "scene_report.json").string();
    }
    return "cult-transcoder.package.report.json";
}

static cult::LfeMode parseLfeModeOrExit(const std::string& val) {
    if (val == "hardcoded") {
        return cult::LfeMode::Hardcoded;
    }
    if (val == "speaker-label") {
        return cult::LfeMode::SpeakerLabel;
    }
    std::cerr << "[cult-transcoder] ERROR: unknown --lfe-mode value: '"
              << val << "'. Valid values: hardcoded, speaker-label\n";
    std::exit(2);
    return cult::LfeMode::Hardcoded;
}

static std::string nextArg(const std::vector<std::string>& argv,
                           size_t& index,
                           const std::string& name) {
    if (index + 1 >= argv.size()) {
        std::cerr << "[cult-transcoder] ERROR: " << name
                  << " requires an argument\n";
        std::exit(2);
    }
    return argv[++index];
}

static bool requireFlag(const std::string& val, const std::string& name) {
    if (val.empty()) {
        std::cerr << "[cult-transcoder] ERROR: missing required flag: " << name << "\n";
        return true;
    }
    return false;
}

static void writeAtomicReport(const cult::Report& report, const std::string& reportPath) {
    const std::string reportTmp = reportPath + ".tmp";
    bool reportWritten = report.writeTo(reportTmp);
    if (reportWritten) {
        std::error_code ec;
        fs::rename(reportTmp, reportPath, ec);
        if (ec) {
            std::cerr << "[cult-transcoder] WARNING: could not rename report: "
                      << ec.message() << "\n";
        }
    }
}

static void printErrors(const cult::Report& report) {
    for (const auto& e : report.errors) {
        std::cerr << "[cult-transcoder] ERROR: " << e << "\n";
    }
}

static void printUsage(const std::string& progName) {
    std::cerr <<
        "Usage: " << progName << " transcode\n"
        "           --in <path>         input file\n"
        "           --in-format <fmt>   input format  (adm_xml, adm_wav)\n"
        "           --out <path>        output file\n"
        "           --out-format <fmt>  output format (lusid_json)\n"
        "           [--report <path>]   report path (default: <out>.report.json)\n"
        "           [--stdout-report]   also print report to stdout\n"
        "           [--lfe-mode <val>]  LFE detection: hardcoded (default) | speaker-label\n"
        "\n"
        "       " << progName << " adm-author\n"
        "           --lusid <scene.lusid.json> --wav-dir <path>\n"
        "           --out-xml <export.adm.xml> --out-wav <export.wav>\n"
        "           [--report <path>] [--stdout-report] [--quiet]\n"
        "           [--dbmd-source <source.wav|dbmd.bin>]\n"
        "           [--metadata-post-data]\n"
        "\n"
        "       " << progName << " adm-author\n"
        "           --lusid-package <path>\n"
        "           --out-xml <export.adm.xml> --out-wav <export.wav>\n"
        "           [--report <path>] [--stdout-report] [--quiet]\n"
        "           [--dbmd-source <source.wav|dbmd.bin>]\n"
        "           [--metadata-post-data]\n"
        "\n"
        "       " << progName << " package-adm-wav\n"
        "           --in <source.wav> --out-package <package-dir>\n"
        "           [--report <path>] [--stdout-report] [--quiet]\n"
        "           [--lfe-mode <val>]  LFE detection: hardcoded (default) | speaker-label\n"
        "\n"
        "       " << progName << " --version\n"
        "       " << progName << " --help\n";
}
