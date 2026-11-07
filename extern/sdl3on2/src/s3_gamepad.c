#include <assert.h>
#include <stdio.h>

#include "s3_internal.h"

static_assert(sizeof(S3_GUID) == sizeof(SDL_GUID), "S3_GUID size");
static_assert((int)S3_GAMEPAD_BUTTON_TOUCHPAD == (int)SDL_CONTROLLER_BUTTON_TOUCHPAD, "SDL_CONTROLLER_BUTTON_TOUCHPAD");
static_assert((int)S3_GAMEPAD_BUTTON_RIGHT_PADDLE1 == (int)SDL_CONTROLLER_BUTTON_PADDLE1, "SDL_CONTROLLER_BUTTON_PADDLE1");
static_assert((int)S3_GAMEPAD_AXIS_RIGHT_TRIGGER == (int)SDL_CONTROLLER_AXIS_TRIGGERRIGHT, "SDL_CONTROLLER_AXIS_TRIGGERRIGHT");
static_assert((int)S3_SENSOR_GYRO_R == (int)SDL_SENSOR_GYRO_R, "SDL_SENSOR_GYRO_R");
static_assert((int)S3_JOYSTICK_TYPE_THROTTLE == (int)SDL_JOYSTICK_TYPE_THROTTLE, "SDL_JOYSTICK_TYPE_THROTTLE");

#define S3_SDL2_BUTTON_COUNT ((int)SDL_CONTROLLER_BUTTON_MAX)

#ifndef __SWITCH__
static SDL_GameController* S3_UnwrapGamepad(S3_Gamepad* gamepad)
{
    return (SDL_GameController*)gamepad;
}

static SDL_Joystick* S3_UnwrapJoystick(S3_Joystick* joystick)
{
    return (SDL_Joystick*)joystick;
}
#endif

static bool S3_InvalidGamepad(void)
{
    SDL_SetError("Parameter 'gamepad' is invalid");
    return false;
}

S3_JoystickID S3_FromSDL2JoystickID(SDL_JoystickID id)
{
    // SDL2 numbers instances from 0; SDL3 reserves 0 for "invalid".
    return id < 0 ? 0u : (S3_JoystickID)id + 1u;
}

#ifndef __SWITCH__
static SDL_JoystickID S3_ToSDL2JoystickID(S3_JoystickID id)
{
    return id == 0u ? -1 : (SDL_JoystickID)(id - 1u);
}

// As many as SDL2 tracks numbers for.
#define S3_MAX_PLAYER_INDEX 16

int S3_JoystickDeviceIndex(S3_JoystickID instance_id)
{
    int index;
    int count;

    if (instance_id == 0) {
        SDL_SetError("Invalid joystick instance ID");
        return -1;
    }

    SDL_LockJoysticks();
    count = SDL_NumJoysticks();
    for (index = 0; index < count; index++) {
        if (S3_FromSDL2JoystickID(SDL_JoystickGetDeviceInstanceID(index)) == instance_id) {
            SDL_UnlockJoysticks();
            return index;
        }
    }
    SDL_UnlockJoysticks();

    SDL_SetError("Invalid joystick instance ID");

    return -1;
}

static S3_PowerState S3_PowerStateFromLevel(SDL_JoystickPowerLevel level, int* percent)
{
    // SDL2 reports coarse levels; the percentages are the top of each SDL2 band.
    int value = -1;
    S3_PowerState state;

    switch (level) {
    case SDL_JOYSTICK_POWER_EMPTY:
        state = S3_POWERSTATE_ON_BATTERY;
        value = 5;
        break;
    case SDL_JOYSTICK_POWER_LOW:
        state = S3_POWERSTATE_ON_BATTERY;
        value = 20;
        break;
    case SDL_JOYSTICK_POWER_MEDIUM:
        state = S3_POWERSTATE_ON_BATTERY;
        value = 70;
        break;
    case SDL_JOYSTICK_POWER_FULL:
        state = S3_POWERSTATE_ON_BATTERY;
        value = 100;
        break;
    case SDL_JOYSTICK_POWER_WIRED:
        state = S3_POWERSTATE_NO_BATTERY;
        break;
    default:
        state = S3_POWERSTATE_UNKNOWN;
        break;
    }

    if (percent != NULL) {
        *percent = value;
    }

    return state;
}
#endif

