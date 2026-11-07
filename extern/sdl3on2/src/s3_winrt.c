#include "s3_internal.h"

#ifdef __WINRT__

#include <windows.h>

// SDL2 has no SDL2main for UWP, and its sample WinMain is C++/CX, which only MSVC compiles.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show;

    // SDL3 has no back navigation, so SDL2 marks every back request handled.
    SDL_SetHintWithPriority(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1", SDL_HINT_DEFAULT);

    return SDL_WinRTRunApp(SDL_main, NULL);
}

#endif
