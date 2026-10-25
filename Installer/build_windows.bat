@echo off
REM Compila e cria o instalador no Windows. Precisa de: Visual Studio 2022 (C++), CMake e Inno Setup 6 (iscc no PATH).
setlocal
cd /d "%~dp0\.."
set VERSION=%1
if "%VERSION%"=="" set VERSION=1.0.0
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release -j || exit /b 1
set ISCC=iscc
where iscc >nul 2>nul || set ISCC="%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
%ISCC% Installer\windows\African-Drum.iss /DAppVersion=%VERSION% /DBuildDir=%CD%\build\DrumForge_artefacts\Release /DOutDir=%CD%\Installer\output || exit /b 1
echo Instalador em Installer\output