void S3_GUIDToString(S3_GUID guid, char* pszGUID, int cbGUID)
{
    SDL_GUID native;

    SDL_memcpy(native.data, guid.data, sizeof(native.data));
    SDL_GUIDToString(native, pszGUID, cbGUID);
}

S3_GUID S3_StringToGUID(const char* pchGUID)
{
    const SDL_GUID native = SDL_GUIDFromString(pchGUID);
    S3_GUID guid;

    SDL_memcpy(guid.data, native.data, sizeof(guid.data));

    return guid;
}

void S3_GetJoystickGUIDInfo(S3_GUID guid, Uint16* vendor, Uint16* product, Uint16* version, Uint16* crc16)
{
    SDL_JoystickGUID native;

    SDL_memcpy(native.data, guid.data, sizeof(native.data));
    SDL_GetJoystickGUIDInfo(native, vendor, product, version, crc16);
}

#ifndef __SWITCH__
void S3_LockJoysticks(void)
{
    SDL_LockJoysticks();
}

void S3_UnlockJoysticks(void)
{
    SDL_UnlockJoysticks();
}

S3_Joystick* S3_OpenJoystick(S3_JoystickID instance_id)
{
    const int index = S3_JoystickDeviceIndex(instance_id);

    if (index < 0) {
        return NULL;
    }

    return (S3_Joystick*)SDL_JoystickOpen(index);
}

S3_JoystickID S3_GetJoystickID(S3_Joystick* joystick)
{
    if (joystick == NULL) {
        SDL_SetError("Parameter 'joystick' is invalid");
        return 0;
    }

    return S3_FromSDL2JoystickID(SDL_JoystickInstanceID(S3_UnwrapJoystick(joystick)));
}

S3_PowerState S3_GetJoystickPowerInfo(S3_Joystick* joystick, int* percent)
{
    if (percent != NULL) {
        *percent = -1;
    }

    if (joystick == NULL) {
        SDL_SetError("Parameter 'joystick' is invalid");
        return S3_POWERSTATE_ERROR;
    }

    return S3_PowerStateFromLevel(SDL_JoystickCurrentPowerLevel(S3_UnwrapJoystick(joystick)), percent);
}

typedef struct S3_VirtualJoystick {
    S3_VirtualJoystickDesc desc;
} S3_VirtualJoystick;

static void SDLCALL S3_VirtualUpdate(void* userdata)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    virt->desc.Update(virt->desc.userdata);
}

static void SDLCALL S3_VirtualSetPlayerIndex(void* userdata, int player_index)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    virt->desc.SetPlayerIndex(virt->desc.userdata, player_index);
}

static int SDLCALL S3_VirtualRumble(void* userdata, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    return virt->desc.Rumble(virt->desc.userdata, low_frequency_rumble, high_frequency_rumble) ? 0 : -1;
}

static int SDLCALL S3_VirtualRumbleTriggers(void* userdata, Uint16 left_rumble, Uint16 right_rumble)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    return virt->desc.RumbleTriggers(virt->desc.userdata, left_rumble, right_rumble) ? 0 : -1;
}

static int SDLCALL S3_VirtualSetLED(void* userdata, Uint8 red, Uint8 green, Uint8 blue)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    return virt->desc.SetLED(virt->desc.userdata, red, green, blue) ? 0 : -1;
}

static int SDLCALL S3_VirtualSendEffect(void* userdata, const void* data, int size)
{
    const S3_VirtualJoystick* virt = (const S3_VirtualJoystick*)userdata;

    return virt->desc.SendEffect(virt->desc.userdata, data, size) ? 0 : -1;
}

