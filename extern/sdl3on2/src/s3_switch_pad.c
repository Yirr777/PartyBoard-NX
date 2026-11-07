// The layer reads Switch controllers from libnx, so SDL2's joystick driver never runs.

#ifdef __SWITCH__

#include <assert.h>

#include <switch.h>

#include "s3_internal.h"

#define S3_SWITCH_PAD_COUNT 8

// Player one's slot also reads the console in handheld mode.
#define S3_SWITCH_SOURCES 2

// Nintendo's USB ids, as SDL3 reports them for these controllers.
#define S3_NINTENDO_VENDOR 0x057E
#define S3_NINTENDO_JOYCON_LEFT 0x2006
#define S3_NINTENDO_JOYCON_RIGHT 0x2007
#define S3_NINTENDO_JOYCON_PAIR 0x2008
#define S3_NINTENDO_PRO 0x2009

static_assert(S3_GAMEPAD_BUTTON_COUNT <= 32, "one bit per button");

typedef struct S3_SwitchPad {
    // The current connection, or the last one while a handle to it is still open.
    S3_JoystickID instance;
    S3_GamepadType type;
    bool connected;
    int player;
    int refcount;
    u32 styles[S3_SWITCH_SOURCES];
    Uint32 buttons;
    Sint16 axes[S3_GAMEPAD_AXIS_COUNT];
    HidVibrationDeviceHandle vibration[S3_SWITCH_SOURCES][2];
    int vibration_count[S3_SWITCH_SOURCES];
    Uint64 rumble_expiry;
} S3_SwitchPad;

typedef struct S3_SwitchButton {
    u64 npad;
    S3_GamepadButton button;
} S3_SwitchButton;

