# Port notes

Problems found while bringing Party Board to the Switch, for anyone working on this port or a similar
one. See [switch.md](switch.md) for how to build.

## Crashes and bugs

**Crash on the first coroutine switch (console only).** libco keeps its swap routine in an array
marked for the text section with a `.text#` trick, where `#` comments out the flags GCC appends. On
AArch64 the comment character is `//`, so the array landed in a read-only, non-executable section.
Ryujinx does not enforce this, so only a console showed it. Fixed in `patches/libco-switch.patch`.

**Crash at the item roulette on board 1.** `RoundItemInit` makes four texture animations from one file.
`HuSprAnimRead` byteswaps raw data in place, so the second read swapped it back and followed garbage
offsets. `sprman.c` now remembers the converted copy of each raw file. This is a Party Board bug and
may affect other platforms. The table reuses entries whose copy went with a freed overlay heap,
since an earlier 64-entry version filled up and the crash came back.

**Crash in `WrappedEGLSync::Create`.** The OpenGL ES path checked `egl.CreateSync == nullptr`, but that
member is only loaded on EGL 1.5 and is uninitialized on 1.4. Dawn's `IsAtLeastVersion` also has its
comparison reversed, and says yes on 1.4, so the patch compares the minor version directly.

**Scratch ticket not updating.** Sprite textures keep their `GXTexObj` across frames and `DCFlushRange`
does nothing on PC, so the scratched pixels never reached the GPU. `GXInitTexObjData` makes Aurora
upload them again.

**Black screen on OpenGL ES.** Horizon's Mesa EGL reports a 0x0 surface, so the final blit drew nothing.
The swapchain now asks the native window for its size.

**Corrupted memory card.** Aurora saves through a temporary file and a rename, and on Horizon `rename()`
does not replace an existing file. `sdl3on2` now removes the target and tries again.

**Game logic relying on the original compiler.** m449 indexes `(&lbl_1_bss_54)[i]` assuming two globals
are contiguous, `mstory3Dll/result.c` lacked a return value, and `hsfload.c` overflowed `rgba[100]` on a
model with 206 materials. These fixes are ported from the Xbox port and have not been exercised
specifically on Switch.

**Open: crash in `_malloc_r`.** Seen once, in `threadCreate` just after `Loading DVD image`, on a normal
launch. It suggests earlier heap corruption and has not been reproduced.

**The Switch's own controller screen at startup.** Opening it before the port has configured the
controller styles it accepts shows every controller struck out, and nothing can continue. It is
only offered from the settings menu, once the game runs.

**Holding a button at startup.** Reading L or ZL with libnx's `padUpdate` right after the pads layer
starts came back empty in two console tests, even polling for 250 ms, so that shortcut was removed.
The launcher menu is a setting ("Skip Party Board Main Menu", stored as `backend.switchSkipMainMenu` because
earlier builds had saved the PC key without showing it).

**Settings that do not apply on a console** are hidden: fullscreen and window size, focus and
background input, VSync (the console always waits for the display, and changing it recreates the
swapchain), the keyboard turbo key and keyboard device, and achievements (Party Board defines none).

## Performance

**Long pauses when saving.** Each card write copies the whole image to a temporary file. With a 16 MiB
image, returning to the board took about 8 s. A 512 KiB image brings that to about 0.6 s and keeps the
atomic write.

**Audio thread.** A thread started with libnx's default core shares its creator's core, here the game
thread's, so the mixer is pinned to core 2 at a slightly higher priority.

**Measured on one console with NVK, with sound:**

| Scene | Mode | fps | CPU work per frame |
|---|---|---|---|
| Board | Handheld and docked | About 60 | 7-9 ms |
| Minigames | Handheld and docked | About 58 or better | 6-10 ms |
| Intro movie | Handheld | About 50 | About 14 ms |

Enable the log and each 300 frames gives a `[nx] perf` line with mode, fps, frame work, heap use and
audio underruns. Memory use keeps climbing during a session (about 1.4 GiB after 20 minutes, with
1.8 GiB free). A full party is needed to tell whether it levels off.

