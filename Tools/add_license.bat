@echo off
setlocal

rem Usage: add_license.bat [--dry-run]

set "SCRIPT_DIR=%~dp0"
set "RESULT=0"

rem --- locate add_license.rs (next to this file, or in Tools\) ---
set "RS_FILE="
if exist "%SCRIPT_DIR%add_license.rs"       set "RS_FILE=%SCRIPT_DIR%add_license.rs"
if exist "%SCRIPT_DIR%Tools\add_license.rs" set "RS_FILE=%SCRIPT_DIR%Tools\add_license.rs"
if not defined RS_FILE goto :no_rs

rem --- locate repo root (the folder containing the top-level CMakeLists.txt) ---
set "ROOT="
if exist "%SCRIPT_DIR%CMakeLists.txt" set "ROOT=%SCRIPT_DIR%"
if not defined ROOT if exist "%SCRIPT_DIR%..\CMakeLists.txt" (
    for %%I in ("%SCRIPT_DIR%..") do set "ROOT=%%~fI"
)
if not defined ROOT goto :no_root

rem --- check rustc ---
where rustc >nul 2>nul
if errorlevel 1 goto :no_rustc

rem --- compile to a temp folder so nothing is left in the repo ---
set "EXE=%TEMP%\da_add_license.exe"
echo Compiling %RS_FILE% ...
rustc -O "%RS_FILE%" -o "%EXE%"
if errorlevel 1 goto :compile_failed

rem --- run from the repo root ---
echo Running in %ROOT%
pushd "%ROOT%"
"%EXE%" %*
set "RESULT=%ERRORLEVEL%"
popd

del "%EXE%" >nul 2>nul
goto :finish

:no_rs
echo ERROR: add_license.rs not found next to this file or in a Tools\ folder.
set "RESULT=1"
goto :finish

:no_root
echo ERROR: could not find the repo root (no CMakeLists.txt in this folder or its parent).
set "RESULT=1"
goto :finish

:no_rustc
echo ERROR: rustc not found on PATH. Install Rust from https://rustup.rs
set "RESULT=1"
goto :finish

:compile_failed
echo ERROR: compilation failed.
set "RESULT=1"
goto :finish

:finish
echo.
pause
exit /b %RESULT%