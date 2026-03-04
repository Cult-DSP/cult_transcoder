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

# ---------------------------------------------------------------------------
# Main executable — cult-transcoder
# ---------------------------------------------------------------------------
add_executable(cult-transcoder
    src/main.cpp
    src/report.cpp
    src/transcoder.cpp
    src/adm_to_lusid.cpp
)

target_include_directories(cult-transcoder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(cult-transcoder
    PRIVATE
        CULT_GIT_COMMIT="${CULT_GIT_COMMIT}"
)

# Strict warnings — helps catch issues early on all platforms
if(MSVC)
    target_compile_options(cult-transcoder PRIVATE /W4 /WX)
else()
    target_compile_options(cult-transcoder PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Phase 2: link libadm + libbw64
# target_link_libraries(cult-transcoder PRIVATE adm bw64)

# ---------------------------------------------------------------------------
# Tests — Catch2
# ---------------------------------------------------------------------------
enable_testing()

add_executable(cult_tests
    tests/test_report.cpp
    tests/test_cli_args.cpp

    # Same source files the main binary uses, so tests exercise real code
    src/report.cpp
    src/transcoder.cpp
)

target_include_directories(cult_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(cult_tests
    PRIVATE
        CULT_GIT_COMMIT="${CULT_GIT_COMMIT}"
)

target_link_libraries(cult_tests
    PRIVATE
        Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_options(cult_tests PRIVATE /W4)
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
message(STATUS "  libadm      : DISABLED (Phase 1 stub — enable in Phase 2)")
message(STATUS "  libbw64     : DISABLED (Phase 1 stub — enable in Phase 2)")
message(STATUS "===========================================")
message(STATUS "")
