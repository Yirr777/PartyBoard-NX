#include <assert.h>

#include "s3_internal.h"

// SDL2's Uint8 key state holds only 0 or 1, a valid object representation for a one-byte bool.
static_assert(sizeof(bool) == sizeof(Uint8), "bool is one byte");
static_assert(S3_EVENT_USER == SDL_USEREVENT, "SDL_USEREVENT");
static_assert(S3_EVENT_LAST == SDL_LASTEVENT, "SDL_LASTEVENT");

// SDL2 renumbered nothing below this range; SDL3 moved the keys SDL2 put here.
#define S3_SCANCODE_RENUMBERED_FIRST 258
#define S3_SCANCODE_RENUMBERED_LAST 286

// SDL3 text events point at memory that lasts the pump cycle; SDL2 copies into the event.
#define S3_TEXT_ARENA_SIZE 32
#define S3_TEXT_ARENA_ENTRY_SIZE 64

static char s3_text_arena[S3_TEXT_ARENA_SIZE][S3_TEXT_ARENA_ENTRY_SIZE];
static int s3_text_arena_index;

static void S3_ResetTextArena(void)
{
    s3_text_arena_index = 0;
}

static Uint32 S3_FromSDL2WindowEvent(Uint8 window_event)
{
    switch (window_event) {
    case SDL_WINDOWEVENT_SHOWN:
        return S3_EVENT_WINDOW_SHOWN;
    case SDL_WINDOWEVENT_HIDDEN:
        return S3_EVENT_WINDOW_HIDDEN;
    case SDL_WINDOWEVENT_EXPOSED:
        return S3_EVENT_WINDOW_EXPOSED;
    case SDL_WINDOWEVENT_MOVED:
        return S3_EVENT_WINDOW_MOVED;
    case SDL_WINDOWEVENT_RESIZED:
        return S3_EVENT_WINDOW_RESIZED;
    case SDL_WINDOWEVENT_SIZE_CHANGED:
        // SDL2's SIZE_CHANGED covers the backing-store change SDL3 names PIXEL_SIZE_CHANGED.
        return S3_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    case SDL_WINDOWEVENT_MINIMIZED:
        return S3_EVENT_WINDOW_MINIMIZED;
    case SDL_WINDOWEVENT_MAXIMIZED:
        return S3_EVENT_WINDOW_MAXIMIZED;
    case SDL_WINDOWEVENT_RESTORED:
        return S3_EVENT_WINDOW_RESTORED;
    case SDL_WINDOWEVENT_ENTER:
        return S3_EVENT_WINDOW_MOUSE_ENTER;
    case SDL_WINDOWEVENT_LEAVE:
        return S3_EVENT_WINDOW_MOUSE_LEAVE;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        return S3_EVENT_WINDOW_FOCUS_GAINED;
    case SDL_WINDOWEVENT_FOCUS_LOST:
        return S3_EVENT_WINDOW_FOCUS_LOST;
    case SDL_WINDOWEVENT_CLOSE:
        return S3_EVENT_WINDOW_CLOSE_REQUESTED;
    case SDL_WINDOWEVENT_DISPLAY_CHANGED:
        return S3_EVENT_WINDOW_DISPLAY_CHANGED;
    default:
        return S3_EVENT_FIRST;
    }
}

static Uint8 S3_ToSDL2WindowEvent(Uint32 type)
{
    Uint8 window_event;

    for (window_event = SDL_WINDOWEVENT_SHOWN; window_event <= SDL_WINDOWEVENT_DISPLAY_CHANGED; window_event++) {
        if (S3_FromSDL2WindowEvent(window_event) == type) {
            return window_event;
        }
    }

    return SDL_WINDOWEVENT_NONE;
}