S3_JoystickID S3_AttachVirtualJoystick(const S3_VirtualJoystickDesc* desc)
{
    S3_VirtualJoystick* virt;
    SDL_VirtualJoystickDesc native;
    int index;

    if (desc == NULL) {
        SDL_SetError("Parameter 'desc' is invalid");
        return 0;
    }

    if (desc->version < sizeof(*desc)) {
        SDL_SetError("Invalid desc, should be initialized with SDL_INIT_INTERFACE()");
        return 0;
    }

    // SDL2's virtual joysticks have no balls, touchpads or sensors.
    if (desc->nballs != 0 || desc->ntouchpads != 0 || desc->nsensors != 0 || desc->SetSensorsEnabled != NULL) {
        SDL_SetError("Virtual joystick balls, touchpads and sensors are not supported by sdl3on2");
        return 0;
    }

    virt = (S3_VirtualJoystick*)SDL_calloc(1, sizeof(*virt));
    if (virt == NULL) {
        SDL_SetError("Out of memory");
        return 0;
    }

    virt->desc = *desc;

    SDL_zero(native);
    native.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    native.type = desc->type;
    native.naxes = desc->naxes;
    native.nbuttons = desc->nbuttons;
    native.nhats = desc->nhats;
    native.vendor_id = desc->vendor_id;
    native.product_id = desc->product_id;
    native.button_mask = desc->button_mask;
    native.axis_mask = desc->axis_mask;
    native.name = desc->name;
    native.userdata = virt;
    native.Update = desc->Update != NULL ? S3_VirtualUpdate : NULL;
    native.SetPlayerIndex = desc->SetPlayerIndex != NULL ? S3_VirtualSetPlayerIndex : NULL;
    native.Rumble = desc->Rumble != NULL ? S3_VirtualRumble : NULL;
    native.RumbleTriggers = desc->RumbleTriggers != NULL ? S3_VirtualRumbleTriggers : NULL;
    native.SetLED = desc->SetLED != NULL ? S3_VirtualSetLED : NULL;
    native.SendEffect = desc->SendEffect != NULL ? S3_VirtualSendEffect : NULL;

    // SDL2 answers with a device index, SDL3 with an instance id.
    index = SDL_JoystickAttachVirtualEx(&native);
    if (index < 0) {
        SDL_free(virt);
        return 0;
    }

    return S3_FromSDL2JoystickID(SDL_JoystickGetDeviceInstanceID(index));
}

bool S3_SetJoystickVirtualButton(S3_Joystick* joystick, int button, bool down)
{
    if (joystick == NULL) {
        SDL_SetError("Parameter 'joystick' is invalid");
        return false;
    }

    return SDL_JoystickSetVirtualButton(S3_UnwrapJoystick(joystick), button, down ? SDL_PRESSED : SDL_RELEASED) == 0;
}

S3_Gamepad* S3_OpenGamepad(S3_JoystickID instance_id)
{
    const int index = S3_JoystickDeviceIndex(instance_id);
    SDL_GameController* native;

    if (index < 0) {
        return NULL;
    }

    native = SDL_GameControllerOpen(index);
    if (native == NULL) {
        return NULL;
    }

    // SDL3 numbers a gamepad as it arrives; SDL2 leaves that to drivers, and some never do.
    if (SDL_GameControllerGetPlayerIndex(native) < 0) {
        int player;

        for (player = 0; player < S3_MAX_PLAYER_INDEX; player++) {
            if (SDL_GameControllerFromPlayerIndex(player) == NULL) {
                SDL_GameControllerSetPlayerIndex(native, player);
                break;
            }
        }
    }

    return (S3_Gamepad*)native;
}