// Face buttons by position, as in SDL3.
static const S3_SwitchButton s3_switch_full_buttons[] = {
    { HidNpadButton_B, S3_GAMEPAD_BUTTON_SOUTH },
    { HidNpadButton_A, S3_GAMEPAD_BUTTON_EAST },
    { HidNpadButton_Y, S3_GAMEPAD_BUTTON_WEST },
    { HidNpadButton_X, S3_GAMEPAD_BUTTON_NORTH },
    { HidNpadButton_Minus, S3_GAMEPAD_BUTTON_BACK },
    { HidNpadButton_Plus, S3_GAMEPAD_BUTTON_START },
    { HidNpadButton_StickL, S3_GAMEPAD_BUTTON_LEFT_STICK },
    { HidNpadButton_StickR, S3_GAMEPAD_BUTTON_RIGHT_STICK },
    { HidNpadButton_L, S3_GAMEPAD_BUTTON_LEFT_SHOULDER },
    { HidNpadButton_R, S3_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { HidNpadButton_Up, S3_GAMEPAD_BUTTON_DPAD_UP },
    { HidNpadButton_Down, S3_GAMEPAD_BUTTON_DPAD_DOWN },
    { HidNpadButton_Left, S3_GAMEPAD_BUTTON_DPAD_LEFT },
    { HidNpadButton_Right, S3_GAMEPAD_BUTTON_DPAD_RIGHT },
    { HidNpadButton_LeftSL, S3_GAMEPAD_BUTTON_LEFT_PADDLE1 },
    { HidNpadButton_LeftSR, S3_GAMEPAD_BUTTON_LEFT_PADDLE2 },
    { HidNpadButton_RightSR, S3_GAMEPAD_BUTTON_RIGHT_PADDLE1 },
    { HidNpadButton_RightSL, S3_GAMEPAD_BUTTON_RIGHT_PADDLE2 },
};

// Held sideways, a single Joy-Con's SL and SR are its shoulders.
static const S3_SwitchButton s3_switch_left_buttons[] = {
    { HidNpadButton_Left, S3_GAMEPAD_BUTTON_SOUTH },
    { HidNpadButton_Down, S3_GAMEPAD_BUTTON_EAST },
    { HidNpadButton_Up, S3_GAMEPAD_BUTTON_WEST },
    { HidNpadButton_Right, S3_GAMEPAD_BUTTON_NORTH },
    { HidNpadButton_Minus, S3_GAMEPAD_BUTTON_START },
    { HidNpadButton_StickL, S3_GAMEPAD_BUTTON_LEFT_STICK },
    { HidNpadButton_LeftSL, S3_GAMEPAD_BUTTON_LEFT_SHOULDER },
    { HidNpadButton_LeftSR, S3_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { HidNpadButton_L, S3_GAMEPAD_BUTTON_LEFT_PADDLE1 },
    { HidNpadButton_ZL, S3_GAMEPAD_BUTTON_LEFT_PADDLE2 },
};

static const S3_SwitchButton s3_switch_right_buttons[] = {
    { HidNpadButton_A, S3_GAMEPAD_BUTTON_SOUTH },
    { HidNpadButton_X, S3_GAMEPAD_BUTTON_EAST },
    { HidNpadButton_B, S3_GAMEPAD_BUTTON_WEST },
    { HidNpadButton_Y, S3_GAMEPAD_BUTTON_NORTH },
    { HidNpadButton_Plus, S3_GAMEPAD_BUTTON_START },
    { HidNpadButton_StickR, S3_GAMEPAD_BUTTON_LEFT_STICK },
    { HidNpadButton_RightSL, S3_GAMEPAD_BUTTON_LEFT_SHOULDER },
    { HidNpadButton_RightSR, S3_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { HidNpadButton_R, S3_GAMEPAD_BUTTON_RIGHT_PADDLE1 },
    { HidNpadButton_ZR, S3_GAMEPAD_BUTTON_RIGHT_PADDLE2 },
};

static S3_SwitchPad s3_pads[S3_SWITCH_PAD_COUNT];
static int s3_pads_refcount;
// Guards pad state from rumble and SDL_LockJoysticks callers on other threads.
static RMutex s3_pads_lock;
static S3_JoystickID s3_next_instance = 1;

static HidNpadIdType S3_SwitchSourceID(int slot, int source)
{
    return source == 0 ? (HidNpadIdType)(HidNpadIdType_No1 + slot) : HidNpadIdType_Handheld;
}

static int S3_SwitchSourceCount(int slot)
{
    return slot == 0 ? 2 : 1;
}

// Returns the style the state was read in, or 0 if nothing is connected.
static u32 S3_SwitchReadNpad(HidNpadIdType id, HidNpadCommonState* state)
{
    const u32 styles = hidGetNpadStyleSet(id);
    u32 style = 0;
    size_t count = 0;

    if ((styles & HidNpadStyleTag_NpadFullKey) != 0) {
        style = HidNpadStyleTag_NpadFullKey;
        count = hidGetNpadStatesFullKey(id, state, 1);
    } else if ((styles & HidNpadStyleTag_NpadHandheld) != 0) {
        style = HidNpadStyleTag_NpadHandheld;
        count = hidGetNpadStatesHandheld(id, state, 1);
    } else if ((styles & HidNpadStyleTag_NpadJoyDual) != 0) {
        style = HidNpadStyleTag_NpadJoyDual;
        count = hidGetNpadStatesJoyDual(id, state, 1);
    } else if ((styles & HidNpadStyleTag_NpadJoyLeft) != 0) {
        style = HidNpadStyleTag_NpadJoyLeft;
        count = hidGetNpadStatesJoyLeft(id, state, 1);
    } else if ((styles & HidNpadStyleTag_NpadJoyRight) != 0) {
        style = HidNpadStyleTag_NpadJoyRight;
        count = hidGetNpadStatesJoyRight(id, state, 1);
    }

    return count != 0 && (state->attributes & HidNpadAttribute_IsConnected) != 0 ? style : 0;
}

static S3_GamepadType S3_SwitchPadType(u32 style)
{
    switch (style) {
    case HidNpadStyleTag_NpadFullKey:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
    case HidNpadStyleTag_NpadHandheld:
    case HidNpadStyleTag_NpadJoyDual:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
    case HidNpadStyleTag_NpadJoyLeft:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT;
    case HidNpadStyleTag_NpadJoyRight:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
    default:
        return S3_GAMEPAD_TYPE_UNKNOWN;
    }
}

static void S3_SwitchMergeAxis(Sint16* axis, Sint16 value)
{
    if (SDL_abs(value) > SDL_abs(*axis)) {
        *axis = value;
    }
}

// HID sticks stop at +/-32767, so negating one cannot overflow; HID's y points up and SDL's down.
static void S3_SwitchAddState(u32 style, const HidNpadCommonState* state, Uint32* buttons, Sint16* axes)
{
    const S3_SwitchButton* map = s3_switch_full_buttons;
    size_t count = SDL_arraysize(s3_switch_full_buttons);
    size_t i;

    if (style == HidNpadStyleTag_NpadJoyLeft) {
        map = s3_switch_left_buttons;
        count = SDL_arraysize(s3_switch_left_buttons);
    } else if (style == HidNpadStyleTag_NpadJoyRight) {
        map = s3_switch_right_buttons;
        count = SDL_arraysize(s3_switch_right_buttons);
    }

    for (i = 0; i < count; i++) {
        if ((state->buttons & map[i].npad) != 0) {
            *buttons |= 1u << map[i].button;
        }
    }

    // Sideways, up on the left Joy-Con's stick points left and on the right Joy-Con's points right.
    switch (style) {
    case HidNpadStyleTag_NpadJoyLeft:
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTX], (Sint16)-state->analog_stick_l.y);
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTY], (Sint16)-state->analog_stick_l.x);
        break;
    case HidNpadStyleTag_NpadJoyRight:
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTX], (Sint16)state->analog_stick_r.y);
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTY], (Sint16)state->analog_stick_r.x);
        break;
    default:
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTX], (Sint16)state->analog_stick_l.x);
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_LEFTY], (Sint16)-state->analog_stick_l.y);
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_RIGHTX], (Sint16)state->analog_stick_r.x);
        S3_SwitchMergeAxis(&axes[S3_GAMEPAD_AXIS_RIGHTY], (Sint16)-state->analog_stick_r.y);
        // ZL and ZR are digital.
        if ((state->buttons & HidNpadButton_ZL) != 0) {
            axes[S3_GAMEPAD_AXIS_LEFT_TRIGGER] = SDL_JOYSTICK_AXIS_MAX;
        }
        if ((state->buttons & HidNpadButton_ZR) != 0) {
            axes[S3_GAMEPAD_AXIS_RIGHT_TRIGGER] = SDL_JOYSTICK_AXIS_MAX;
        }
        break;
    }
}

