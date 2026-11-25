@echo off
setlocal
cd /d "%~dp0"

echo ===== QuadMotion: build and install =====
echo.

where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake was not found. Install CMake and Visual Studio 2022 with "Desktop development with C++".
  goto :fail
)

where git >nul 2>nul
if errorlevel 1 (
  echo Git was not found. Install Git for Windows ^(JUCE is downloaded with it on the first run^).
  goto :fail
)

cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

cmake --build build --config Release
if errorlevel 1 goto :fail

set "SRC=build\QuadMotion_artefacts\Release\VST3\QuadMotion.vst3"
set "DST=%CommonProgramFiles%\VST3\QuadMotion.vst3"

if not exist "%SRC%" (
  echo Build finished but "%SRC%" was not found.
  goto :fail
)

echo.
echo Copying to "%DST%" ...
xcopy "%SRC%" "%DST%\" /E /I /Y >nul
if errorlevel 1 (
  echo Copy failed. Right-click this file and choose "Run as administrator".
  goto :fail
)

echo.
echo Done. In Cubase: Studio ^> VST Plug-in Manager ^> refresh.
pause
exit /b 0

:fail
echo.
echo Something went wrong - see the messages above.
pause
exit /b 1
