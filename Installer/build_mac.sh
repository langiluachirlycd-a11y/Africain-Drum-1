#!/bin/bash
# Compila e cria o instalador no macOS (precisa de Xcode Command Line Tools + CMake: brew install cmake)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-1.0.0}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(sysctl -n hw.ncpu)"
Installer/macos/build_pkg.sh "$VERSION"
echo; echo "Instalador em: Installer/output/"
