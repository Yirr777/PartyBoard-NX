#!/bin/bash
# Configure and build Party Board for Nintendo Switch.
#   C:\msys64\usr\bin\bash.exe -l tools/switch/build.sh [build-dir]
# Run tools/switch/msys2-deps.sh, tools/switch/build-nvk-msys2.sh and
# tools/switch/fetch-switch-deps.sh first.
set -eo pipefail
PORT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$PORT/build-switch}"
export DEVKITPRO=/opt/devkitpro
export PATH="/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:/usr/bin:$PATH"
cd "$PORT"
if [ ! -f "$BUILD/build.ninja" ]; then
    cmake -S . -B "$BUILD" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$PORT/platforms/switch/toolchain-switch.cmake" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_SHARED_LIBS=OFF \
        -DMP4_SWITCH_DAWN_ASSERTS=OFF \
        -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
        -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
        -DCMAKE_DISABLE_FIND_PACKAGE_absl=ON \
        -DCMAKE_DISABLE_FIND_PACKAGE_fmt=ON \
        -DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON \
        -DPython3_EXECUTABLE=/usr/bin/python3
fi
cmake --build "$BUILD" --target partyboard_nro -- -k 0 "${@:2}"