static void S3_SwitchInitVibration(S3_SwitchPad* pad, int slot, int source, u32 style)
{
    // A single Joy-Con has one actuator; the other styles have two.
    const int count = style == HidNpadStyleTag_NpadJoyLeft || style == HidNpadStyleTag_NpadJoyRight ? 1 : 2;

    pad->styles[source] = style;
    pad->vibration_count[source] = 0;

    if (style != 0 && R_SUCCEEDED(hidInitializeVibrationDevices(pad->vibration[source], count, S3_SwitchSourceID(slot, source), (HidNpadStyleTag)style))) {
        pad->vibration_count[source] = count;
    }
}

static void S3_SwitchSendVibration(S3_SwitchPad* pad, const HidVibrationValue* value)
{
    HidVibrationValue values[2];
    int source;

    values[0] = *value;
    values[1] = *value;

    for (source = 0; source < S3_SWITCH_SOURCES; source++) {
        if (pad->vibration_count[source] > 0) {
            hidSendVibrationValues(pad->vibration[source], values, pad->vibration_count[source]);
        }
    }
}

static void S3_SwitchVibrate(S3_SwitchPad* pad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    // The bands' resting frequencies, which SDL3 uses too.
    HidVibrationValue value;

    value.amp_low = (float)low_frequency_rumble / 65535.0f;
    value.freq_low = 160.0f;
    value.amp_high = (float)high_frequency_rumble / 65535.0f;
    value.freq_high = 320.0f;
    S3_SwitchSendVibration(pad, &value);
}

// Pushed as SDL2 events, whose instance ids start at 0.
static void S3_SwitchPushPadEvent(Uint32 type, const S3_SwitchPad* pad, Uint8 index, Sint16 value)
{
    const SDL_JoystickID which = (SDL_JoystickID)(pad->instance - 1u);
    SDL_Event native;

    SDL_zero(native);
    native.type = type;

    switch (type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        native.cbutton.which = which;
        native.cbutton.button = index;
        native.cbutton.state = type == SDL_CONTROLLERBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        break;
    case SDL_CONTROLLERAXISMOTION:
        native.caxis.which = which;
        native.caxis.axis = index;
        native.caxis.value = value;
        break;
    default:
        native.cdevice.which = which;
        break;
    }

    SDL_PushEvent(&native);
}

