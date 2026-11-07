#ifndef SDL3ON2_SDL_DIALOG_H_
#define SDL3ON2_SDL_DIALOG_H_
// mp4-switch addition: Horizon has no file dialogs. The callback runs at once with a failure, as
// SDL3 reports a platform without dialog support. Party Board finds the disc image by scanning
// the .nro's directory instead.
#include <SDL3/SDL_video.h>
#include <stddef.h>

typedef struct SDL_DialogFileFilter {
    const char *name;
    const char *pattern;
} SDL_DialogFileFilter;

typedef void (*SDL_DialogFileCallback)(void *userdata, const char *const *filelist, int filter);

static inline void SDL_ShowOpenFileDialog(SDL_DialogFileCallback callback, void *userdata, SDL_Window *window,
                                          const SDL_DialogFileFilter *filters, int nfilters,
                                          const char *default_location, bool allow_many)
{
    (void)window;
    (void)filters;
    (void)nfilters;
    (void)default_location;
    (void)allow_many;
    if (callback != NULL) {
        callback(userdata, NULL, -1);
    }
}

#endif
