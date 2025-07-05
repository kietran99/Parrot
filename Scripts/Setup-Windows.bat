@echo off
setlocal EnableDelayedExpansion

REM Get absolute path to this script's directory (project-root)
set SCRIPT_DIR=%~dp0
REM Remove trailing backslash if exists
if "%SCRIPT_DIR:~-1%"=="\" set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

REM Path to premake-toolchain folder
set TOOLKIT_PREMAKE_PATH=%SCRIPT_DIR%\..\..\FlightKitCpp\Premake

REM Call get_binaries.py inside premake-toolchain
for /f "usebackq delims=" %%p in (`python "%TOOLKIT_PREMAKE_PATH%\get_binaries.py"`) do set PREMAKE=%%p

REM Check if PREMAKE path was returned
if not defined PREMAKE (
    echo ERROR: Failed to get premake path from get_binaries.py
    exit /b 1
)

pushd ..
"%PREMAKE%" --file=Build.lua vs2022
popd

endlocal