void S3_CloseGamepad(S3_Gamepad* gamepad)
{
    SDL_GameController* native = S3_UnwrapGamepad(gamepad);
    SDL_JoystickID id;

    if (native == NULL) {
        return;
    }

    id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(native));
    SDL_GameControllerClose(native);

    // SDL2 refcounts opens; the properties go with the last close, as in SDL3.
    if (SDL_GameControllerFromInstanceID(id) != native) {
        S3_ReleaseObjectProperties(gamepad);
    }
}

S3_JoystickID S3_GetGamepadID(S3_Gamepad* gamepad)
{
    SDL_Joystick* joystick = gamepad != NULL ? SDL_GameControllerGetJoystick(S3_UnwrapGamepad(gamepad)) : NULL;

    if (joystick == NULL) {
        S3_InvalidGamepad();
        return 0;
    }

    return S3_FromSDL2JoystickID(SDL_JoystickInstanceID(joystick));
}

S3_Joystick* S3_GetGamepadJoystick(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return NULL;
    }

    return (S3_Joystick*)SDL_GameControllerGetJoystick(S3_UnwrapGamepad(gamepad));
}

S3_PropertiesID S3_GetGamepadProperties(S3_Gamepad* gamepad)
{
    SDL_GameController* native = S3_UnwrapGamepad(gamepad);
    S3_PropertiesID props;
    bool created;

    if (native == NULL) {
        S3_InvalidGamepad();
        return 0;
    }

    props = S3_AcquireObjectProperties(gamepad, &created);
    if (props == 0) {
        return 0;
    }

    // SDL2 exposes the capabilities as queries, not properties; SDL3 keeps them current.
    S3_SetBooleanProperty(props, S3_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, SDL_GameControllerHasLED(native) == SDL_TRUE);
    S3_SetBooleanProperty(props, S3_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, SDL_GameControllerHasRumble(native) == SDL_TRUE);
    S3_SetBooleanProperty(props, S3_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, SDL_GameControllerHasRumbleTriggers(native) == SDL_TRUE);

    return props;
}

const char* S3_GetGamepadName(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return NULL;
    }

    return SDL_GameControllerName(S3_UnwrapGamepad(gamepad));
}

const char* S3_GetGamepadSerial(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return NULL;
    }

    return SDL_GameControllerGetSerial(S3_UnwrapGamepad(gamepad));
}

static S3_GamepadType S3_FromSDL2GamepadType(SDL_GameControllerType type, Uint16 vendor, Uint16 product)
{
    switch (type) {
    case SDL_CONTROLLER_TYPE_XBOX360:
        return S3_GAMEPAD_TYPE_XBOX360;
    case SDL_CONTROLLER_TYPE_XBOXONE:
        return S3_GAMEPAD_TYPE_XBOXONE;
    case SDL_CONTROLLER_TYPE_PS3:
        return S3_GAMEPAD_TYPE_PS3;
    case SDL_CONTROLLER_TYPE_PS4:
        return S3_GAMEPAD_TYPE_PS4;
    case SDL_CONTROLLER_TYPE_PS5:
        return S3_GAMEPAD_TYPE_PS5;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT;
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR;
    default:
        break;
    }

    // SDL2 has no GameCube type; SDL3 recognises Nintendo's adapter by VID/PID.
    if (vendor == 0x057E && product == 0x0337) {
        return S3_GAMEPAD_TYPE_GAMECUBE;
    }

    return S3_GAMEPAD_TYPE_STANDARD;
}

S3_GamepadType S3_GetGamepadType(S3_Gamepad* gamepad)
{
    SDL_GameController* native = S3_UnwrapGamepad(gamepad);

    if (native == NULL) {
        S3_InvalidGamepad();
        return S3_GAMEPAD_TYPE_UNKNOWN;
    }

    return S3_FromSDL2GamepadType(SDL_GameControllerGetType(native),
        SDL_GameControllerGetVendor(native),
        SDL_GameControllerGetProduct(native));
}

