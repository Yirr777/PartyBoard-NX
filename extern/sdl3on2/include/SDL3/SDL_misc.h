#ifndef SDL3ON2_SDL_MISC_H_
#define SDL3ON2_SDL_MISC_H_
// mp4-switch addition: no browser to open URLs with on Horizon.
#include <SDL3/s3_defs.h>

static inline bool SDL_OpenURL(const char *url)
{
    (void)url;
    return false;
}

#endif
