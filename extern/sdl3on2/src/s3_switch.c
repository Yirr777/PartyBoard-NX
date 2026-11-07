// The layer owns the Switch window so SDL2 never takes the default NWindow.

#ifdef __SWITCH__

#include <switch.h>

#include "s3_internal.h"

#define S3_SWITCH_WINDOW_ID 1u
#define S3_SWITCH_DISPLAY_ID 1u

// Flags that describe the surface a caller asked for; the rest are the console's answer.
#define S3_SWITCH_CREATION_FLAGS (S3_WINDOW_HIGH_PIXEL_DENSITY | S3_WINDOW_VULKAN | S3_WINDOW_OPENGL)

typedef struct S3_SwitchWindow {
    S3_WindowFlags creation_flags;
    S3_PropertiesID props;
    int w;
    int h;
} S3_SwitchWindow;

static int s3_video_refcount;
static bool s3_window_exists;
static S3_SwitchWindow s3_window;
static AppletOperationMode s3_operation_mode;
static bool s3_focused = true;
static bool s3_quit_sent;
static S3_DisplayMode s3_display_mode;

static void S3_SwitchScreenSize(AppletOperationMode mode, int* w, int* h)
{
    // Docked output is 1080p, handheld is the 720p panel.
    if (mode == AppletOperationMode_Console) {
        *w = 1920;
        *h = 1080;
    } else {
        *w = 1280;
        *h = 720;
    }
}

static bool S3_UninitializedVideo(void)
{
    SDL_SetError("Video subsystem has not been initialized");
    return false;
}

static S3_SwitchWindow* S3_SwitchWindowFrom(S3_Window* window)
{
    if (!s3_window_exists || window != (S3_Window*)&s3_window) {
        SDL_SetError("Invalid window");
        return NULL;
    }

    return &s3_window;
}

static bool S3_ValidDisplay(S3_DisplayID display_id)
{
    if (s3_video_refcount == 0) {
        return S3_UninitializedVideo();
    }

    if (display_id != S3_SWITCH_DISPLAY_ID) {
        SDL_SetError("Invalid display");
        return false;
    }

    return true;
}

bool S3_SwitchInitVideo(void)
{
    if (s3_video_refcount++ == 0) {
        s3_operation_mode = appletGetOperationMode();
        s3_focused = appletGetFocusState() == AppletFocusState_InFocus;
    }

    return true;
}

void S3_SwitchQuitVideo(bool all)
{
    if (all) {
        s3_video_refcount = 0;
    } else if (s3_video_refcount > 0) {
        s3_video_refcount--;
    }

    if (s3_video_refcount == 0 && s3_window_exists) {
        S3_DestroyWindow((S3_Window*)&s3_window);
    }
}

bool S3_SwitchVideoInitialized(void)
{
    return s3_video_refcount > 0;
}

void S3_SwitchPumpApplet(void)
{
    AppletOperationMode mode;
    bool focused;

    if (!appletMainLoop()) {
        if (!s3_quit_sent) {
            SDL_Event quit;

            SDL_zero(quit);
            quit.type = SDL_QUIT;
            s3_quit_sent = SDL_PushEvent(&quit) == 1;
        }
        return;
    }

    if (!s3_window_exists) {
        return;
    }

    mode = appletGetOperationMode();
    if (mode != s3_operation_mode) {
        s3_operation_mode = mode;
        S3_SwitchScreenSize(mode, &s3_window.w, &s3_window.h);
        S3_PushWindowEvent(SDL_WINDOWEVENT_RESIZED, S3_SWITCH_WINDOW_ID, s3_window.w, s3_window.h);
        S3_PushWindowEvent(SDL_WINDOWEVENT_SIZE_CHANGED, S3_SWITCH_WINDOW_ID, s3_window.w, s3_window.h);
    }

    focused = appletGetFocusState() == AppletFocusState_InFocus;
    if (focused != s3_focused) {
        s3_focused = focused;
        S3_PushWindowEvent(focused ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST, S3_SWITCH_WINDOW_ID, 0, 0);
    }
}