bool S3_TranslateEvent(const SDL_Event* native, S3_Event* event, char* text, size_t text_size)
{
    SDL_memset(event, 0, sizeof(*event));

    // SDL2 timestamps are milliseconds, SDL3's are nanoseconds.
    event->common.timestamp = (Uint64)native->common.timestamp * UINT64_C(1000000);

    switch (native->type) {
    case SDL_QUIT:
        event->type = S3_EVENT_QUIT;
        break;

    case SDL_APP_WILLENTERBACKGROUND:
        event->type = S3_EVENT_WILL_ENTER_BACKGROUND;
        break;

    case SDL_APP_DIDENTERBACKGROUND:
        event->type = S3_EVENT_DID_ENTER_BACKGROUND;
        break;

    case SDL_APP_WILLENTERFOREGROUND:
        event->type = S3_EVENT_WILL_ENTER_FOREGROUND;
        break;

    case SDL_APP_DIDENTERFOREGROUND:
        event->type = S3_EVENT_DID_ENTER_FOREGROUND;
        break;

    case SDL_WINDOWEVENT:
        // SDL3 made the sub-events top-level types; an unmapped one stays a zeroed type-0 event.
        event->type = S3_FromSDL2WindowEvent(native->window.event);
        if (event->type != S3_EVENT_FIRST) {
            event->window.windowID = native->window.windowID;
            event->window.data1 = native->window.data1;
            event->window.data2 = native->window.data2;
        }
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
        event->type = native->type == SDL_KEYDOWN ? S3_EVENT_KEY_DOWN : S3_EVENT_KEY_UP;
        event->key.windowID = native->key.windowID;
        event->key.scancode = (S3_Scancode)native->key.keysym.scancode;
        event->key.key = (S3_Keycode)native->key.keysym.sym;
        event->key.mod = (S3_Keymod)native->key.keysym.mod;
        event->key.down = native->key.state == SDL_PRESSED;
        event->key.repeat = native->key.repeat != 0;
        break;

    case SDL_TEXTINPUT:
        SDL_strlcpy(text, native->text.text, text_size);
        event->type = S3_EVENT_TEXT_INPUT;
        event->text.windowID = native->text.windowID;
        event->text.text = text;
        break;

    case SDL_TEXTEDITING:
        SDL_strlcpy(text, native->edit.text, text_size);
        event->type = S3_EVENT_TEXT_EDITING;
        event->edit.windowID = native->edit.windowID;
        event->edit.text = text;
        event->edit.start = native->edit.start;
        event->edit.length = native->edit.length;
        break;

    case SDL_MOUSEMOTION:
        event->type = S3_EVENT_MOUSE_MOTION;
        event->motion.windowID = native->motion.windowID;
        event->motion.which = native->motion.which;
        event->motion.state = native->motion.state;
        event->motion.x = (float)native->motion.x;
        event->motion.y = (float)native->motion.y;
        event->motion.xrel = (float)native->motion.xrel;
        event->motion.yrel = (float)native->motion.yrel;
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        event->type = native->type == SDL_MOUSEBUTTONDOWN ? S3_EVENT_MOUSE_BUTTON_DOWN : S3_EVENT_MOUSE_BUTTON_UP;
        event->button.windowID = native->button.windowID;
        event->button.which = native->button.which;
        event->button.button = native->button.button;
        event->button.down = native->button.state == SDL_PRESSED;
        event->button.clicks = native->button.clicks;
        event->button.x = (float)native->button.x;
        event->button.y = (float)native->button.y;
        break;

    case SDL_MOUSEWHEEL:
        event->type = S3_EVENT_MOUSE_WHEEL;
        event->wheel.windowID = native->wheel.windowID;
        event->wheel.which = native->wheel.which;
        event->wheel.x = native->wheel.preciseX;
        event->wheel.y = native->wheel.preciseY;
        event->wheel.integer_x = native->wheel.x;
        event->wheel.integer_y = native->wheel.y;
        event->wheel.mouse_x = (float)native->wheel.mouseX;
        event->wheel.mouse_y = (float)native->wheel.mouseY;
        // Neither SDL2 nor SDL3 folds the flip into the values.
        event->wheel.direction = native->wheel.direction;
        break;

    case SDL_CONTROLLERDEVICEADDED:
        event->type = S3_EVENT_GAMEPAD_ADDED;
#ifdef __SWITCH__
        // The layer's own Switch pads carry the instance id here too.
        event->gdevice.which = S3_FromSDL2JoystickID(native->cdevice.which);
#else
        // SDL2 carries the device index here and the instance id everywhere else.
        event->gdevice.which = S3_FromSDL2JoystickID(SDL_JoystickGetDeviceInstanceID(native->cdevice.which));
#endif
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_CONTROLLERDEVICEREMAPPED:
        event->type = native->type == SDL_CONTROLLERDEVICEREMOVED ? S3_EVENT_GAMEPAD_REMOVED : S3_EVENT_GAMEPAD_REMAPPED;
        event->gdevice.which = S3_FromSDL2JoystickID(native->cdevice.which);
        break;

    case SDL_CONTROLLERAXISMOTION:
        event->type = S3_EVENT_GAMEPAD_AXIS_MOTION;
        event->gaxis.which = S3_FromSDL2JoystickID(native->caxis.which);
        event->gaxis.axis = native->caxis.axis;
        event->gaxis.value = native->caxis.value;
        break;

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        event->type = native->type == SDL_CONTROLLERBUTTONDOWN ? S3_EVENT_GAMEPAD_BUTTON_DOWN : S3_EVENT_GAMEPAD_BUTTON_UP;
        event->gbutton.which = S3_FromSDL2JoystickID(native->cbutton.which);
        event->gbutton.button = native->cbutton.button;
        event->gbutton.down = native->cbutton.state == SDL_PRESSED;
        break;

    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
        event->type = native->type == SDL_FINGERDOWN ? S3_EVENT_FINGER_DOWN
            : native->type == SDL_FINGERUP           ? S3_EVENT_FINGER_UP
                                                     : S3_EVENT_FINGER_MOTION;
        event->tfinger.touchID = (Uint64)native->tfinger.touchId;
        event->tfinger.fingerID = (Uint64)native->tfinger.fingerId;
        event->tfinger.x = native->tfinger.x;
        event->tfinger.y = native->tfinger.y;
        event->tfinger.dx = native->tfinger.dx;
        event->tfinger.dy = native->tfinger.dy;
        event->tfinger.pressure = native->tfinger.pressure;
        event->tfinger.windowID = native->tfinger.windowID;
        break;

    default:
        if (native->type >= SDL_USEREVENT && native->type < SDL_LASTEVENT) {
            event->type = native->type;
            event->user.windowID = native->user.windowID;
            event->user.code = native->user.code;
            event->user.data1 = native->user.data1;
            event->user.data2 = native->user.data2;
        }
        break;
    }

    return event->type != S3_EVENT_FIRST;
}

