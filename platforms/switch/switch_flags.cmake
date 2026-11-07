# Compile flags for the game code on Switch (devkitA64 GCC), and the static REL build.

# The decompiled code is tested on x86 (signed char) with MSVC /Od and clang -O2. GCC needs the
# usual relaxations for C that predates C99 rules, and must not optimise on strict-aliasing or
# signed-overflow assumptions the original compiler did not make.
function(mp4_switch_game_flags target)
    target_compile_options(${target} PRIVATE
        # Party Board builds the decompiled code unoptimised (MSVC /Od): it relies on behaviour
        # optimisers do not preserve.
        ${MP4_SWITCH_GAME_OPT}
        -fsigned-char
        -fno-strict-aliasing
        -fwrapv
        -fno-delete-null-pointer-checks
        -ffp-contract=off
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=incompatible-pointer-types>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=int-conversion>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-function-declaration>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=implicit-int>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=return-mismatch>
        $<$<COMPILE_LANGUAGE:C>:-Wno-error=declaration-missing-parameter-type>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-changes-meaning>
        -w)
endfunction()

# -O0 does not link: C99 inline functions only get an external definition when optimising.
set(MP4_SWITCH_GAME_OPT "-O1" CACHE STRING "Optimisation level for the game (DOL and RELs)")

set(MP4_SWITCH_RELS "" CACHE INTERNAL "")

# A REL becomes one relocatable object: its own symbols hidden but ObjectSetup, which is renamed
# mp4rel_<name>_ObjectSetup, and its writable data moved to the section mp4ovl_<name>, so a later
# link of the same REL can restore its globals (see platforms/switch/rel_registry.h).
function(mp4_switch_add_rel name)
    set(sources ${ARGN})
    add_library(${name}_objs OBJECT ${sources})
    target_compile_definitions(${name}_objs PRIVATE
        TARGET_PC OPTIMIZED_TEXTURE_LOADING BYTESWAPPING NON_MATCHING VERSION=${VERSION})
    target_include_directories(${name}_objs PRIVATE
        include extern extern/aurora/include extern/aurora/include/dolphin)
    target_link_libraries(${name}_objs PRIVATE musyx aurora::mtx aurora::gx)
    mp4_switch_game_flags(${name}_objs)
    # Zero-initialised and uninitialised globals go in .data too, so one section holds all state.
    target_compile_options(${name}_objs PRIVATE -fno-common -fno-zero-initialized-in-bss
        -fno-data-sections)

    set(out "${CMAKE_BINARY_DIR}/rel/${name}.o")
    set(tool "${CMAKE_SOURCE_DIR}/platforms/switch/rel_isolate.sh")
    add_custom_command(
        OUTPUT "${out}"
        COMMAND sh "${tool}" "${name}" "${out}" "${MP4_SWITCH_BINUTILS_PREFIX}" "$<TARGET_OBJECTS:${name}_objs>"
        DEPENDS ${name}_objs "${tool}" $<TARGET_OBJECTS:${name}_objs>
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "Isolating REL ${name}")
    set(MP4_SWITCH_RELS ${MP4_SWITCH_RELS} ${name} CACHE INTERNAL "")
endfunction()

get_filename_component(_mp4_cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
set(MP4_SWITCH_BINUTILS_PREFIX "${_mp4_cc_dir}/aarch64-none-elf-")