## Widescreen (16:9)

`include/port/widescreen.h` is the C version of Dolphin's "$16:9 Widescreen" Gecko code for
GMPE01 (`Data/Sys/GameSettings/GMPE01.ini`), the approach SwitchWaker took for Wind Waker. It is
anamorphic: the game still draws its 640x480 frame and Aurora stretches it to the screen
(`AURORA_VIEWPORT_STRETCH`), so the game draws everything 3/4 as wide.

| Gecko code line | Function | Here |
|---|---|---|
| `C_MTXPerspective`, `C_MTXLightPerspective`: aspect x1.336 | All 3D and light projections | `__MTXAspectScale` (Aurora patch), x4/3 |
| 576.0 -> 768.0 and `GXSetProjection` sets m[0][3] = -0.75 for that ortho | `HuSprDispInit`, `MesDispFunc` | 2D space -96..672 (`sprput.c`, `window.c`) |
| `HuWinCreate` keeps 576.0 | Window centring | unchanged |
| 640.0 -> 853.33, `cmpwi 640` -> 854, `print8` x*4/3 | `pfDrawFonts` | `printfunc.c` |
| `objMesh`: `ObjCullCheck` -> 1; `BoardCameraCullCheck` -> `blr` | Culling off | Culling widened instead (`hsfdraw.c`, `board/main.c`) |
| x*1.336-96 | `UpdateBoardStatus`, `BoardStatusItemSet`, `ExecItemPick`, `ItemGetPos`, `SetItemUIStatus` | `HuWideX` in `board/ui.c` |
| `m432Dll` +0x10088: x*1.336-96 | Its own 3D-to-2D | `HuWideX` in `REL/m432Dll/main.c` |

`Hu3D3Dto2D` needs no change: it uses the 4:3 aspect and the 576 space, which line up with the
widened 3D and 2D. Additions not in the code: sprites and movies as wide as the 576 screen are
stretched to the full width (`HuWideBackdropMtx`), so menus and movie transitions have no side bars;
in file select the chosen memory card flies further, past the 16:9 edge.

## Bundled shader cache

The package carries two databases from a console, beside the `.nro`:

- `initial_pipeline_cache.db`: Aurora's list of pipelines (`cache/pipeline_cache.db`). Aurora merges
  it into the player's list at start and builds those pipelines ahead of use.
- `initial_dawn_cache.db`: Dawn's compiled shaders (`cache/dawn_cache.db`). Every Switch has the
  same GPU and driver, so they are valid for the same build. `portmain.cpp` copies it once, when the
  player has none (`std::filesystem::copy_file` is not implemented by devkitA64's libc, so it copies
  with stdio).

To refresh them, play the scenes to cover with a new build, then copy `cache/pipeline_cache.db` and
`cache/dawn_cache.db` from the console (with the game closed) to `platforms/switch/shader-cache/`
under the `initial_` names. A cache made by another build still loads; entries that no longer match
are compiled again. The pauses that remain when a scene loads (about 0.3-0.6 s) are loading, not
shaders: they are the same with a full cache.

## Emulator-only workaround

`platforms/switch/bo_pool.c` works around Ryujinx writing into driver buffers after they are freed,
which corrupted the heap. It reserves 384 MiB and fills freed blocks, so it turns itself off under
Atmosphère.

## Compiler issues (devkitA64 GCC 16)

- Dawn, Tint and SPIRV-Tools build at `-O1 -fno-strict-aliasing`, because `-O2` generates invalid SPIR-V.
- The game builds at `-O1` (`MP4_SWITCH_GAME_OPT`, the default), because C99 `inline` functions do
  not link at `-O0`.
- `patches/abseil-switch.patch` works around a constant-expression error and missing platform features.
- libgcc's unwinder uses `CHKFEAT`/`GCSPR` instructions that Ryujinx rejects, so
  `platforms/switch/patch_chkfeat.py` replaces them with `NOP` in the linked ELF.
