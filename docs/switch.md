# Party Board for Nintendo Switch: build guide

A port of Party Board (Mario Party 4 on Aurora) to Horizon/libnx. The graphics recipe comes from
`new-coke/strikers` (CC0): Aurora and Dawn (`new-coke/dawn-switch`) on NVK
(`danfromtico/mesa-switch`), with SDL3 provided by `new-coke/sdl3on2` over devkitPro's SDL2.

See [port-notes.md](port-notes.md) for the problems found along the way.

## What differs from Party Board on PC

| Part | PC | Switch | Where |
|---|---|---|---|
| Overlays (91 RELs) | One DLL/.so per REL, `LoadLibrary`/`dlopen` | All linked into the `.nro`. Each REL is isolated (`ld -r` and `objcopy`), with its globals in `mp4ovl_<rel>`/`mp4bss_<rel>`, restored on each load | `platforms/switch/rel_isolate.sh`, `rel_registry*`, `src/game/objdll.c` |
| Disc reading | `nod` (Rust) | `nodlite` (C): GameCube ISO/GCM, CISO, GCZ | `platforms/switch/nodlite/` |
| Graphics | Prebuilt Dawn (D3D12/Vulkan/Metal) | Dawn built from `dawn-switch`, Vulkan backend on static NVK, with OpenGL ES as a fallback | `switch_pre_aurora.cmake`, Aurora patch |
| SDL3 | Real SDL3 | `sdl3on2` with `include/SDL3/s3_mp4_compat.h` (cursors, clipboard, dialogs: no-ops) | `extern/sdl3on2/` |
| Audio output | MusyX's SDL adapter | `platforms/switch/hw_pc_nx.c`: 32 kHz mixer thread on core 2 | `CMakeLists.txt` |
| Disc path | File picker | First disc image beside the `.nro`, then the one saved; Startup > Disc Image when there are several | `src/port/portmain.cpp`, `src/port/ui/settings.cpp` |
| Language | Setting | The console's language on European discs, unless one is picked | `platforms/switch/nx_init.c` |
| Controllers | GameCube positions | By Switch label (A confirms, B cancels) | `src/port/portmain.cpp` |
| Memory card | 16 MiB raw image | 512 KiB raw image; larger ones are migrated, keeping a `.bak` | Aurora patch, `lib/dolphin/card.cpp` |
| Process | n/a | `userAppInit`: home folder, log, romfs, NVK, SQLite VFS, CPU boost, 16 MiB thread stacks | `platforms/switch/nx_init.c` |

Changes to third-party code live in `platforms/switch/patches/` (Dawn, abseil, EGL-Registry, Aurora,
libco, and RmlUi for the MSYS host). `tools/switch/fetch-switch-deps.sh` applies them. If you change
`extern/dawn-switch`, `extern/aurora` or `extern/libco`, export the patch again: those trees are not
part of this repository's commits.

## Build (Windows and MSYS2)

Requirements: MSYS2 in `C:\msys64` with the devkitPro repositories (`dkp-libs`, `dkp-windows`).

```sh
# In C:\msys64\usr\bin\bash.exe -l, from the repository root
git submodule update --init
bash tools/switch/msys2-deps.sh          # devkitA64, libnx, portlibs, LLVM/clang64, meson
git clone https://github.com/danfromtico/mesa-switch extern/mesa-switch
git -C extern/mesa-switch checkout 0ff52079ca9e9e837ed27eb95a8d239f439a1864
# Rust in extern/mesa-switch/build/deps (rustup-init -y --default-host x86_64-pc-windows-gnu
#   --default-toolchain 1.99.1, with RUSTUP_HOME/CARGO_HOME there, then
#   rustup target add aarch64-unknown-linux-gnu)
bash tools/switch/build-nvk-msys2.sh     # NVK -> extern/nvk-switch/lib/libvulkan.a (about 15 min)
sh tools/switch/fetch-switch-deps.sh     # Dawn for Switch, and all the patches
bash tools/switch/build.sh               # configures and builds build-switch/partyboard.nro
bash tools/switch/package.sh             # dist/partyboard-switch.zip (SD card layout)
```

These steps build a fresh clone (checked on 7 November 2026), with `extern/nvk-switch` copied from
an existing build rather than built with `build-nvk-msys2.sh`.

Notes on the MSYS host:

- The compiler is a native `.exe` and cannot read MSYS paths inside files. That is why there are no
  precompiled headers or C++20 module scanning, and why archiving and linking go through
  `platforms/switch/rspwrap.sh`.
- Do not use `CMAKE_NINJA_FORCE_RESPONSE_FILE`.
- Dawn, Tint and SPIRV-Tools build at `-O1`. At `-O2`, devkitA64's GCC 16 generates SPIR-V that the
  validator rejects ("Invalid instruction OpExtension").

## Testing

Console: `nxlink` streams the log to the PC. Press Y in the title-mode Homebrew Menu, then run this
until it connects (it does not retry when given an address):

```sh
nxlink -a <console IP> -s -p partyboard/partyboard.nro build-switch/partyboard.nro
```

Emulators are of limited use. Ryujinx runs the OpenGL ES backend with a patched recompiler but cannot
run NVK, and it does not enforce non-executable memory, so a build that passes there can still crash
on a console. Eden does not reach the first frame with this stack.

A CISO's removed blocks read back as zeros, so the launcher's hash check would tend to fail on one.
It does not run on Switch, and the launcher reports "Disc detected." instead.
