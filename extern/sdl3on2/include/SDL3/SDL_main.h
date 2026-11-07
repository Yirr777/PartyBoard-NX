#ifndef SDL3ON2_SDL_MAIN_H_
#define SDL3ON2_SDL_MAIN_H_

#ifdef S3_IMPLEMENTATION
#error "sdl3on2 implementation files must include \"s3_internal.h\", never <SDL3/...>."
#endif

#include <SDL3/SDL.h>

// Windows links SDL2main and UWP takes WinMain from the layer; on Switch libnx supplies it.
#if defined(_WIN32) && !defined(__SWITCH__)
#ifdef __cplusplus
extern "C" {
#endif

extern int SDL_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#define main SDL_main
#endif

#endif