static void S3_SwitchSetState(S3_SwitchPad* pad, Uint32 buttons, const Sint16* axes)
{
    // SDL3 sends input events for open gamepads only.
    if (pad->refcount > 0) {
        const Uint32 changed = buttons ^ pad->buttons;
        int i;

        for (i = 0; i < S3_GAMEPAD_BUTTON_COUNT; i++) {
            if ((changed & (1u << i)) != 0) {
                S3_SwitchPushPadEvent((buttons & (1u << i)) != 0 ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP, pad, (Uint8)i, 0);
            }
        }

        for (i = 0; i < S3_GAMEPAD_AXIS_COUNT; i++) {
            if (axes[i] != pad->axes[i]) {
                S3_SwitchPushPadEvent(SDL_CONTROLLERAXISMOTION, pad, (Uint8)i, axes[i]);
            }
        }
    }

    pad->buttons = buttons;
    SDL_memcpy(pad->axes, axes, sizeof(pad->axes));
}

void S3_SwitchPumpPads(void)
{
    int slot;

    if (s3_pads_refcount == 0) {
        return;
    }

    rmutexLock(&s3_pads_lock);
    for (slot = 0; slot < S3_SWITCH_PAD_COUNT; slot++) {
        S3_SwitchPad* pad = &s3_pads[slot];
        S3_GamepadType type = S3_GAMEPAD_TYPE_UNKNOWN;
        Uint32 buttons = 0;
        Sint16 axes[S3_GAMEPAD_AXIS_COUNT];
        int source;

        SDL_zeroa(axes);

        for (source = 0; source < S3_SwitchSourceCount(slot); source++) {
            HidNpadCommonState state;
            const u32 style = S3_SwitchReadNpad(S3_SwitchSourceID(slot, source), &state);

            if (style != pad->styles[source]) {
                S3_SwitchInitVibration(pad, slot, source, style);
            }

            if (style != 0) {
                if (type == S3_GAMEPAD_TYPE_UNKNOWN) {
                    type = S3_SwitchPadType(style);
                }
                S3_SwitchAddState(style, &state, &buttons, axes);
            }
        }

        // SDL3 treats a controller that changes style as a different gamepad.
        if (type != (pad->connected ? pad->type : S3_GAMEPAD_TYPE_UNKNOWN)) {
            if (pad->connected) {
                pad->connected = false;
                S3_SwitchPushPadEvent(SDL_CONTROLLERDEVICEREMOVED, pad, 0, 0);
            }

            if (type != S3_GAMEPAD_TYPE_UNKNOWN) {
                pad->instance = s3_next_instance++;
                pad->type = type;
                pad->connected = true;
                // The player number the controller's lights show.
                pad->player = slot;
                pad->buttons = 0;
                SDL_zeroa(pad->axes);
                pad->rumble_expiry = 0;
                S3_SwitchPushPadEvent(SDL_CONTROLLERDEVICEADDED, pad, 0, 0);
            }
        }

        if (!pad->connected) {
            continue;
        }

        S3_SwitchSetState(pad, buttons, axes);

        if (pad->rumble_expiry != 0 && SDL_GetTicks64() >= pad->rumble_expiry) {
            pad->rumble_expiry = 0;
            S3_SwitchVibrate(pad, 0, 0);
        }
    }
    rmutexUnlock(&s3_pads_lock);
}

bool S3_SwitchInitPads(void)
{
    int slot;

    if (s3_pads_refcount++ > 0) {
        return true;
    }

    padConfigureInput(S3_SWITCH_PAD_COUNT, HidNpadStyleSet_NpadStandard);
    hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);

    rmutexLock(&s3_pads_lock);
    SDL_zeroa(s3_pads);
    for (slot = 0; slot < S3_SWITCH_PAD_COUNT; slot++) {
        s3_pads[slot].player = slot;
    }
    rmutexUnlock(&s3_pads_lock);

    // SDL3 announces the controllers already connected.
    S3_SwitchPumpPads();

    return true;
}