static bool S3_ToSDL2Event(const S3_Event* event, SDL_Event* native)
{
    SDL_zerop(native);

    if (event->type == S3_EVENT_QUIT) {
        native->type = SDL_QUIT;
        return true;
    }

    if (event->type >= S3_EVENT_USER && event->type < S3_EVENT_LAST) {
        native->type = event->type;
        native->user.windowID = event->user.windowID;
        native->user.code = event->user.code;
        native->user.data1 = event->user.data1;
        native->user.data2 = event->user.data2;
        return true;
    }

    if (event->type >= S3_EVENT_WINDOW_FIRST && event->type <= S3_EVENT_WINDOW_LAST) {
        native->window.event = S3_ToSDL2WindowEvent(event->type);
        if (native->window.event != SDL_WINDOWEVENT_NONE) {
            native->type = SDL_WINDOWEVENT;
            native->window.windowID = event->window.windowID;
            native->window.data1 = event->window.data1;
            native->window.data2 = event->window.data2;
            return true;
        }
    }

    SDL_SetError("Event type 0x%x cannot be pushed through sdl3on2", (unsigned int)event->type);

    return false;
}

static void S3_PumpPlatform(void)
{
#ifdef __SWITCH__
    // Without SDL2's video driver nothing else runs the applet loop.
    S3_SwitchPumpApplet();
    S3_SwitchPumpPads();
#endif
}

void S3_PumpEvents(void)
{
    S3_ResetTextArena();
    S3_PumpPlatform();

    SDL_PumpEvents();
}

static bool S3_DeliverEvent(const SDL_Event* native, S3_Event* event)
{
    S3_Event scratch;
    char* text;

    text = s3_text_arena[s3_text_arena_index];
    if (native->type == SDL_TEXTINPUT || native->type == SDL_TEXTEDITING) {
        s3_text_arena_index = (s3_text_arena_index + 1) % S3_TEXT_ARENA_SIZE;
    }

    S3_TranslateEvent(native, &scratch, text, S3_TEXT_ARENA_ENTRY_SIZE);
    *event = scratch;

    return true;
}

static bool s3_poll_cycle_pumped;

bool S3_PollEvent(S3_Event* event)
{
    SDL_Event native;

    // Run the applet loop once per queue drain; SDL2 handles native event pumping.
    if (!s3_poll_cycle_pumped) {
        S3_PumpPlatform();
        s3_poll_cycle_pumped = true;
    }

    // A NULL event asks whether one is waiting and leaves it queued; SDL2 peeks on NULL too.
    if (!SDL_PollEvent(event != NULL ? &native : NULL)) {
        S3_ResetTextArena();
        s3_poll_cycle_pumped = false;
        return false;
    }

    return event == NULL || S3_DeliverEvent(&native, event);
}

