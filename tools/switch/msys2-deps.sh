#!/bin/bash
# Installs the MSYS2/devkitPro packages needed to build NVK (mesa-switch), Dawn and the game.
# Run from an MSYS2 shell: bash tools/switch/msys2-deps.sh
set -e
pacman -S --noconfirm --needed \
    devkitA64 switch-dev switch-sdl2 switch-mesa switch-libdrm_nouveau switch-zlib switch-libzstd \
    switch-pkg-config switch-libexpat switch-freetype switch-libpng switch-bzip2 switch-xxhash \
    flex bison git zip unzip patch \
    mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-llvm mingw-w64-clang-x86_64-libclc \
    mingw-w64-clang-x86_64-spirv-llvm-translator mingw-w64-clang-x86_64-spirv-tools \
    mingw-w64-clang-x86_64-spirv-headers mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja \
    mingw-w64-clang-x86_64-pkgconf mingw-w64-clang-x86_64-python mingw-w64-clang-x86_64-python-mako \
    mingw-w64-clang-x86_64-python-yaml mingw-w64-clang-x86_64-python-packaging \
    mingw-w64-clang-x86_64-meson mingw-w64-clang-x86_64-zlib mingw-w64-clang-x86_64-zstd \
    mingw-w64-clang-x86_64-expat mingw-w64-clang-x86_64-rust-bindgen mingw-w64-clang-x86_64-cbindgen \
    mingw-w64-ucrt-x86_64-gcc
echo "==> msys2 deps ok"
