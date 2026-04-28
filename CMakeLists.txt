# Copyright 2026 Cult-DSP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# ---------------------------------------------------------------------------
# CMakeLists.txt — CULT Transcoder
#
# Phase 1: builds cult-transcoder CLI + Catch2 test suite.
#          libadm and libbw64 are declared but EXCLUDE_FROM_ALL (disabled).
#          Un-comment EXCLUDE_FROM_ALL removal in Phase 2.
#
# Binary path expected by Spatial Root pipeline (AGENTS §9):
#   macOS / Linux : <build_dir>/cult-transcoder
#   Windows       : <build_dir>/cult-transcoder.exe  (called via .bat wrapper)
#
# Quick start:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ctest --test-dir build --output-on-failure
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)

project(cult_transcoder
    VERSION 0.1.0
    DESCRIPTION "CULT Transcoder — immersive audio format transcoder"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++ standard — C++17 pinned (AGENTS §0.5)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Default build type
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type set — defaulting to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Git commit SHA — injected into the binary at build time so every report
# carries the exact commit that produced it (AGENTS §7: tool.gitCommit).
# ---------------------------------------------------------------------------
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE CULT_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT CULT_GIT_COMMIT)
    set(CULT_GIT_COMMIT "unknown")
endif()
message(STATUS "cult-transcoder git commit: ${CULT_GIT_COMMIT}")

# ---------------------------------------------------------------------------
# FetchContent — third-party dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

# --- Catch2 (test framework, Phase 1+) -----------------------------------
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# --- pugixml (lightweight XML parser, MIT license) ------------------------
# Used instead of libadm so that C++ mirrors the Python oracle's raw XML
# traversal order exactly (AGENTS §3, §5 — ordering parity is non-negotiable).
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pugixml)