static_assert(S3_ADDEVENT == (int)SDL_ADDEVENT && S3_PEEKEVENT == (int)SDL_PEEKEVENT &&
                  S3_GETEVENT == (int)SDL_GETEVENT,
              "SDL_eventaction");

int S3_PeepEvents(S3_Event* events, int numevents, S3_EventAction action, Uint32 minType, Uint32 maxType)
{
    SDL_Event native[16];
    int total = 0;

    // Outside the window range both versions number their events alike; SDL2 has one window type.
    if (maxType >= S3_EVENT_WINDOW_FIRST && minType <= S3_EVENT_WINDOW_LAST) {
        return SDL_SetError("SDL_PeepEvents through sdl3on2 cannot select window events");
    }

    if (action == S3_ADDEVENT) {
        for (; total < numevents; total++) {
            if (!S3_PushEvent(&events[total])) {
                break;
            }
        }
        return total;
    }

    while (total < numevents) {
        const int want = numevents - total < 16 ? numevents - total : 16;
        const int got = SDL_PeepEvents(native, want, (SDL_eventaction)action, minType, maxType);

        if (got < 0) {
            return total > 0 ? total : got;
        }
        for (int i = 0; events != NULL && i < got; i++) {
            S3_DeliverEvent(&native[i], &events[total + i]);
        }
        total += got;
        // Peeking again would return the same events.
        if (got < want || action == S3_PEEKEVENT) {
            break;
        }
    }

    return total;
}

bool S3_WaitEventTimeout(S3_Event* event, Sint32 timeoutMS)
{
    SDL_Event native;
    SDL_Event* const into = event != NULL ? &native : NULL;

#ifdef __SWITCH__
    // The applet loop has to keep running while the thread waits, so wait in short slices.
    const Uint64 start = SDL_GetTicks64();

    for (;;) {
        Sint32 slice = 10;

        S3_PumpPlatform();

        if (timeoutMS >= 0) {
            const Uint64 elapsed = SDL_GetTicks64() - start;

            if (elapsed >= (Uint64)timeoutMS) {
                return SDL_PollEvent(into) && (event == NULL || S3_DeliverEvent(&native, event));
            }

            if ((Uint64)slice > (Uint64)timeoutMS - elapsed) {
                slice = (Sint32)((Uint64)timeoutMS - elapsed);
            }
        }

        if (SDL_WaitEventTimeout(into, slice)) {
            return event == NULL || S3_DeliverEvent(&native, event);
        }
    }
#else
    const int rc = timeoutMS < 0 ? SDL_WaitEvent(into) : SDL_WaitEventTimeout(into, timeoutMS);

    if (rc == 0) {
        return false;
    }

    return event == NULL || S3_DeliverEvent(&native, event);
#endif
}

bool S3_WaitEvent(S3_Event* event)
{
    return S3_WaitEventTimeout(event, -1);
}

bool S3_PushEvent(S3_Event* event)
{
    SDL_Event native;

    if (event == NULL) {
        SDL_SetError("Parameter 'event' is invalid");
        return false;
    }

    if (!S3_ToSDL2Event(event, &native)) {
        return false;
    }

    // SDL2 returns 1 if queued, 0 if filtered, -1 on error; SDL3 folds the last two into false.
    return SDL_PushEvent(&native) == 1;
}

bool S3_PushWindowEvent(Uint8 sdl2_window_event, Uint32 window_id, Sint32 data1, Sint32 data2)
{
    SDL_Event native;

    SDL_zero(native);
    native.type = SDL_WINDOWEVENT;
    native.window.event = sdl2_window_event;
    native.window.windowID = window_id;
    native.window.data1 = data1;
    native.window.data2 = data2;

    return SDL_PushEvent(&native) == 1;
}

Uint32 S3_RegisterEvents(int numevents)
{
    const Uint32 first = SDL_RegisterEvents(numevents);

    // SDL2 signals exhaustion with (Uint32)-1, SDL3 with 0.
    return first == (Uint32)-1 ? 0 : first;
}

typedef struct S3_EventWatch {
    S3_EventFilter filter;
    void* userdata;
    struct S3_EventWatch* next;
} S3_EventWatch;

static S3_EventWatch* s3_event_watches;

static int SDLCALL S3_EventWatchTrampoline(void* userdata, SDL_Event* native)
{
    const S3_EventWatch* watch = (const S3_EventWatch*)userdata;
    char text[S3_TEXT_ARENA_ENTRY_SIZE];
    S3_Event event;

    // SDL2 runs watches on the pushing thread, so the text lives on this stack for the call only.
    if (S3_TranslateEvent(native, &event, text, sizeof(text))) {
        watch->filter(watch->userdata, &event);
    }

    return 1;
}