void S3_SwitchQuitPads(bool all)
{
    int slot;

    if (s3_pads_refcount == 0) {
        return;
    }

    s3_pads_refcount = all ? 0 : s3_pads_refcount - 1;
    if (s3_pads_refcount > 0) {
        return;
    }

    rmutexLock(&s3_pads_lock);
    for (slot = 0; slot < S3_SWITCH_PAD_COUNT; slot++) {
        if (s3_pads[slot].connected) {
            S3_SwitchVibrate(&s3_pads[slot], 0, 0);
        }
        if (s3_pads[slot].refcount > 0) {
            S3_ReleaseObjectProperties(&s3_pads[slot]);
        }
    }
    SDL_zeroa(s3_pads);
    rmutexUnlock(&s3_pads_lock);
}

bool S3_SwitchPadsInitialized(void)
{
    return s3_pads_refcount > 0;
}

static S3_SwitchPad* S3_SwitchPadForID(S3_JoystickID instance_id)
{
    int slot;

    for (slot = 0; slot < S3_SWITCH_PAD_COUNT; slot++) {
        if (instance_id != 0 && s3_pads[slot].connected && s3_pads[slot].instance == instance_id) {
            return &s3_pads[slot];
        }
    }

    SDL_SetError("Invalid joystick instance ID");

    return NULL;
}

static S3_SwitchPad* S3_SwitchPadFrom(const void* handle, const char* parameter)
{
    int slot;

    for (slot = 0; slot < S3_SWITCH_PAD_COUNT; slot++) {
        if (handle == &s3_pads[slot] && s3_pads[slot].refcount > 0) {
            return &s3_pads[slot];
        }
    }

    SDL_SetError("Parameter '%s' is invalid", parameter);

    return NULL;
}

static S3_SwitchPad* S3_SwitchOpen(S3_JoystickID instance_id)
{
    S3_SwitchPad* pad;

    rmutexLock(&s3_pads_lock);
    pad = S3_SwitchPadForID(instance_id);
    if (pad != NULL) {
        pad->refcount++;
    }
    rmutexUnlock(&s3_pads_lock);

    return pad;
}

static Uint16 S3_SwitchPadProduct(S3_GamepadType type)
{
    switch (type) {
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        return S3_NINTENDO_JOYCON_LEFT;
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        return S3_NINTENDO_JOYCON_RIGHT;
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return S3_NINTENDO_JOYCON_PAIR;
    default:
        return S3_NINTENDO_PRO;
    }
}

void S3_LockJoysticks(void)
{
    rmutexLock(&s3_pads_lock);
}

void S3_UnlockJoysticks(void)
{
    rmutexUnlock(&s3_pads_lock);
}

S3_Joystick* S3_OpenJoystick(S3_JoystickID instance_id)
{
    return (S3_Joystick*)S3_SwitchOpen(instance_id);
}

S3_JoystickID S3_GetJoystickID(S3_Joystick* joystick)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(joystick, "joystick");

    return pad != NULL ? pad->instance : 0;
}

S3_PowerState S3_GetJoystickPowerInfo(S3_Joystick* joystick, int* percent)
{
    if (percent != NULL) {
        *percent = -1;
    }

    // HID files a controller's battery level in a different slot for each style.
    return S3_SwitchPadFrom(joystick, "joystick") != NULL ? S3_POWERSTATE_UNKNOWN : S3_POWERSTATE_ERROR;
}

S3_JoystickID S3_AttachVirtualJoystick(const S3_VirtualJoystickDesc* desc)
{
    (void)desc;

    SDL_SetError("Virtual joysticks are not supported by sdl3on2 on Switch");

    return 0;
}

bool S3_SetJoystickVirtualButton(S3_Joystick* joystick, int button, bool down)
{
    (void)joystick;
    (void)button;
    (void)down;

    SDL_SetError("Virtual joysticks are not supported by sdl3on2 on Switch");

    return false;
}

S3_Gamepad* S3_OpenGamepad(S3_JoystickID instance_id)
{
    return (S3_Gamepad*)S3_SwitchOpen(instance_id);
}

void S3_CloseGamepad(S3_Gamepad* gamepad)
{
    S3_SwitchPad* pad;

    if (gamepad == NULL) {
        return;
    }

    rmutexLock(&s3_pads_lock);
    pad = S3_SwitchPadFrom(gamepad, "gamepad");
    if (pad != NULL && --pad->refcount == 0) {
        // SDL3 stops a gamepad's rumble when the last handle closes.
        if (pad->connected) {
            S3_SwitchVibrate(pad, 0, 0);
        }
        pad->rumble_expiry = 0;
        S3_ReleaseObjectProperties(gamepad);
    }
    rmutexUnlock(&s3_pads_lock);
}

