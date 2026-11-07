cmake_minimum_required(VERSION 3.21)
project(sdl3on2 C)

file(GLOB SDL3ON2_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SDL3_mixer/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
file(GLOB SDL3ON2_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(sdl3on2 STATIC
    ${SDL3ON2_HEADERS}
    ${SDL3ON2_SOURCES}
)

# static_assert at file scope needs C11.
set_target_properties(sdl3on2 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

target_include_directories(sdl3on2 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(DEFINED DEVKITPRO)
    set(SDL3ON2_DEVKITPRO "${DEVKITPRO}")
elseif(DEFINED ENV{DEVKITPRO})
    set(SDL3ON2_DEVKITPRO "$ENV{DEVKITPRO}")
endif()
target_include_directories(sdl3on2 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    $<$<BOOL:${NX}>:${SDL3ON2_DEVKITPRO}/portlibs/switch/include>
)

if(TARGET SDL2::SDL2-static)
    set(SDL3ON2_SDL2_LIBRARY SDL2::SDL2-static)
elseif(TARGET SDL2::SDL2)
    set(SDL3ON2_SDL2_LIBRARY SDL2::SDL2)
else()
    set(SDL3ON2_SDL2_LIBRARY ${SDL2_LIBRARIES})
endif()

target_link_libraries(sdl3on2 PUBLIC
    ${SDL3ON2_SDL2_LIBRARY}
)

# SDL_main.h renames main only on Windows, where SDL2main supplies WinMain, or the layer on UWP.
if(TARGET SDL2::SDL2main AND WIN32 AND NOT WINDOWS_STORE)
    target_link_libraries(sdl3on2 PUBLIC SDL2::SDL2main)
endif()

# The layer's headers find UWP from WINAPI_FAMILY, so every target that includes them needs it.
if(WINDOWS_STORE)
    target_compile_definitions(sdl3on2 PUBLIC WINAPI_FAMILY=WINAPI_FAMILY_APP)
endif()

if(MSVC)
    target_compile_options(sdl3on2 PRIVATE /W4)
else()
    target_compile_options(sdl3on2 PRIVATE -Wall -Wextra)
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(SDL3ON2_LIBRARY "sdl3on2" PARENT_SCOPE)
    set(SDL3ON2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
endif()