bool S3_AddEventWatch(S3_EventFilter filter, void* userdata)
{
    S3_EventWatch* watch;

    if (filter == NULL) {
        SDL_SetError("Parameter 'filter' is invalid");
        return false;
    }

    watch = (S3_EventWatch*)SDL_calloc(1, sizeof(*watch));
    if (watch == NULL) {
        SDL_SetError("Out of memory");
        return false;
    }

    watch->filter = filter;
    watch->userdata = userdata;
    watch->next = s3_event_watches;
    s3_event_watches = watch;

    SDL_AddEventWatch(S3_EventWatchTrampoline, watch);

    return true;
}

void S3_RemoveEventWatch(S3_EventFilter filter, void* userdata)
{
    S3_EventWatch** link;

    for (link = &s3_event_watches; *link != NULL; link = &(*link)->next) {
        S3_EventWatch* watch = *link;

        if (watch->filter == filter && watch->userdata == userdata) {
            SDL_DelEventWatch(S3_EventWatchTrampoline, watch);
            *link = watch->next;
            SDL_free(watch);
            return;
        }
    }
}

S3_Window* S3_GetWindowFromEvent(const S3_Event* event)
{
    S3_WindowID id;

    if (event == NULL) {
        return NULL;
    }

    if (event->type >= S3_EVENT_WINDOW_FIRST && event->type <= S3_EVENT_WINDOW_LAST) {
        id = event->window.windowID;
    } else {
        switch (event->type) {
        case S3_EVENT_KEY_DOWN:
        case S3_EVENT_KEY_UP:
            id = event->key.windowID;
            break;
        case S3_EVENT_TEXT_INPUT:
            id = event->text.windowID;
            break;
        case S3_EVENT_TEXT_EDITING:
            id = event->edit.windowID;
            break;
        case S3_EVENT_MOUSE_MOTION:
            id = event->motion.windowID;
            break;
        case S3_EVENT_MOUSE_BUTTON_DOWN:
        case S3_EVENT_MOUSE_BUTTON_UP:
            id = event->button.windowID;
            break;
        case S3_EVENT_MOUSE_WHEEL:
            id = event->wheel.windowID;
            break;
        case S3_EVENT_FINGER_DOWN:
        case S3_EVENT_FINGER_UP:
        case S3_EVENT_FINGER_MOTION:
            id = event->tfinger.windowID;
            break;
        default:
            if (event->type >= S3_EVENT_USER && event->type < S3_EVENT_LAST) {
                id = event->user.windowID;
                break;
            }
            return NULL;
        }
    }

    return id != 0 ? S3_GetWindowFromID(id) : NULL;
}

bool S3_ConvertEventToRenderCoordinates(S3_Renderer* renderer, S3_Event* event)
{
    float x;
    float y;
    float origin_x;
    float origin_y;
    float unit_x;
    float unit_y;
    SDL_Window* window;
    int window_width;
    int window_height;

    if (renderer == NULL || event == NULL) {
        SDL_SetError("Invalid renderer or event");
        return false;
    }

    switch (event->type) {
    case S3_EVENT_MOUSE_MOTION:
        SDL_RenderWindowToLogical(renderer, (int)event->motion.x, (int)event->motion.y, &x, &y);
        SDL_RenderWindowToLogical(renderer, 0, 0, &origin_x, &origin_y);
        SDL_RenderWindowToLogical(renderer, 1, 1, &unit_x, &unit_y);

        event->motion.x = x;
        event->motion.y = y;
        event->motion.xrel *= unit_x - origin_x;
        event->motion.yrel *= unit_y - origin_y;
        break;

    case S3_EVENT_MOUSE_BUTTON_DOWN:
    case S3_EVENT_MOUSE_BUTTON_UP:
        SDL_RenderWindowToLogical(renderer, (int)event->button.x, (int)event->button.y, &x, &y);

        event->button.x = x;
        event->button.y = y;
        break;

    case S3_EVENT_MOUSE_WHEEL:
        SDL_RenderWindowToLogical(renderer, (int)event->wheel.mouse_x, (int)event->wheel.mouse_y, &x, &y);

        event->wheel.mouse_x = x;
        event->wheel.mouse_y = y;
        break;

    case S3_EVENT_FINGER_DOWN:
    case S3_EVENT_FINGER_UP:
    case S3_EVENT_FINGER_MOTION:
        // SDL3 maps normalized touch to logical coordinates here; SDL2 has no counterpart.
        window = SDL_RenderGetWindow(renderer);
        if (window == NULL) {
            return false;
        }

        window_width = 0;
        window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        if (window_width <= 0 || window_height <= 0) {
            SDL_SetError("Could not get the window size");
            return false;
        }

        SDL_RenderWindowToLogical(renderer,
            (int)(event->tfinger.x * (float)window_width),
            (int)(event->tfinger.y * (float)window_height),
            &x,
            &y);
        SDL_RenderWindowToLogical(renderer, 0, 0, &origin_x, &origin_y);
        SDL_RenderWindowToLogical(renderer, 1, 1, &unit_x, &unit_y);

        event->tfinger.x = x;
        event->tfinger.y = y;
        event->tfinger.dx *= (float)window_width * (unit_x - origin_x);
        event->tfinger.dy *= (float)window_height * (unit_y - origin_y);
        break;

    default:
        break;
    }

    return true;
}