S3_DisplayID* S3_GetDisplays(int* count)
{
    S3_DisplayID* displays;

    if (count != NULL) {
        *count = 0;
    }

    if (s3_video_refcount == 0) {
        S3_UninitializedVideo();
        return NULL;
    }

    displays = (S3_DisplayID*)SDL_malloc(sizeof(*displays) * 2);
    if (displays == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    displays[0] = S3_SWITCH_DISPLAY_ID;
    displays[1] = 0;

    if (count != NULL) {
        *count = 1;
    }

    return displays;
}

S3_DisplayID S3_GetPrimaryDisplay(void)
{
    if (s3_video_refcount == 0) {
        S3_UninitializedVideo();
        return 0;
    }

    return S3_SWITCH_DISPLAY_ID;
}

S3_DisplayID S3_GetDisplayForWindow(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL ? S3_SWITCH_DISPLAY_ID : 0;
}

float S3_GetDisplayContentScale(S3_DisplayID display_id)
{
    return S3_ValidDisplay(display_id) ? 1.0f : 0.0f;
}

bool S3_GetDisplayBounds(S3_DisplayID display_id, S3_Rect* rect)
{
    if (rect != NULL) {
        SDL_zerop(rect);
    }

    if (!S3_ValidDisplay(display_id)) {
        return false;
    }

    if (rect == NULL) {
        SDL_SetError("Parameter 'rect' is invalid");
        return false;
    }

    S3_SwitchScreenSize(appletGetOperationMode(), &rect->w, &rect->h);

    return true;
}

bool S3_GetDisplayUsableBounds(S3_DisplayID display_id, S3_Rect* rect)
{
    return S3_GetDisplayBounds(display_id, rect);
}

const S3_DisplayMode* S3_GetCurrentDisplayMode(S3_DisplayID display_id)
{
    if (!S3_ValidDisplay(display_id)) {
        return NULL;
    }

    SDL_zero(s3_display_mode);
    s3_display_mode.displayID = S3_SWITCH_DISPLAY_ID;
    s3_display_mode.format = S3_PIXELFORMAT_RGBA8888;
    S3_SwitchScreenSize(appletGetOperationMode(), &s3_display_mode.w, &s3_display_mode.h);
    s3_display_mode.pixel_density = 1.0f;
    s3_display_mode.refresh_rate = 60.0f;
    s3_display_mode.refresh_rate_numerator = 60;
    s3_display_mode.refresh_rate_denominator = 1;

    return &s3_display_mode;
}

const S3_DisplayMode* S3_GetDesktopDisplayMode(S3_DisplayID display_id)
{
    return S3_GetCurrentDisplayMode(display_id);
}

S3_DisplayOrientation S3_GetNaturalDisplayOrientation(S3_DisplayID display_id)
{
    return S3_ValidDisplay(display_id) ? S3_ORIENTATION_LANDSCAPE : S3_ORIENTATION_UNKNOWN;
}

S3_DisplayOrientation S3_GetCurrentDisplayOrientation(S3_DisplayID display_id)
{
    return S3_ValidDisplay(display_id) ? S3_ORIENTATION_LANDSCAPE : S3_ORIENTATION_UNKNOWN;
}

const char* S3_GetCurrentVideoDriver(void)
{
    return s3_video_refcount > 0 ? "switch" : NULL;
}

bool S3_DisableScreenSaver(void)
{
    return s3_video_refcount > 0 ? true : S3_UninitializedVideo();
}

bool S3_EnableScreenSaver(void)
{
    return s3_video_refcount > 0 ? true : S3_UninitializedVideo();
}

static S3_Window* S3_SwitchCreateWindow(S3_WindowFlags flags)
{
    if (s3_video_refcount == 0 && !S3_InitSubSystem(S3_INIT_VIDEO)) {
        return NULL;
    }

    if (s3_window_exists) {
        SDL_SetError("Switch only supports one window");
        return NULL;
    }

    SDL_zero(s3_window);
    s3_window.props = S3_CreateProperties();
    if (s3_window.props == 0) {
        return NULL;
    }

    s3_window.creation_flags = flags & S3_SWITCH_CREATION_FLAGS;
    s3_operation_mode = appletGetOperationMode();
    s3_focused = appletGetFocusState() == AppletFocusState_InFocus;
    S3_SwitchScreenSize(s3_operation_mode, &s3_window.w, &s3_window.h);
    s3_window_exists = true;

    return (S3_Window*)&s3_window;
}

S3_Window* S3_CreateWindow(const char* title, int w, int h, S3_WindowFlags flags)
{
    // The console decides the size; the title has nowhere to go.
    (void)title;
    (void)w;
    (void)h;

    return S3_SwitchCreateWindow(flags);
}

S3_Window* S3_CreateWindowWithProperties(S3_PropertiesID props)
{
    return S3_SwitchCreateWindow((S3_WindowFlags)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_FLAGS_NUMBER, 0));
}

