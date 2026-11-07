# Nintendo Switch (Horizon, libnx): dependencies set up before Aurora is added.
# Recipe after new-coke/strikers (CC0): Aurora + Dawn (dawn-switch) on NVK (mesa-switch), with SDL3
# provided by sdl3on2 over devkitPro's SDL2.

set(MP4_SWITCH_DIR "${CMAKE_SOURCE_DIR}/platforms/switch")
set(MP4_SWITCH_DAWN_DIR "${CMAKE_SOURCE_DIR}/extern/dawn-switch" CACHE PATH
    "Dawn source tree with the Switch changes, from tools/switch/fetch-switch-deps.sh")
set(MP4_SWITCH_NVK_DIR "${CMAKE_SOURCE_DIR}/extern/nvk-switch" CACHE PATH
    "NVK for Switch: lib/libvulkan.a and include/vulkan, from tools/switch/build-nvk-msys2.sh")
foreach(_nx_dep "${MP4_SWITCH_DAWN_DIR}/CMakeLists.txt" "${MP4_SWITCH_NVK_DIR}/lib/libvulkan.a")
    if(NOT EXISTS "${_nx_dep}")
        message(FATAL_ERROR "${_nx_dep} is missing; see docs/switch.md.")
    endif()
endforeach()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Archive and link through a response file: the MSYS host would otherwise hit Windows' command
# length limit starting the native devkitA64 tools (Dawn's archive, the final link).
if (CMAKE_HOST_SYSTEM_NAME MATCHES "MSYS|MINGW|Windows")
    set(_rspwrap "${CMAKE_SOURCE_DIR}/platforms/switch/rspwrap.sh")
    foreach (_lang C CXX)
        set(CMAKE_${_lang}_ARCHIVE_CREATE "sh ${_rspwrap} <CMAKE_AR> qc <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_${_lang}_ARCHIVE_APPEND "sh ${_rspwrap} <CMAKE_AR> q <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_${_lang}_LINKER_LAUNCHER sh "${_rspwrap}")
    endforeach ()
endif ()

# The pipeline cache is SQLite, through a VFS written for Horizon (no mmap, WAL or temp files).
set(AURORA_CACHE_USE_ZSTD ON CACHE BOOL "" FORCE)

# devkitPro ships SDL2 only; sdl3on2 exposes it as SDL3. In SDL2's link order.
set(SDL2_LIBRARIES
    "${DEVKITPRO}/portlibs/switch/lib/libSDL2.a"
    "${DEVKITPRO}/portlibs/switch/lib/libEGL.a"
    "${DEVKITPRO}/portlibs/switch/lib/libglapi.a"
    "${DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a"
    "${DEVKITPRO}/portlibs/switch/lib/libGLESv2.a"
    nx stdc++ m)
add_subdirectory(extern/sdl3on2 EXCLUDE_FROM_ALL)
add_library(SDL3::SDL3-static ALIAS sdl3on2)
add_library(SDL3::SDL3 ALIAS sdl3on2)
set(SDL3_FOUND TRUE)
set(AURORA_SDL3_PROVIDER system CACHE STRING "" FORCE)
set(AURORA_SDL3_LINKAGE static CACHE STRING "" FORCE)

# Aurora's vendored Dawn build, pointed at the patched tree.
set(AURORA_DAWN_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_DAWN_LINKAGE static CACHE STRING "" FORCE)
set(FETCHCONTENT_SOURCE_DIR_DAWN "${MP4_SWITCH_DAWN_DIR}" CACHE PATH "" FORCE)
set(DAWN_PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
foreach(_nx_off DAWN_ENABLE_D3D11 DAWN_ENABLE_D3D12 DAWN_ENABLE_METAL DAWN_ENABLE_DESKTOP_GL
                DAWN_USE_WAYLAND DAWN_USE_X11 DAWN_BUILD_TESTS DAWN_BUILD_SAMPLES
                TINT_BUILD_TESTS TINT_BUILD_CMD_TOOLS DAWN_BUILD_PROTOBUF)
    set(${_nx_off} OFF CACHE BOOL "" FORCE)
endforeach()
# No C++20 module scanning: its rules use response files, which the native compiler cannot read
# MSYS paths from.
set(DAWN_SUPPORTS_CXX_MODULES FALSE CACHE BOOL "" FORCE)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(DAWN_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
# OpenGL ES through devkitPro's Mesa (nouveau nvc0) as a fallback backend, statically linked;
# emulators run it where they cannot run NVK.
set(DAWN_ENABLE_OPENGLES ON CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_WRITER ON CACHE BOOL "" FORCE)
option(MP4_SWITCH_DAWN_ASSERTS "Dawn asserts and per-call GL error checks (diagnostics)" OFF)
set(DAWN_ALWAYS_ASSERT ${MP4_SWITCH_DAWN_ASSERTS} CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_VALIDATOR OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL ON CACHE BOOL "" FORCE)
set(DAWN_ENABLE_SPIRV_VALIDATION ON CACHE BOOL "" FORCE)

# nod (Rust) does not build for Horizon; nodlite implements the subset of its C API Aurora uses.
add_library(nodlite STATIC "${MP4_SWITCH_DIR}/nodlite/nodlite.c")
target_include_directories(nodlite PUBLIC "${MP4_SWITCH_DIR}/nodlite/include")
target_include_directories(nodlite PRIVATE "${DEVKITPRO}/portlibs/switch/include")
target_link_libraries(nodlite PRIVATE "${DEVKITPRO}/portlibs/switch/lib/libz.a")
add_library(nod::nod ALIAS nodlite)

# Tracy's client does not build for Horizon.
add_library(TracyClient INTERFACE)
target_include_directories(TracyClient INTERFACE "${MP4_SWITCH_DIR}/tracy")
add_library(Tracy::TracyClient ALIAS TracyClient)

# RmlUi, as Aurora pins it, with its install rules made to configure on an MSYS host. Declared
# before Aurora, so this declaration is the one FetchContent uses.
include(FetchContent)
FetchContent_Declare(rmlui
    URL https://github.com/encounter/RmlUi/archive/f00a0fee38391b2f927114e11bea18dc0a7dba1e.tar.gz
    URL_HASH SHA256=6d9fd1164b7873c9dfba72d724753bda9e3db8280c9af802eed8e2a12035f152
    PATCH_COMMAND ${CMAKE_COMMAND} -P "${MP4_SWITCH_DIR}/patches/patch_rmlui.cmake"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL
)

# Freetype and zstd from devkitPro's portlibs.
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
