#!/bin/bash
# Stage the SD card layout and zip it: switch/partyboard/{partyboard.nro,res/,licences}.
#   C:\msys64\usr\bin\bash.exe -l tools/switch/package.sh [build-dir]
set -eo pipefail
PORT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${1:-$PORT/build-switch}"
cd "$PORT"
if [ ! -f "$BUILD/partyboard.nro" ]; then
    echo "package.sh: no $BUILD/partyboard.nro; run tools/switch/build.sh first" >&2
    exit 1
fi
STAGE="$BUILD/sd"
OUT="$STAGE/switch/partyboard"
rm -rf "$STAGE"
mkdir -p "$OUT"
cp "$BUILD/partyboard.nro" "$OUT/"
cp -r res "$OUT/res"
# Shaders compiled on a console, so the first launch does not pause to compile them
# (src/port/portmain.cpp). How to refresh them: docs/port-notes.md, "Bundled shader cache".
cp platforms/switch/shader-cache/initial_pipeline_cache.db platforms/switch/shader-cache/initial_dawn_cache.db "$OUT/"
mkdir -p "$OUT/licenses"
cp LICENSE "$OUT/licenses/LICENSE-PARTYBOARD.txt"
cp extern/aurora/LICENSE "$OUT/licenses/LICENSE-AURORA.txt"
cp extern/musyx/LICENSE "$OUT/licenses/LICENSE-MUSYX.txt" 2>/dev/null || true
cp extern/sdl3on2/LICENSE.txt "$OUT/licenses/LICENSE-SDL3ON2.txt"
cp extern/dawn-switch/LICENSE "$OUT/licenses/LICENSE-DAWN.txt"
cp platforms/switch/LICENSE-LIBNX.txt "$OUT/licenses/"
cp extern/nvk-switch/NOTICE.md "$OUT/licenses/NOTICE-NVK.md"
cp platforms/switch/THIRD_PARTY.md "$OUT/licenses/THIRD_PARTY.md"
cp extern/libco/LICENSE "$OUT/licenses/LICENSE-LIBCO.txt"
D=extern/dawn-switch/third_party
cp "$D/abseil-cpp/LICENSE" "$OUT/licenses/LICENSE-ABSEIL.txt"
cp "$D/spirv-tools/LICENSE" "$OUT/licenses/LICENSE-SPIRV-TOOLS.txt"
cp "$D/spirv-headers/LICENSE" "$OUT/licenses/LICENSE-SPIRV-HEADERS.txt"
T="$BUILD/_deps"
cp "$T/fmt-src/LICENSE" "$OUT/licenses/LICENSE-FMT.txt"
cp "$T/imgui-src/LICENSE.txt" "$OUT/licenses/LICENSE-IMGUI.txt"
cp "$T/json-src/LICENSE.MIT" "$OUT/licenses/LICENSE-JSON.txt"
cp "$T/rmlui-src/LICENSE.txt" "$OUT/licenses/LICENSE-RMLUI.txt"
cp "$T/xxhash-src/LICENSE" "$OUT/licenses/LICENSE-XXHASH.txt"
# devkitPro's portlibs ship these; the rest are listed with their source in THIRD_PARTY.md.
for lib in bzip2 freetype libpng libzstd zlib; do
    cp -r "/opt/devkitpro/portlibs/switch/licenses/switch-$lib" "$OUT/licenses/" 2>/dev/null || \
        echo "package.sh: no license folder for $lib" >&2
done
cp platforms/switch/README-SD.txt "$OUT/README.txt"
# An existing file turns logging on (see nx_init.c); early builds ship with it for bug reports.
: > "$OUT/partyboard-log.txt"
mkdir -p dist
rm -f dist/partyboard-switch.zip
(cd "$STAGE" && zip -qrX "$PORT/dist/partyboard-switch.zip" switch)
echo "==> ok: dist/partyboard-switch.zip"
