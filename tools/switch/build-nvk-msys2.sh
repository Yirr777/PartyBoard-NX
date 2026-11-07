#!/bin/bash
# Build mesa-switch's NVK (Vulkan only) on an x86-64 Windows host with MSYS2, then package it
# as extern/nvk-switch/lib/libvulkan.a (one partially linked object, vk* symbols global).
#
#   C:\msys64\usr\bin\bash.exe -l tools/switch/build-nvk-msys2.sh
#
# Needs tools/switch/msys2-deps.sh and a Rust toolchain with the aarch64-unknown-linux-gnu std in
# extern/mesa-switch/build/deps/{rustup,cargo} (see docs/switch.md).
set -euo pipefail

PORT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="$PORT/extern/mesa-switch"
DEST="$PORT/extern/nvk-switch"
HOST_BUILD="$SRC/builddir-native-x64-host"
BUILD="$SRC/builddir-switch"
MESA_REF=0ff52079ca9e9e837ed27eb95a8d239f439a1864

export PATH="/clang64/bin:/usr/bin:$PATH"
export MESA_SWITCH_RUST_TARGET=aarch64-unknown-linux-gnu
export MESA_SWITCH_BINDGEN=/clang64/bin/bindgen.exe
# bindgen-switch-wrapper.sh hands these to the native Windows clang, so they must be Windows paths.
export DEVKITPRO=C:/msys64/opt/devkitpro
export MSYS2_ARG_CONV_EXCL="--prefix="
export LC_ALL=C

cd "$SRC"
if [ "$(git rev-parse HEAD)" != "$MESA_REF" ]; then
    echo "mesa-switch is not at $MESA_REF" >&2
    exit 1
fi

TMPD="$SRC/build/tmp"
mkdir -p "$TMPD"
export TMP="$(cygpath -m "$TMPD")" TEMP="$(cygpath -m "$TMPD")" TMPDIR="$TMPD"

# Native-tool file for an x64 host (upstream's points at CLANGARM64's cbindgen).
NATIVE="$SRC/build/switch_native_tools_x64.txt"
cat > "$NATIVE" <<EOF
[binaries]
mesa_clc = ['C:/msys64/usr/bin/bash.exe', '@GLOBAL_SOURCE_ROOT@/host-tool-msys2-wrapper.sh', 'builddir-native-x64-host/src/compiler/clc/mesa_clc.exe']
vtn_bindgen2 = ['C:/msys64/usr/bin/bash.exe', '@GLOBAL_SOURCE_ROOT@/host-tool-msys2-wrapper.sh', 'builddir-native-x64-host/src/compiler/spirv/vtn_bindgen2.exe']
cbindgen = 'C:/msys64/clang64/bin/cbindgen.exe'
rust = ['C:/msys64/usr/bin/bash.exe', '@GLOBAL_SOURCE_ROOT@/rustc-native-msys2-wrapper.sh']
EOF

# Rust std for aarch64-unknown-linux-gnu asks for -ldl -lrt -lutil; mesa provides the symbols.
LIBS=/opt/devkitpro/portlibs/switch/lib
if [ ! -f "$LIBS/libdl.a" ]; then
    echo "void __mesa_switch_stub_lib(void) {}" > "$TMPD/stub_posix.c"
    /opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc -c "$TMPD/stub_posix.c" -o "$TMPD/stub_posix.o"
    for L in dl rt util; do
        /opt/devkitpro/devkitA64/bin/aarch64-none-elf-ar rcs "$LIBS/lib$L.a" "$TMPD/stub_posix.o"
    done
fi
cp src/gallium/winsys/nouveau/drm/nouveau.h /opt/devkitpro/portlibs/switch/include/
cp src/nouveau/headers/nv_device_info.h /opt/devkitpro/portlibs/switch/include/

if [ ! -x "$HOST_BUILD/src/compiler/clc/mesa_clc.exe" ] || [ ! -x "$HOST_BUILD/src/compiler/spirv/vtn_bindgen2.exe" ]; then
    echo "==> host tools"
    if [ ! -f "$HOST_BUILD/build.ninja" ]; then
        meson setup "$HOST_BUILD" \
            --cross-file windows_x64_host_cross_file_msys2.txt \
            --buildtype=release \
            -Dvulkan-drivers= -Dgallium-drivers= -Dshader-cache=disabled -Dplatforms= \
            -Dglx=disabled -Degl=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
            -Dtools=[] -Dllvm=enabled -Dmesa-clc=enabled -Dprecomp-compiler=enabled \
            -Dinstall-mesa-clc=true
    fi
    ninja -C "$HOST_BUILD" src/compiler/clc/mesa_clc.exe src/compiler/spirv/vtn_bindgen2.exe
fi

echo "==> cross configure"
if [ ! -f "$BUILD/build.ninja" ]; then
    meson setup "$BUILD" \
        --cross-file switch_cross_file_msys2.txt \
        --native-file "$NATIVE" \
        --default-library=static \
        --buildtype=release \
        -Doptimization=2 -Db_lto=false -Db_ndebug=true \
        -Dvulkan-drivers=nouveau -Dgallium-drivers= -Dgallium-rusticl=false \
        -Dplatforms=switch -Dglx=disabled -Degl=disabled -Dopengl=false \
        -Dgles1=disabled -Dgles2=disabled -Dvideo-codecs= \
        -Dshader-cache=enabled -Dxmlconfig=enabled -Dexpat=enabled -Dtools=[] \
        -Dllvm=disabled -Dshared-glapi=disabled -Dshared-llvm=disabled \
        -Dmesa-clc=system -Dprecomp-compiler=system -Dcpp_rtti=false -Dbuild-tests=false \
        -Dnvk-build-id="$MESA_REF"
fi

echo "==> cross build"
ninja -C "$BUILD" src/nouveau/vulkan/libvulkan.a

echo "==> package"
rm -rf "$DEST"
mkdir -p "$DEST"
MESA_REF="$MESA_REF" OWNER= sh "$PORT/tools/switch/package-nvk.sh" "$SRC" "$DEST"
echo "$MESA_REF" > "$DEST/.mp4-nvk"
echo "==> ok: $DEST/lib/libvulkan.a"