# --- nlohmann/json (LUSID reader — authoring path) -----------------------
# Used by src/parsing/lusid_reader.cpp to parse scene.lusid.json in the adm-author
# command.  Not used in the ingest/parity path.
if(NOT TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# --- libbw64 (EBU BW64/RF64 container reader, Apache-2.0, header-only) ---
# Phase 3+: used by transcoding/adm/adm_reader.cpp to extract the axml chunk
# from ADM BW64/WAV files (--in-format adm_wav).
# Added as a git submodule at thirdparty/libbw64 (not FetchContent) so that
# cult_transcoder owns the dependency without requiring spatialroot's
# thirdparty/libbw64 submodule.  (AGENTS-CULT §8, DEV-PLAN Phase 3)
set(LIBBW64_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libbw64")
if(NOT EXISTS "${LIBBW64_DIR}/include/bw64/bw64.hpp")
    message(FATAL_ERROR
        "libbw64 headers not found at ${LIBBW64_DIR}/include/bw64/bw64.hpp\n"
        "Run:  git submodule update --init thirdparty/libbw64\n"
        "from inside the cult_transcoder directory."
    )
endif()
add_library(libbw64 INTERFACE)
# SYSTEM suppresses warnings from libbw64 headers under -Werror (gcc treats
# plain INTERFACE includes as project headers, promoting warnings to errors).
target_include_directories(libbw64 SYSTEM INTERFACE "${LIBBW64_DIR}/include")

# --- r8brain-free-src (resampler, MIT license, header-only) ---------------
# Used by src/audio/resampler_r8brain.cpp in the adm-author normalization
# path.  Vendored as a git submodule at third_party/r8brain.
# Not used in the parity-critical adm_xml → lusid_json ingest path.
set(R8BRAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/r8brain")
if(NOT EXISTS "${R8BRAIN_DIR}/CDSPResampler.h")
    message(FATAL_ERROR
        "r8brain headers not found at ${R8BRAIN_DIR}/CDSPResampler.h\n"
        "Run:  git submodule update --init thirdparty/r8brain\n"
        "from inside the cult_transcoder directory."
    )
endif()

# ---------------------------------------------------------------------------
# Main executable — cult-transcoder
# ---------------------------------------------------------------------------
add_executable(cult-transcoder
    src/main.cpp
    src/reporting/report.cpp
    src/transcoder.cpp
    transcoding/adm/adm_to_lusid.cpp
    src/authoring/adm_author.cpp             # adm-author command (Steps 1-2 functional)
    src/authoring/adm_writer.cpp             # adm-writer abstraction mapping XML + BW64
    src/parsing/lusid_reader.cpp             # LUSID JSON reader (authoring path)
    src/audio/wav_io.cpp                     # self-contained RIFF WAV reader/writer
    src/audio/normalize_audio.cpp            # 48 kHz float32 normalization coordinator
    src/audio/resampler_r8brain.cpp          # r8brain-backed mono resampling
    src/packaging/adm_package.cpp            # ADM WAV -> LUSID package generation
    transcoding/adm/adm_reader.cpp           # Phase 3: BW64 axml extraction (libbw64)
    transcoding/adm/adm_profile_resolver.cpp # Phase 4: ADM profile detection
    transcoding/adm/sony360ra_to_lusid.cpp   # Phase 6: Sony 360RA ADM → LUSID
)

target_include_directories(cult-transcoder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/transcoding/adm  # adm_reader.hpp (Phase 3)
        ${R8BRAIN_DIR}                               # CDSPResampler.h (adm-author path)
)

target_compile_definitions(cult-transcoder
    PRIVATE
        CULT_GIT_COMMIT="${CULT_GIT_COMMIT}"
)

# Strict warnings — helps catch issues early on all platforms
if(MSVC)
    target_compile_options(cult-transcoder PRIVATE /W4 /WX)
    target_compile_definitions(cult-transcoder PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(cult-transcoder PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(cult-transcoder PRIVATE pugixml::pugixml libbw64 nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# Tests — Catch2
# ---------------------------------------------------------------------------
enable_testing()

add_executable(cult_tests
    tests/test_report.cpp
    tests/test_cli_args.cpp
    tests/test_lfe_mode.cpp                  # Phase 4: LFE mode + profile detection tests
    tests/test_360ra.cpp                     # Phase 6: Sony 360RA conversion tests
    tests/test_adm_author_args.cpp           # adm-author CLI validation tests
    tests/test_lusid_to_adm_mapping.cpp      # adm-author LUSID -> ADM mapping tests
    tests/test_adm_author_integration.cpp    # adm-author XML + BW64 integration tests
    tests/test_adm_package.cpp               # package-adm-wav roundtrip tests
    tests/parity/run_parity.cpp

    # Same source files the main binary uses, so tests exercise real code
    src/reporting/report.cpp
    src/transcoder.cpp
    transcoding/adm/adm_to_lusid.cpp
    src/authoring/adm_author.cpp
    src/authoring/adm_writer.cpp
    src/parsing/lusid_reader.cpp
    src/audio/wav_io.cpp
    src/audio/normalize_audio.cpp
    src/audio/resampler_r8brain.cpp
    src/packaging/adm_package.cpp
    transcoding/adm/adm_reader.cpp           # Phase 3: needed by transcoder
    transcoding/adm/adm_profile_resolver.cpp # Phase 4: ADM profile detection
    transcoding/adm/sony360ra_to_lusid.cpp   # Phase 6: Sony 360RA ADM → LUSID
)

target_include_directories(cult_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/transcoding/adm  # adm_reader.hpp (Phase 3)
        ${R8BRAIN_DIR}                               # CDSPResampler.h (adm-author path)
)

target_compile_definitions(cult_tests
    PRIVATE
        CULT_GIT_COMMIT="${CULT_GIT_COMMIT}"
        PARITY_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/parity/fixtures"
)

target_link_libraries(cult_tests
    PRIVATE
        Catch2::Catch2WithMain
        pugixml::pugixml
        libbw64
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(cult_tests PRIVATE /W4)
    target_compile_definitions(cult_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(cult_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
include(Catch)
catch_discover_tests(cult_tests)

# ---------------------------------------------------------------------------
# Install — not needed for Phase 1, scaffolded for completeness
# ---------------------------------------------------------------------------
install(TARGETS cult-transcoder DESTINATION bin)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== cult-transcoder build configuration ===")
message(STATUS "  Version     : ${PROJECT_VERSION}")
message(STATUS "  Git commit  : ${CULT_GIT_COMMIT}")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Binary path : ${CMAKE_BINARY_DIR}/cult-transcoder")
message(STATUS "  Tests       : enabled (Catch2 v3)")
message(STATUS "  XML parser  : pugixml (encounter-order parity with Python oracle)")
message(STATUS "  WAV reader  : libbw64 (git submodule, thirdparty/libbw64)")
message(STATUS "  Resampler   : r8brain-free-src (git submodule, thirdparty/r8brain)")
message(STATUS "  JSON parser : nlohmann/json v3.11.3 (FetchContent)")
message(STATUS "===========================================")
message(STATUS "")