Uint16 S3_GetGamepadVendor(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return 0;
    }

    return SDL_GameControllerGetVendor(S3_UnwrapGamepad(gamepad));
}

Uint16 S3_GetGamepadProduct(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return 0;
    }

    return SDL_GameControllerGetProduct(S3_UnwrapGamepad(gamepad));
}

const char* S3_GetGamepadNameForID(S3_JoystickID instance_id)
{
    const int index = S3_JoystickDeviceIndex(instance_id);

    return index >= 0 ? SDL_GameControllerNameForIndex(index) : NULL;
}

S3_Gamepad* S3_GetGamepadFromID(S3_JoystickID instance_id)
{
    SDL_GameController* native = SDL_GameControllerFromInstanceID(S3_ToSDL2JoystickID(instance_id));

    if (native == NULL) {
        SDL_SetError("Invalid joystick instance ID");
    }

    return (S3_Gamepad*)native;
}

S3_GUID S3_GetGamepadGUIDForID(S3_JoystickID instance_id)
{
    const int index = S3_JoystickDeviceIndex(instance_id);
    S3_GUID guid;

    SDL_zero(guid);

    if (index >= 0) {
        const SDL_JoystickGUID native = SDL_JoystickGetDeviceGUID(index);
        SDL_memcpy(guid.data, native.data, sizeof(guid.data));
    }

    return guid;
}

int S3_GetGamepadPlayerIndex(S3_Gamepad* gamepad)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return -1;
    }

    return SDL_GameControllerGetPlayerIndex(S3_UnwrapGamepad(gamepad));
}

int S3_GetGamepadPlayerIndexForID(S3_JoystickID instance_id)
{
    SDL_GameController* native = SDL_GameControllerFromInstanceID(S3_ToSDL2JoystickID(instance_id));
    int index;

    // An open gamepad carries the number assigned to it, which the device list does not.
    if (native != NULL) {
        return SDL_GameControllerGetPlayerIndex(native);
    }

    index = S3_JoystickDeviceIndex(instance_id);

    return index < 0 ? -1 : SDL_JoystickGetDevicePlayerIndex(index);
}

bool S3_SetGamepadPlayerIndex(S3_Gamepad* gamepad, int player_index)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    SDL_GameControllerSetPlayerIndex(S3_UnwrapGamepad(gamepad), player_index);

    return true;
}

S3_PowerState S3_GetGamepadPowerInfo(S3_Gamepad* gamepad, int* percent)
{
    if (percent != NULL) {
        *percent = -1;
    }

    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return S3_POWERSTATE_ERROR;
    }

    return S3_GetJoystickPowerInfo(S3_GetGamepadJoystick(gamepad), percent);
}

Sint16 S3_GetGamepadAxis(S3_Gamepad* gamepad, S3_GamepadAxis axis)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return 0;
    }

    if (axis <= S3_GAMEPAD_AXIS_INVALID || axis >= S3_GAMEPAD_AXIS_COUNT) {
        SDL_SetError("Parameter 'axis' is invalid");
        return 0;
    }

    return SDL_GameControllerGetAxis(S3_UnwrapGamepad(gamepad), (SDL_GameControllerAxis)axis);
}

bool S3_GetGamepadButton(S3_Gamepad* gamepad, S3_GamepadButton button)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    if (button <= S3_GAMEPAD_BUTTON_INVALID || button >= S3_GAMEPAD_BUTTON_COUNT) {
        SDL_SetError("Parameter 'button' is invalid");
        return false;
    }

    // MISC2 to MISC6 are SDL3 additions no SDL2 mapping can bind.
    if ((int)button >= S3_SDL2_BUTTON_COUNT) {
        return false;
    }

    return SDL_GameControllerGetButton(S3_UnwrapGamepad(gamepad), (SDL_GameControllerButton)button) != 0;
}
#endif

