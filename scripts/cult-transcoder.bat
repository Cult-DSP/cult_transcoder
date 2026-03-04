@echo off
REM ==========================================================================
REM  cult-transcoder.bat  --  Windows wrapper for cult-transcoder.exe
REM ==========================================================================
REM
REM  IMPORTANT: On Windows, Spatial Root's pipeline calls THIS wrapper instead
REM  of cult-transcoder.exe directly.  This lets all call-sites in the Python
REM  / shell pipeline avoid OS-specific branching — they always invoke the
REM  same script name regardless of platform.
REM
REM  HOW IT WORKS:
REM    %~dp0  expands to the directory containing this .bat file.
REM    All arguments (%*) are forwarded verbatim to the .exe next to it.
REM    Exit code from the .exe is propagated back to the caller.
REM
REM  IF THE BINARY PATH CHANGES:
REM    1. Move / rename cult-transcoder.exe in the CMake build.
REM    2. Update the path below to match.
REM    3. Update all call-sites in the Spatial Root pipeline that reference
REM       this wrapper (see AGENTS-CULT.md §9 for the canonical list).
REM
REM  BUILD NOTE:
REM    This file should be placed next to cult-transcoder.exe after the CMake
REM    build.  CMake installs it via the install(FILES ...) rule in
REM    CMakeLists.txt.  Do NOT commit the built .exe — commit only this .bat.
REM ==========================================================================

"%~dp0cult-transcoder.exe" %*