S3_JoystickID S3_GetGamepadID(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    return pad != NULL ? pad->instance : 0;
}

S3_Joystick* S3_GetGamepadJoystick(S3_Gamepad* gamepad)
{
    return S3_SwitchPadFrom(gamepad, "gamepad") != NULL ? (S3_Joystick*)gamepad : NULL;
}

S3_PropertiesID S3_GetGamepadProperties(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");
    S3_PropertiesID props;
    bool created;

    if (pad == NULL) {
        return 0;
    }

    props = S3_AcquireObjectProperties(gamepad, &created);
    if (props != 0) {
        S3_SetBooleanProperty(props, S3_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, pad->vibration_count[0] + pad->vibration_count[1] > 0);
    }

    return props;
}

static const char* S3_SwitchPadName(const S3_SwitchPad* pad)
{
    switch (pad->type) {
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        return "Nintendo Switch Joy-Con (L)";
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        return "Nintendo Switch Joy-Con (R)";
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return "Nintendo Switch Joy-Con (L/R)";
    default:
        return "Nintendo Switch Pro Controller";
    }
}

const char* S3_GetGamepadName(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    return pad != NULL ? S3_SwitchPadName(pad) : NULL;
}

const char* S3_GetGamepadNameForID(S3_JoystickID instance_id)
{
    const S3_SwitchPad* pad = S3_SwitchPadForID(instance_id);

    return pad != NULL ? S3_SwitchPadName(pad) : NULL;
}

S3_Gamepad* S3_GetGamepadFromID(S3_JoystickID instance_id)
{
    S3_SwitchPad* pad;

    rmutexLock(&s3_pads_lock);
    pad = S3_SwitchPadForID(instance_id);
    if (pad != NULL && pad->refcount == 0) {
        SDL_SetError("Gamepad %u is not open", (unsigned int)instance_id);
        pad = NULL;
    }
    rmutexUnlock(&s3_pads_lock);

    return (S3_Gamepad*)pad;
}

const char* S3_GetGamepadSerial(S3_Gamepad* gamepad)
{
    S3_SwitchPadFrom(gamepad, "gamepad");

    return NULL;
}

S3_GamepadType S3_GetGamepadType(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    return pad != NULL ? pad->type : S3_GAMEPAD_TYPE_UNKNOWN;
}

Uint16 S3_GetGamepadVendor(S3_Gamepad* gamepad)
{
    return S3_SwitchPadFrom(gamepad, "gamepad") != NULL ? S3_NINTENDO_VENDOR : 0;
}

Uint16 S3_GetGamepadProduct(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    return pad != NULL ? S3_SwitchPadProduct(pad->type) : 0;
}

S3_GUID S3_GetGamepadGUIDForID(S3_JoystickID instance_id)
{
    const S3_SwitchPad* pad = S3_SwitchPadForID(instance_id);
    S3_GUID guid;

    SDL_zero(guid);

    // SDL's bus, CRC, vendor, 0, product, 0 layout, which SDL_GetJoystickGUIDInfo reads back.
    if (pad != NULL) {
        const Uint16 product = S3_SwitchPadProduct(pad->type);

        guid.data[4] = S3_NINTENDO_VENDOR & 0xFF;
        guid.data[5] = S3_NINTENDO_VENDOR >> 8;
        guid.data[8] = product & 0xFF;
        guid.data[9] = product >> 8;
    }

    return guid;
}

int S3_GetGamepadPlayerIndex(S3_Gamepad* gamepad)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    return pad != NULL ? pad->player : -1;
}

int S3_GetGamepadPlayerIndexForID(S3_JoystickID instance_id)
{
    const S3_SwitchPad* pad = S3_SwitchPadForID(instance_id);

    return pad != NULL ? pad->player : -1;
}

bool S3_SetGamepadPlayerIndex(S3_Gamepad* gamepad, int player_index)
{
    S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    if (pad == NULL) {
        return false;
    }

    pad->player = player_index < 0 ? -1 : player_index;

    return true;
}

S3_PowerState S3_GetGamepadPowerInfo(S3_Gamepad* gamepad, int* percent)
{
    return S3_GetJoystickPowerInfo((S3_Joystick*)gamepad, percent);
}