static const char* const s3_gamepad_button_names[S3_GAMEPAD_BUTTON_COUNT] = {
    "a",
    "b",
    "x",
    "y",
    "back",
    "guide",
    "start",
    "leftstick",
    "rightstick",
    "leftshoulder",
    "rightshoulder",
    "dpup",
    "dpdown",
    "dpleft",
    "dpright",
    "misc1",
    "paddle1",
    "paddle2",
    "paddle3",
    "paddle4",
    "touchpad",
    "misc2",
    "misc3",
    "misc4",
    "misc5",
    "misc6",
};

static const char* const s3_gamepad_axis_names[S3_GAMEPAD_AXIS_COUNT] = {
    "leftx",
    "lefty",
    "rightx",
    "righty",
    "lefttrigger",
    "righttrigger",
};

S3_GamepadButton S3_GetGamepadButtonFromString(const char* str)
{
    int index;

    if (str == NULL || *str == '\0') {
        return S3_GAMEPAD_BUTTON_INVALID;
    }

    for (index = 0; index < S3_GAMEPAD_BUTTON_COUNT; index++) {
        if (SDL_strcasecmp(str, s3_gamepad_button_names[index]) == 0) {
            return (S3_GamepadButton)index;
        }
    }

    return S3_GAMEPAD_BUTTON_INVALID;
}

const char* S3_GetGamepadStringForButton(S3_GamepadButton button)
{
    if (button > S3_GAMEPAD_BUTTON_INVALID && button < S3_GAMEPAD_BUTTON_COUNT) {
        return s3_gamepad_button_names[button];
    }

    return NULL;
}

const char* S3_GetGamepadStringForAxis(S3_GamepadAxis axis)
{
    if (axis > S3_GAMEPAD_AXIS_INVALID && axis < S3_GAMEPAD_AXIS_COUNT) {
        return s3_gamepad_axis_names[axis];
    }

    return NULL;
}

S3_GamepadButtonLabel S3_GetGamepadButtonLabelForType(S3_GamepadType type, S3_GamepadButton button)
{
    static const S3_GamepadButtonLabel abxy[4] = { S3_GAMEPAD_BUTTON_LABEL_A, S3_GAMEPAD_BUTTON_LABEL_B, S3_GAMEPAD_BUTTON_LABEL_X, S3_GAMEPAD_BUTTON_LABEL_Y };
    static const S3_GamepadButtonLabel axby[4] = { S3_GAMEPAD_BUTTON_LABEL_A, S3_GAMEPAD_BUTTON_LABEL_X, S3_GAMEPAD_BUTTON_LABEL_B, S3_GAMEPAD_BUTTON_LABEL_Y };
    static const S3_GamepadButtonLabel bayx[4] = { S3_GAMEPAD_BUTTON_LABEL_B, S3_GAMEPAD_BUTTON_LABEL_A, S3_GAMEPAD_BUTTON_LABEL_Y, S3_GAMEPAD_BUTTON_LABEL_X };
    static const S3_GamepadButtonLabel sony[4] = { S3_GAMEPAD_BUTTON_LABEL_CROSS, S3_GAMEPAD_BUTTON_LABEL_CIRCLE, S3_GAMEPAD_BUTTON_LABEL_SQUARE, S3_GAMEPAD_BUTTON_LABEL_TRIANGLE };

    // Indexed by SOUTH, EAST, WEST, NORTH; SDL3 picks the face style from the gamepad type.
    if (button < S3_GAMEPAD_BUTTON_SOUTH || button > S3_GAMEPAD_BUTTON_NORTH) {
        return S3_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    }

    switch (type) {
    case S3_GAMEPAD_TYPE_PS3:
    case S3_GAMEPAD_TYPE_PS4:
    case S3_GAMEPAD_TYPE_PS5:
        return sony[button];
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return bayx[button];
    case S3_GAMEPAD_TYPE_GAMECUBE:
        return axby[button];
    default:
        return abxy[button];
    }
}