static bool S3_ScancodeRenumbered(S3_Scancode scancode)
{
    return scancode >= S3_SCANCODE_RENUMBERED_FIRST && scancode <= S3_SCANCODE_RENUMBERED_LAST;
}

const bool* S3_GetKeyboardState(int* numkeys)
{
    return (const bool*)SDL_GetKeyboardState(numkeys);
}

S3_Keymod S3_GetModState(void)
{
    return (S3_Keymod)SDL_GetModState();
}

S3_Scancode S3_GetScancodeFromName(const char* name)
{
    const SDL_Scancode scancode = SDL_GetScancodeFromName(name);

    if (S3_ScancodeRenumbered((S3_Scancode)scancode)) {
        SDL_SetError("Unknown key");
        return 0;
    }

    return (S3_Scancode)scancode;
}

const char* S3_GetScancodeName(S3_Scancode scancode)
{
    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES) {
        SDL_SetError("Parameter 'scancode' is invalid");
        return "";
    }

    if (S3_ScancodeRenumbered(scancode)) {
        return "";
    }

    return SDL_GetScancodeName((SDL_Scancode)scancode);
}

S3_Keycode S3_GetKeyFromScancode(S3_Scancode scancode, S3_Keymod modstate, bool key_event)
{
    // SDL2's lookup ignores modifiers, which matches SDL3 for SDL_KMOD_NONE.
    (void)modstate;
    (void)key_event;

    if (scancode < 0 || scancode >= SDL_NUM_SCANCODES || S3_ScancodeRenumbered(scancode)) {
        return 0;
    }

    return (S3_Keycode)SDL_GetKeyFromScancode((SDL_Scancode)scancode);
}

const char* S3_GetKeyName(S3_Keycode key)
{
    const S3_Scancode scancode = (key & (1u << 30)) != 0 ? (S3_Scancode)(key & ~(1u << 30)) : 0;

    if (S3_ScancodeRenumbered(scancode)) {
        return "";
    }

    return SDL_GetKeyName((SDL_Keycode)key);
}

bool S3_StartTextInput(S3_Window* window)
{
    // SDL2 text input is global, so this cannot isolate input to one window.
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_StartTextInput();

    return true;
}

bool S3_StopTextInput(S3_Window* window)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_StopTextInput();

    return true;
}

S3_MouseButtonFlags S3_GetMouseState(float* x, float* y)
{
    int ix = 0;
    int iy = 0;
    const Uint32 buttons = SDL_GetMouseState(&ix, &iy);

    if (x != NULL) {
        *x = (float)ix;
    }

    if (y != NULL) {
        *y = (float)iy;
    }

    return buttons;
}

S3_MouseButtonFlags S3_GetGlobalMouseState(float* x, float* y)
{
    int ix = 0;
    int iy = 0;
    const Uint32 buttons = SDL_GetGlobalMouseState(&ix, &iy);

    if (x != NULL) {
        *x = (float)ix;
    }

    if (y != NULL) {
        *y = (float)iy;
    }

    return buttons;
}

S3_MouseButtonFlags S3_GetRelativeMouseState(float* x, float* y)
{
    int ix = 0;
    int iy = 0;
    const Uint32 buttons = SDL_GetRelativeMouseState(&ix, &iy);

    if (x != NULL) {
        *x = (float)ix;
    }

    if (y != NULL) {
        *y = (float)iy;
    }

    return buttons;
}