Sint16 S3_GetGamepadAxis(S3_Gamepad* gamepad, S3_GamepadAxis axis)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    if (pad == NULL) {
        return 0;
    }

    if (axis <= S3_GAMEPAD_AXIS_INVALID || axis >= S3_GAMEPAD_AXIS_COUNT) {
        SDL_SetError("Parameter 'axis' is invalid");
        return 0;
    }

    return pad->connected ? pad->axes[axis] : 0;
}

bool S3_GetGamepadButton(S3_Gamepad* gamepad, S3_GamepadButton button)
{
    const S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    if (pad == NULL) {
        return false;
    }

    if (button <= S3_GAMEPAD_BUTTON_INVALID || button >= S3_GAMEPAD_BUTTON_COUNT) {
        SDL_SetError("Parameter 'button' is invalid");
        return false;
    }

    return pad->connected && (pad->buttons & (1u << button)) != 0;
}

// The caller holds s3_pads_lock.
static S3_SwitchPad* S3_SwitchRumblePad(S3_Gamepad* gamepad)
{
    S3_SwitchPad* pad = S3_SwitchPadFrom(gamepad, "gamepad");

    if (pad != NULL && (!pad->connected || pad->vibration_count[0] + pad->vibration_count[1] == 0)) {
        SDL_SetError("That operation is not supported");
        return NULL;
    }

    return pad;
}

bool S3_RumbleGamepad(S3_Gamepad* gamepad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    S3_SwitchPad* pad;

    rmutexLock(&s3_pads_lock);
    pad = S3_SwitchRumblePad(gamepad);
    if (pad != NULL) {
        S3_SwitchVibrate(pad, low_frequency_rumble, high_frequency_rumble);

        // As in SDL3, a zero duration lasts until the next call and any other is capped at 0xFFFF ms.
        pad->rumble_expiry = 0;
        if ((low_frequency_rumble != 0 || high_frequency_rumble != 0) && duration_ms != 0) {
            pad->rumble_expiry = SDL_GetTicks64() + SDL_min(duration_ms, 0xFFFFu);
        }
    }
    rmutexUnlock(&s3_pads_lock);

    return pad != NULL;
}

bool S3_SetGamepadLED(S3_Gamepad* gamepad, Uint8 red, Uint8 green, Uint8 blue)
{
    (void)red;
    (void)green;
    (void)blue;

    if (S3_SwitchPadFrom(gamepad, "gamepad") == NULL) {
        return false;
    }

    SDL_SetError("That operation is not supported");

    return false;
}

bool S3_SendGamepadEffect(S3_Gamepad* gamepad, const void* data, int size)
{
    S3_SwitchPad* pad;
    HidVibrationValue value;

    if (data == NULL || size != (int)sizeof(value)) {
        SDL_SetError("Parameter 'size' is invalid");
        return false;
    }
    SDL_memcpy(&value, data, sizeof(value));

    rmutexLock(&s3_pads_lock);
    pad = S3_SwitchRumblePad(gamepad);
    if (pad != NULL) {
        S3_SwitchSendVibration(pad, &value);
        pad->rumble_expiry = 0;
    }
    rmutexUnlock(&s3_pads_lock);

    return pad != NULL;
}

bool S3_GamepadHasSensor(S3_Gamepad* gamepad, S3_SensorType type)
{
    (void)type;

    S3_SwitchPadFrom(gamepad, "gamepad");

    return false;
}

bool S3_SetGamepadSensorEnabled(S3_Gamepad* gamepad, S3_SensorType type, bool enabled)
{
    (void)type;
    (void)enabled;

    if (S3_SwitchPadFrom(gamepad, "gamepad") == NULL) {
        return false;
    }

    SDL_SetError("That operation is not supported");

    return false;
}

bool S3_GetGamepadSensorData(S3_Gamepad* gamepad, S3_SensorType type, float* data, int num_values)
{
    (void)type;
    (void)data;
    (void)num_values;

    if (S3_SwitchPadFrom(gamepad, "gamepad") == NULL) {
        return false;
    }

    SDL_SetError("That operation is not supported");

    return false;
}

#else

// Keeps the translation unit non-empty off Switch.
typedef int s3_switch_pad_unused;

#endif