void S3_DestroyWindow(S3_Window* window)
{
    if (!s3_window_exists || window != (S3_Window*)&s3_window) {
        return;
    }

    S3_DestroyProperties(s3_window.props);
    SDL_zero(s3_window);
    s3_window_exists = false;
}

S3_WindowID S3_GetWindowID(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL ? S3_SWITCH_WINDOW_ID : 0;
}

S3_Window* S3_GetWindowFromID(S3_WindowID id)
{
    if (!s3_window_exists || id != S3_SWITCH_WINDOW_ID) {
        SDL_SetError("Invalid window ID");
        return NULL;
    }

    return (S3_Window*)&s3_window;
}

S3_PropertiesID S3_GetWindowProperties(S3_Window* window)
{
    const S3_SwitchWindow* state = S3_SwitchWindowFrom(window);

    // Dawn takes the default NWindow directly, so no native handle is published here.
    return state != NULL ? state->props : 0;
}

S3_WindowFlags S3_GetWindowFlags(S3_Window* window)
{
    const S3_SwitchWindow* state = S3_SwitchWindowFrom(window);
    S3_WindowFlags flags;

    if (state == NULL) {
        return 0;
    }

    flags = S3_WINDOW_FULLSCREEN | state->creation_flags;

    if (s3_focused) {
        flags |= S3_WINDOW_INPUT_FOCUS | S3_WINDOW_MOUSE_FOCUS;
    }

    return flags;
}

bool S3_GetWindowSize(S3_Window* window, int* w, int* h)
{
    const S3_SwitchWindow* state = S3_SwitchWindowFrom(window);

    if (state == NULL) {
        return false;
    }

    if (w != NULL) {
        *w = state->w;
    }

    if (h != NULL) {
        *h = state->h;
    }

    return true;
}

bool S3_GetWindowSizeInPixels(S3_Window* window, int* w, int* h)
{
    return S3_GetWindowSize(window, w, h);
}

float S3_GetWindowPixelDensity(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL ? 1.0f : 0.0f;
}

float S3_GetWindowDisplayScale(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL ? 1.0f : 0.0f;
}

bool S3_SetWindowTitle(S3_Window* window, const char* title)
{
    (void)title;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_SetWindowIcon(S3_Window* window, S3_Surface* icon)
{
    (void)icon;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_SetWindowPosition(S3_Window* window, int x, int y)
{
    (void)x;
    (void)y;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_SetWindowSize(S3_Window* window, int w, int h)
{
    (void)w;
    (void)h;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_SetWindowMinimumSize(S3_Window* window, int min_w, int min_h)
{
    (void)min_w;
    (void)min_h;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_SetWindowFullscreen(S3_Window* window, bool fullscreen)
{
    (void)fullscreen;

    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_ShowWindow(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_HideWindow(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_RaiseWindow(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL;
}

bool S3_RestoreWindow(S3_Window* window)
{
    return S3_SwitchWindowFrom(window) != NULL;
}

#else

// Keeps the translation unit non-empty off Switch.
typedef int s3_switch_unused;

#endif
