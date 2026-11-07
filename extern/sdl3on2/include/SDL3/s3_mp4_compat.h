#ifndef SDL3ON2_S3_MP4_COMPAT_H_
#define SDL3ON2_S3_MP4_COMPAT_H_
// mp4-switch additions: the SDL3 names RmlUi's SDL platform backend and Party Board's UI use that
// sdl3on2 does not map. The Switch has no mouse cursor, clipboard or IME, so these are no-ops.

#ifndef SDL_MAJOR_VERSION
#define SDL_MAJOR_VERSION 3
#define SDL_MINOR_VERSION 2
#define SDL_MICRO_VERSION 0
#endif

typedef Uint64 SDL_FingerID;
typedef Uint64 SDL_TouchID;

typedef struct S3_Cursor S3_Cursor;
#define SDL_Cursor S3_Cursor

typedef enum SDL_SystemCursor {
    SDL_SYSTEM_CURSOR_DEFAULT,
    SDL_SYSTEM_CURSOR_TEXT,
    SDL_SYSTEM_CURSOR_WAIT,
    SDL_SYSTEM_CURSOR_CROSSHAIR,
    SDL_SYSTEM_CURSOR_PROGRESS,
    SDL_SYSTEM_CURSOR_NWSE_RESIZE,
    SDL_SYSTEM_CURSOR_NESW_RESIZE,
    SDL_SYSTEM_CURSOR_EW_RESIZE,
    SDL_SYSTEM_CURSOR_NS_RESIZE,
    SDL_SYSTEM_CURSOR_MOVE,
    SDL_SYSTEM_CURSOR_NOT_ALLOWED,
    SDL_SYSTEM_CURSOR_POINTER,
} SDL_SystemCursor;

static inline SDL_Cursor *SDL_CreateSystemCursor(SDL_SystemCursor id)
{
    (void)id;
    return (SDL_Cursor *)0;
}

static inline void SDL_DestroyCursor(SDL_Cursor *cursor)
{
    (void)cursor;
}

static inline bool SDL_SetCursor(SDL_Cursor *cursor)
{
    (void)cursor;
    return true;
}

static inline bool SDL_CaptureMouse(bool enabled)
{
    (void)enabled;
    return true;
}

static inline bool SDL_SetClipboardText(const char *text)
{
    (void)text;
    return false;
}

static inline char *SDL_GetClipboardText(void)
{
    char *s = (char *)SDL_malloc(1);
    if (s) {
        s[0] = '\0';
    }
    return s;
}

#ifdef __cplusplus
extern "C" {
#endif
extern S3_Surface *S3_LoadPNG_IO(S3_IOStream *src, bool closeio);
#ifdef __cplusplus
}
#endif
#define SDL_LoadPNG_IO S3_LoadPNG_IO

typedef enum SDL_JoystickConnectionState {
    SDL_JOYSTICK_CONNECTION_INVALID = -1,
    SDL_JOYSTICK_CONNECTION_UNKNOWN,
    SDL_JOYSTICK_CONNECTION_WIRED,
    SDL_JOYSTICK_CONNECTION_WIRELESS,
} SDL_JoystickConnectionState;

static inline SDL_JoystickConnectionState SDL_GetGamepadConnectionState(SDL_Gamepad *gamepad)
{
    (void)gamepad;
    return SDL_JOYSTICK_CONNECTION_WIRELESS;
}

typedef enum SDL_TextInputType {
    SDL_TEXTINPUT_TYPE_TEXT,
    SDL_TEXTINPUT_TYPE_TEXT_NAME,
    SDL_TEXTINPUT_TYPE_TEXT_EMAIL,
    SDL_TEXTINPUT_TYPE_TEXT_USERNAME,
    SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN,
    SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE,
    SDL_TEXTINPUT_TYPE_NUMBER,
    SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN,
    SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE,
} SDL_TextInputType;

#define SDL_PROP_TEXTINPUT_TYPE_NUMBER "SDL.textinput.type"

static inline bool SDL_StartTextInputWithProperties(SDL_Window *window, SDL_PropertiesID props)
{
    (void)props;
    return SDL_StartTextInput(window);
}

static inline bool SDL_GamepadConnected(SDL_Gamepad *gamepad)
{
    return gamepad != (SDL_Gamepad *)0;
}

// The console's screen has no notches or overscan: the safe area is the whole window.
static inline bool SDL_GetWindowSafeArea(SDL_Window *window, SDL_Rect *rect)
{
    int w = 0;
    int h = 0;
    if (rect == (SDL_Rect *)0) {
        return false;
    }
    SDL_GetWindowSize(window, &w, &h);
    rect->x = 0;
    rect->y = 0;
    rect->w = w;
    rect->h = h;
    return true;
}

static inline bool SDL_TextInputActive(SDL_Window *window)
{
    (void)window;
    return false;
}

static inline bool SDL_SetTextInputArea(SDL_Window *window, const SDL_Rect *rect, int cursor)
{
    (void)window;
    (void)rect;
    (void)cursor;
    return true;
}

#endif