S3_GamepadButtonLabel S3_GetGamepadButtonLabel(S3_Gamepad* gamepad, S3_GamepadButton button)
{
    if (gamepad == NULL) {
        S3_InvalidGamepad();
        return S3_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    }

    return S3_GetGamepadButtonLabelForType(S3_GetGamepadType(gamepad), button);
}

#ifndef __SWITCH__
bool S3_RumbleGamepad(S3_Gamepad* gamepad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerRumble(S3_UnwrapGamepad(gamepad), low_frequency_rumble, high_frequency_rumble, duration_ms) == 0;
}

bool S3_SetGamepadLED(S3_Gamepad* gamepad, Uint8 red, Uint8 green, Uint8 blue)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerSetLED(S3_UnwrapGamepad(gamepad), red, green, blue) == 0;
}

bool S3_SendGamepadEffect(S3_Gamepad* gamepad, const void* data, int size)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerSendEffect(S3_UnwrapGamepad(gamepad), data, size) == 0;
}

bool S3_GamepadHasSensor(S3_Gamepad* gamepad, S3_SensorType type)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerHasSensor(S3_UnwrapGamepad(gamepad), (SDL_SensorType)type) == SDL_TRUE;
}

bool S3_SetGamepadSensorEnabled(S3_Gamepad* gamepad, S3_SensorType type, bool enabled)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerSetSensorEnabled(S3_UnwrapGamepad(gamepad), (SDL_SensorType)type, enabled ? SDL_TRUE : SDL_FALSE) == 0;
}

bool S3_GetGamepadSensorData(S3_Gamepad* gamepad, S3_SensorType type, float* data, int num_values)
{
    if (gamepad == NULL) {
        return S3_InvalidGamepad();
    }

    return SDL_GameControllerGetSensorData(S3_UnwrapGamepad(gamepad), (SDL_SensorType)type, data, num_values) == 0;
}
#endif

static int S3_SensorDeviceIndex(S3_SensorID instance_id)
{
    const int count = SDL_NumSensors();
    int index;

    for (index = 0; index < count; index++) {
        if (S3_FromSDL2JoystickID(SDL_SensorGetDeviceInstanceID(index)) == instance_id) {
            return index;
        }
    }

    SDL_SetError("Invalid sensor instance ID");

    return -1;
}

S3_SensorID* S3_GetSensors(int* count)
{
    const int total = SDL_NumSensors() > 0 ? SDL_NumSensors() : 0;
    S3_SensorID* ids = (S3_SensorID*)SDL_malloc(sizeof(*ids) * ((size_t)total + 1));
    int index;

    if (count != NULL) {
        *count = 0;
    }

    if (ids == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    for (index = 0; index < total; index++) {
        ids[index] = S3_FromSDL2JoystickID(SDL_SensorGetDeviceInstanceID(index));
    }
    ids[total] = 0;

    if (count != NULL) {
        *count = total;
    }

    return ids;
}

S3_SensorType S3_GetSensorTypeForID(S3_SensorID instance_id)
{
    const int index = S3_SensorDeviceIndex(instance_id);

    return index < 0 ? S3_SENSOR_INVALID : (S3_SensorType)SDL_SensorGetDeviceType(index);
}

S3_Sensor* S3_OpenSensor(S3_SensorID instance_id)
{
    const int index = S3_SensorDeviceIndex(instance_id);

    return index < 0 ? NULL : (S3_Sensor*)SDL_SensorOpen(index);
}

bool S3_GetSensorData(S3_Sensor* sensor, float* data, int num_values)
{
    if (sensor == NULL) {
        SDL_SetError("Parameter 'sensor' is invalid");
        return false;
    }

    return SDL_SensorGetData((SDL_Sensor*)sensor, data, num_values) == 0;
}

void S3_CloseSensor(S3_Sensor* sensor)
{
    SDL_SensorClose((SDL_Sensor*)sensor);
}

void S3_UpdateSensors(void)
{
    SDL_SensorUpdate();
}
