#include "s3_internal.h"

#ifndef __SWITCH__

// SDL_syswm.h brings in windows.h and Xlib.h, so it has a file of its own.
#if defined(__has_include)
#if __has_include(<SDL2/SDL_syswm.h>)
#include <SDL2/SDL_syswm.h>
#else
#include <SDL_syswm.h>
#endif
#else
#include <SDL2/SDL_syswm.h>
#endif

void S3_PublishNativeWindowProperties(SDL_Window* window, S3_PropertiesID props)
{
    SDL_SysWMinfo info;

    (void)props;

    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) {
        return;
    }

    switch (info.subsystem) {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    case SDL_SYSWM_WINDOWS:
        S3_SetPointerProperty(props, S3_PROP_WINDOW_WIN32_HWND_POINTER, info.info.win.window);
        S3_SetPointerProperty(props, S3_PROP_WINDOW_WIN32_INSTANCE_POINTER, info.info.win.hinstance);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_WINRT)
    case SDL_SYSWM_WINRT:
        S3_SetPointerProperty(props, S3_PROP_WINDOW_WINRT_WINDOW_POINTER, info.info.winrt.window);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
    case SDL_SYSWM_X11:
        S3_SetPointerProperty(props, S3_PROP_WINDOW_X11_DISPLAY_POINTER, info.info.x11.display);
        S3_SetNumberProperty(props, S3_PROP_WINDOW_X11_WINDOW_NUMBER, (Sint64)info.info.x11.window);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
    case SDL_SYSWM_WAYLAND:
        S3_SetPointerProperty(props, S3_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, info.info.wl.display);
        S3_SetPointerProperty(props, S3_PROP_WINDOW_WAYLAND_SURFACE_POINTER, info.info.wl.surface);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_ANDROID)
    case SDL_SYSWM_ANDROID:
        S3_SetPointerProperty(props, S3_PROP_WINDOW_ANDROID_WINDOW_POINTER, info.info.android.window);
        break;
#endif
    default:
        break;
    }
}

#endif
