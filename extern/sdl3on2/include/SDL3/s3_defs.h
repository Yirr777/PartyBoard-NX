// Everything the layer implements, under the S3_ prefix so it can sit beside SDL2's headers.

#ifndef SDL3ON2_S3_DEFS_H_
#define SDL3ON2_S3_DEFS_H_

// SDL3's stdinc pulls these in, and code written against SDL3 leans on that.
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef S3_CALL
#if defined(_WIN32) && !defined(_WIN64)
#define S3_CALL __cdecl
#else
#define S3_CALL
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define S3_PRINTF_ATTR(fmtarg, firstvararg) __attribute__((format(printf, fmtarg, firstvararg)))
#else
#define S3_PRINTF_ATTR(fmtarg, firstvararg)
#endif

// Types consumers forward-declare by tag keep SDL3's tag outside the implementation.
#ifdef S3_IMPLEMENTATION
#define S3_PUBLIC_TAG(sdl3, internal) internal
#else
#define S3_PUBLIC_TAG(sdl3, internal) sdl3
#endif

// The same typedefs as SDL2's, which C11 allows twice.

typedef int8_t Sint8;
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef int32_t Sint32;
typedef uint32_t Uint32;
typedef int64_t Sint64;
typedef uint64_t Uint64;

extern void* S3_malloc(size_t size);
extern void S3_free(void* mem);

extern int S3_snprintf(char* text, size_t maxlen, const char* fmt, ...) S3_PRINTF_ATTR(3, 4);
extern int S3_vsnprintf(char* text, size_t maxlen, const char* fmt, va_list ap);

extern int S3_strcmp(const char* str1, const char* str2);
extern int S3_strcasecmp(const char* str1, const char* str2);
extern int S3_strncasecmp(const char* str1, const char* str2, size_t maxlen);
extern size_t S3_strlcpy(char* dst, const char* src, size_t maxlen);
extern size_t S3_strlcat(char* dst, const char* src, size_t maxlen);
extern char* S3_strlwr(char* str);
extern char* S3_strupr(char* str);
extern char* S3_strrev(char* str);
extern char* S3_itoa(int value, char* str, int radix);
extern char* S3_lltoa(Sint64 value, char* str, int radix);
extern char* S3_ulltoa(Uint64 value, char* str, int radix);
extern Sint64 S3_strtoll(const char* str, char** endp, int base);

extern int S3_isalpha(int x);
extern int S3_isdigit(int x);
extern int S3_isspace(int x);
extern int S3_toupper(int x);
extern int S3_tolower(int x);

extern Uint32 S3_rand_bits(void);

extern const char* S3_GetError(void);
extern bool S3_SetError(const char* fmt, ...) S3_PRINTF_ATTR(1, 2);
extern void S3_Log(const char* fmt, ...) S3_PRINTF_ATTR(1, 2);

typedef Uint32 S3_InitFlags;

#define S3_INIT_AUDIO 0x00000010u
#define S3_INIT_VIDEO 0x00000020u
#define S3_INIT_JOYSTICK 0x00000200u
#define S3_INIT_HAPTIC 0x00001000u
#define S3_INIT_GAMEPAD 0x00002000u
#define S3_INIT_EVENTS 0x00004000u
#define S3_INIT_SENSOR 0x00008000u
#define S3_INIT_CAMERA 0x00010000u

extern bool S3_Init(S3_InitFlags flags);
extern bool S3_InitSubSystem(S3_InitFlags flags);
extern void S3_QuitSubSystem(S3_InitFlags flags);
extern S3_InitFlags S3_WasInit(S3_InitFlags flags);
extern void S3_Quit(void);
extern bool S3_IsMainThread(void);

#define S3_PROP_APP_METADATA_NAME_STRING "SDL.app.metadata.name"
#define S3_PROP_APP_METADATA_VERSION_STRING "SDL.app.metadata.version"
#define S3_PROP_APP_METADATA_IDENTIFIER_STRING "SDL.app.metadata.identifier"
#define S3_PROP_APP_METADATA_TYPE_STRING "SDL.app.metadata.type"

extern bool S3_SetAppMetadata(const char* appname, const char* appversion, const char* appidentifier);
extern bool S3_SetAppMetadataProperty(const char* name, const char* value);

#define S3_HINT_RENDER_DRIVER "SDL_RENDER_DRIVER"
#define S3_HINT_MOUSE_TOUCH_EVENTS "SDL_MOUSE_TOUCH_EVENTS"
#define S3_HINT_TOUCH_MOUSE_EVENTS "SDL_TOUCH_MOUSE_EVENTS"
#define S3_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"
#define S3_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE "SDL_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE"
#define S3_HINT_NO_SIGNAL_HANDLERS "SDL_NO_SIGNAL_HANDLERS"
#define S3_HINT_ORIENTATIONS "SDL_ORIENTATIONS"
#define S3_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME "SDL_SCREENSAVER_INHIBIT_ACTIVITY_NAME"
#define S3_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"

extern bool S3_SetHint(const char* name, const char* value);

typedef enum S3_ThreadPriority {
    S3_THREAD_PRIORITY_LOW,
    S3_THREAD_PRIORITY_NORMAL,
    S3_THREAD_PRIORITY_HIGH,
    S3_THREAD_PRIORITY_TIME_CRITICAL,
} S3_ThreadPriority;

extern bool S3_SetCurrentThreadPriority(S3_ThreadPriority priority);
extern Uint64 S3_GetTicks(void);
extern Uint64 S3_GetTicksNS(void);
extern Uint64 S3_GetPerformanceCounter(void);
extern Uint64 S3_GetPerformanceFrequency(void);

// Laid out as SDL2's, so they pass between the two unconverted.

typedef struct S3_Rect {
    int x;
    int y;
    int w;
    int h;
} S3_Rect;

typedef struct S3_FPoint {
    float x;
    float y;
} S3_FPoint;

typedef struct S3_FRect {
    float x;
    float y;
    float w;
    float h;
} S3_FRect;

typedef struct S3_Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
} S3_Color;

typedef Uint32 S3_PixelFormat;

#define S3_PIXELFORMAT_UNKNOWN 0u
#define S3_PIXELFORMAT_INDEX8 0x13000801u
#define S3_PIXELFORMAT_RGB24 0x17101803u
#define S3_PIXELFORMAT_XRGB8888 0x16161804u
#define S3_PIXELFORMAT_ARGB8888 0x16362004u
#define S3_PIXELFORMAT_RGBA8888 0x16462004u
#define S3_PIXELFORMAT_ABGR8888 0x16762004u

#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define S3_PIXELFORMAT_RGBA32 S3_PIXELFORMAT_RGBA8888
#else
#define S3_PIXELFORMAT_RGBA32 S3_PIXELFORMAT_ABGR8888
#endif

typedef struct S3_Palette {
    int ncolors;
    S3_Color* colors;
    Uint32 version;
    int refcount;
} S3_Palette;

typedef struct S3_PixelFormatDetails {
    S3_PixelFormat format;
    Uint8 bits_per_pixel;
    Uint8 bytes_per_pixel;
    Uint8 padding[2];
    Uint32 Rmask;
    Uint32 Gmask;
    Uint32 Bmask;
    Uint32 Amask;
    Uint8 Rbits;
    Uint8 Gbits;
    Uint8 Bbits;
    Uint8 Abits;
    Uint8 Rshift;
    Uint8 Gshift;
    Uint8 Bshift;
    Uint8 Ashift;
} S3_PixelFormatDetails;

extern const S3_PixelFormatDetails* S3_GetPixelFormatDetails(S3_PixelFormat format);
extern bool S3_GetMasksForPixelFormat(S3_PixelFormat format, int* bpp, Uint32* rmask, Uint32* gmask, Uint32* bmask, Uint32* amask);
extern S3_PixelFormat S3_GetPixelFormatForMasks(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

// SDL3's layout, with the wrapped SDL2 surface in SDL3's private `reserved` slot.

typedef Uint32 S3_SurfaceFlags;

#define S3_SURFACE_PREALLOCATED 0x00000001u
#define S3_SURFACE_LOCK_NEEDED 0x00000002u
#define S3_SURFACE_LOCKED 0x00000004u
#define S3_SURFACE_SIMD_ALIGNED 0x00000008u

#define S3_MUSTLOCK(S) (((S)->flags & S3_SURFACE_LOCK_NEEDED) == S3_SURFACE_LOCK_NEEDED)

typedef struct S3_Surface {
    S3_SurfaceFlags flags;
    S3_PixelFormat format;
    int w;
    int h;
    int pitch;
    void* pixels;
    int refcount;
    void* reserved;
} S3_Surface;

typedef enum S3_ScaleMode {
    S3_SCALEMODE_INVALID = -1,
    S3_SCALEMODE_NEAREST = 0,
    S3_SCALEMODE_LINEAR = 1,
    S3_SCALEMODE_PIXELART = 2,
} S3_ScaleMode;

extern S3_Surface* S3_CreateSurface(int width, int height, S3_PixelFormat format);
extern S3_Surface* S3_CreateSurfaceFrom(int width, int height, S3_PixelFormat format, void* pixels, int pitch);
extern void S3_DestroySurface(S3_Surface* surface);
extern S3_Surface* S3_ConvertSurface(S3_Surface* surface, S3_PixelFormat format);
extern bool S3_FillSurfaceRect(S3_Surface* dst, const S3_Rect* rect, Uint32 color);
extern bool S3_BlitSurface(S3_Surface* src, const S3_Rect* srcrect, S3_Surface* dst, const S3_Rect* dstrect);
extern bool S3_BlitSurfaceScaled(S3_Surface* src, const S3_Rect* srcrect, S3_Surface* dst, const S3_Rect* dstrect, S3_ScaleMode scale_mode);
extern bool S3_SetSurfaceColorKey(S3_Surface* surface, bool enabled, Uint32 key);
extern bool S3_LockSurface(S3_Surface* surface);
extern void S3_UnlockSurface(S3_Surface* surface);
extern S3_Palette* S3_GetSurfacePalette(S3_Surface* surface);

// Wraps an SDL2 SDL_RWops and adds SDL3's per-stream status.

typedef struct S3_IOStream S3_IOStream;

typedef enum S3_IOStatus {
    S3_IO_STATUS_READY,
    S3_IO_STATUS_ERROR,
    S3_IO_STATUS_EOF,
    S3_IO_STATUS_NOT_READY,
    S3_IO_STATUS_READONLY,
    S3_IO_STATUS_WRITEONLY,
} S3_IOStatus;

typedef enum S3_IOWhence {
    S3_IO_SEEK_SET,
    S3_IO_SEEK_CUR,
    S3_IO_SEEK_END,
} S3_IOWhence;

typedef struct S3_IOStreamInterface {
    Uint32 version;
    Sint64(S3_CALL* size)(void* userdata);
    Sint64(S3_CALL* seek)(void* userdata, Sint64 offset, S3_IOWhence whence);
    size_t(S3_CALL* read)(void* userdata, void* ptr, size_t size, S3_IOStatus* status);
    size_t(S3_CALL* write)(void* userdata, const void* ptr, size_t size, S3_IOStatus* status);
    bool(S3_CALL* flush)(void* userdata, S3_IOStatus* status);
    bool(S3_CALL* close)(void* userdata);
} S3_IOStreamInterface;

extern S3_IOStream* S3_OpenIO(const S3_IOStreamInterface* iface, void* userdata);
extern S3_IOStream* S3_IOFromFile(const char* file, const char* mode);
extern S3_IOStream* S3_IOFromConstMem(const void* mem, size_t size);
extern bool S3_CloseIO(S3_IOStream* context);
extern S3_IOStatus S3_GetIOStatus(S3_IOStream* context);
extern Sint64 S3_GetIOSize(S3_IOStream* context);
extern Sint64 S3_SeekIO(S3_IOStream* context, Sint64 offset, S3_IOWhence whence);
extern Sint64 S3_TellIO(S3_IOStream* context);
extern size_t S3_ReadIO(S3_IOStream* context, void* ptr, size_t size);
extern size_t S3_WriteIO(S3_IOStream* context, const void* ptr, size_t size);
extern bool S3_FlushIO(S3_IOStream* context);
extern bool S3_ReadU8(S3_IOStream* src, Uint8* value);
extern bool S3_ReadU16LE(S3_IOStream* src, Uint16* value);
extern bool S3_ReadU32LE(S3_IOStream* src, Uint32* value);
extern bool S3_WriteU8(S3_IOStream* dst, Uint8 value);
extern bool S3_WriteU16LE(S3_IOStream* dst, Uint16 value);
extern bool S3_WriteU32LE(S3_IOStream* dst, Uint32 value);
extern bool S3_WriteS32LE(S3_IOStream* dst, Sint32 value);
extern S3_Surface* S3_LoadBMP_IO(S3_IOStream* src, bool closeio);
extern bool S3_SaveBMP_IO(S3_Surface* surface, S3_IOStream* dst, bool closeio);
extern S3_Surface* S3_LoadPNG(const char* file);

typedef Sint64 S3_Time;

typedef enum S3_PathType {
    S3_PATHTYPE_NONE,
    S3_PATHTYPE_FILE,
    S3_PATHTYPE_DIRECTORY,
    S3_PATHTYPE_OTHER,
} S3_PathType;

typedef struct S3_PathInfo {
    S3_PathType type;
    Uint64 size;
    S3_Time create_time;
    S3_Time modify_time;
    S3_Time access_time;
} S3_PathInfo;

typedef Uint32 S3_GlobFlags;

#define S3_GLOB_CASEINSENSITIVE (1u << 0)

typedef enum S3_Folder {
    S3_FOLDER_HOME,
    S3_FOLDER_DESKTOP,
    S3_FOLDER_DOCUMENTS,
    S3_FOLDER_DOWNLOADS,
    S3_FOLDER_MUSIC,
    S3_FOLDER_PICTURES,
    S3_FOLDER_PUBLICSHARE,
    S3_FOLDER_SAVEDGAMES,
    S3_FOLDER_SCREENSHOTS,
    S3_FOLDER_TEMPLATES,
    S3_FOLDER_VIDEOS,
    S3_FOLDER_COUNT,
} S3_Folder;

extern bool S3_GetPathInfo(const char* path, S3_PathInfo* info);
extern char** S3_GlobDirectory(const char* path, const char* pattern, S3_GlobFlags flags, int* count);
extern bool S3_CreateDirectory(const char* path);
extern bool S3_RemovePath(const char* path);
extern bool S3_RenamePath(const char* oldpath, const char* newpath);
extern const char* S3_GetBasePath(void);
extern char* S3_GetPrefPath(const char* org, const char* app);
extern const char* S3_GetUserFolder(S3_Folder folder);

typedef enum S3_DateFormat {
    S3_DATE_FORMAT_YYYYMMDD = 0,
    S3_DATE_FORMAT_DDMMYYYY = 1,
    S3_DATE_FORMAT_MMDDYYYY = 2,
} S3_DateFormat;

typedef enum S3_TimeFormat {
    S3_TIME_FORMAT_24HR = 0,
    S3_TIME_FORMAT_12HR = 1,
} S3_TimeFormat;

extern bool S3_GetDateTimeLocalePreferences(S3_DateFormat* date_format, S3_TimeFormat* time_format);

typedef Uint32 S3_PropertiesID;

#define S3_PROP_RENDERER_NAME_STRING "SDL.renderer.name"
#define S3_PROP_RENDERER_CREATE_WINDOW_POINTER "SDL.renderer.create.window"
#define S3_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER "SDL.renderer.create.present_vsync"
#define S3_PROP_TEXTURE_FORMAT_NUMBER "SDL.texture.format"
#define S3_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN "SDL.window.create.external_graphics_context"
#define S3_PROP_WINDOW_CREATE_FLAGS_NUMBER "SDL.window.create.flags"
#define S3_PROP_WINDOW_CREATE_HEIGHT_NUMBER "SDL.window.create.height"
#define S3_PROP_WINDOW_CREATE_TITLE_STRING "SDL.window.create.title"
#define S3_PROP_WINDOW_CREATE_WIDTH_NUMBER "SDL.window.create.width"
#define S3_PROP_WINDOW_CREATE_X_NUMBER "SDL.window.create.x"
#define S3_PROP_WINDOW_CREATE_Y_NUMBER "SDL.window.create.y"
#define S3_PROP_WINDOW_ANDROID_WINDOW_POINTER "SDL.window.android.window"
#define S3_PROP_WINDOW_WAYLAND_DISPLAY_POINTER "SDL.window.wayland.display"
#define S3_PROP_WINDOW_WAYLAND_SURFACE_POINTER "SDL.window.wayland.surface"
#define S3_PROP_WINDOW_WIN32_HWND_POINTER "SDL.window.win32.hwnd"
#define S3_PROP_WINDOW_WIN32_INSTANCE_POINTER "SDL.window.win32.instance"
#define S3_PROP_WINDOW_WINRT_WINDOW_POINTER "SDL.window.winrt.window"
#define S3_PROP_WINDOW_X11_DISPLAY_POINTER "SDL.window.x11.display"
#define S3_PROP_WINDOW_X11_WINDOW_NUMBER "SDL.window.x11.window"
#define S3_PROP_JOYSTICK_CAP_MONO_LED_BOOLEAN "SDL.joystick.cap.mono_led"
#define S3_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN "SDL.joystick.cap.rgb_led"
#define S3_PROP_JOYSTICK_CAP_PLAYER_LED_BOOLEAN "SDL.joystick.cap.player_led"
#define S3_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN "SDL.joystick.cap.rumble"
#define S3_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN "SDL.joystick.cap.trigger_rumble"
#define S3_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN S3_PROP_JOYSTICK_CAP_MONO_LED_BOOLEAN
#define S3_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN S3_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN
#define S3_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN S3_PROP_JOYSTICK_CAP_PLAYER_LED_BOOLEAN
#define S3_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN S3_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN
#define S3_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN S3_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN

extern S3_PropertiesID S3_CreateProperties(void);
extern void S3_DestroyProperties(S3_PropertiesID props);
extern bool S3_SetNumberProperty(S3_PropertiesID props, const char* name, Sint64 value);
extern Sint64 S3_GetNumberProperty(S3_PropertiesID props, const char* name, Sint64 default_value);
extern bool S3_SetStringProperty(S3_PropertiesID props, const char* name, const char* value);
extern const char* S3_GetStringProperty(S3_PropertiesID props, const char* name, const char* default_value);
extern bool S3_SetPointerProperty(S3_PropertiesID props, const char* name, void* value);
extern void* S3_GetPointerProperty(S3_PropertiesID props, const char* name, void* default_value);
extern bool S3_SetBooleanProperty(S3_PropertiesID props, const char* name, bool value);
extern bool S3_GetBooleanProperty(S3_PropertiesID props, const char* name, bool default_value);

// SDL2's own objects, except on Switch, where the layer creates the window.

typedef struct SDL_Window S3_Window;
typedef struct SDL_Renderer S3_Renderer;
typedef struct SDL_Texture S3_Texture;

typedef Uint32 S3_DisplayID;
typedef Uint32 S3_WindowID;
typedef Uint64 S3_WindowFlags;

#define S3_WINDOW_FULLSCREEN 0x0000000000000001ULL
#define S3_WINDOW_OPENGL 0x0000000000000002ULL
#define S3_WINDOW_OCCLUDED 0x0000000000000004ULL
#define S3_WINDOW_HIDDEN 0x0000000000000008ULL
#define S3_WINDOW_BORDERLESS 0x0000000000000010ULL
#define S3_WINDOW_RESIZABLE 0x0000000000000020ULL
#define S3_WINDOW_MINIMIZED 0x0000000000000040ULL
#define S3_WINDOW_MAXIMIZED 0x0000000000000080ULL
#define S3_WINDOW_MOUSE_GRABBED 0x0000000000000100ULL
#define S3_WINDOW_INPUT_FOCUS 0x0000000000000200ULL
#define S3_WINDOW_MOUSE_FOCUS 0x0000000000000400ULL
#define S3_WINDOW_EXTERNAL 0x0000000000000800ULL
#define S3_WINDOW_HIGH_PIXEL_DENSITY 0x0000000000002000ULL
#define S3_WINDOW_MOUSE_CAPTURE 0x0000000000004000ULL
#define S3_WINDOW_ALWAYS_ON_TOP 0x0000000000010000ULL
#define S3_WINDOW_UTILITY 0x0000000000020000ULL
#define S3_WINDOW_TOOLTIP 0x0000000000040000ULL
#define S3_WINDOW_POPUP_MENU 0x0000000000080000ULL
#define S3_WINDOW_KEYBOARD_GRABBED 0x0000000000100000ULL
#define S3_WINDOW_VULKAN 0x0000000010000000ULL
#define S3_WINDOW_METAL 0x0000000020000000ULL

#define S3_WINDOWPOS_UNDEFINED_MASK 0x1FFF0000u
#define S3_WINDOWPOS_UNDEFINED_DISPLAY(X) (S3_WINDOWPOS_UNDEFINED_MASK | (X))
#define S3_WINDOWPOS_UNDEFINED S3_WINDOWPOS_UNDEFINED_DISPLAY(0)
#define S3_WINDOWPOS_ISUNDEFINED(X) (((X) & 0xFFFF0000) == S3_WINDOWPOS_UNDEFINED_MASK)
#define S3_WINDOWPOS_CENTERED_MASK 0x2FFF0000u
#define S3_WINDOWPOS_CENTERED_DISPLAY(X) (S3_WINDOWPOS_CENTERED_MASK | (X))
#define S3_WINDOWPOS_CENTERED S3_WINDOWPOS_CENTERED_DISPLAY(0)
#define S3_WINDOWPOS_ISCENTERED(X) (((X) & 0xFFFF0000) == S3_WINDOWPOS_CENTERED_MASK)

typedef enum S3_DisplayOrientation {
    S3_ORIENTATION_UNKNOWN,
    S3_ORIENTATION_LANDSCAPE,
    S3_ORIENTATION_LANDSCAPE_FLIPPED,
    S3_ORIENTATION_PORTRAIT,
    S3_ORIENTATION_PORTRAIT_FLIPPED,
} S3_DisplayOrientation;

typedef struct S3_DisplayMode {
    S3_DisplayID displayID;
    S3_PixelFormat format;
    int w;
    int h;
    float pixel_density;
    float refresh_rate;
    int refresh_rate_numerator;
    int refresh_rate_denominator;
    void* internal;
} S3_DisplayMode;

extern S3_DisplayID* S3_GetDisplays(int* count);
extern S3_DisplayID S3_GetPrimaryDisplay(void);
extern S3_DisplayID S3_GetDisplayForWindow(S3_Window* window);
extern float S3_GetDisplayContentScale(S3_DisplayID display_id);
extern bool S3_GetDisplayBounds(S3_DisplayID display_id, S3_Rect* rect);
extern bool S3_GetDisplayUsableBounds(S3_DisplayID display_id, S3_Rect* rect);
extern const S3_DisplayMode* S3_GetCurrentDisplayMode(S3_DisplayID display_id);
extern const S3_DisplayMode* S3_GetDesktopDisplayMode(S3_DisplayID display_id);
extern S3_DisplayOrientation S3_GetNaturalDisplayOrientation(S3_DisplayID display_id);
extern S3_DisplayOrientation S3_GetCurrentDisplayOrientation(S3_DisplayID display_id);
extern const char* S3_GetCurrentVideoDriver(void);
extern bool S3_DisableScreenSaver(void);
extern bool S3_EnableScreenSaver(void);

extern S3_Window* S3_CreateWindow(const char* title, int w, int h, S3_WindowFlags flags);
extern S3_Window* S3_CreateWindowWithProperties(S3_PropertiesID props);
extern bool S3_CreateWindowAndRenderer(const char* title, int width, int height, S3_WindowFlags window_flags, S3_Window** window, S3_Renderer** renderer);
extern void S3_DestroyWindow(S3_Window* window);
extern S3_WindowID S3_GetWindowID(S3_Window* window);
extern S3_Window* S3_GetWindowFromID(S3_WindowID id);
extern S3_PropertiesID S3_GetWindowProperties(S3_Window* window);
extern S3_WindowFlags S3_GetWindowFlags(S3_Window* window);
extern bool S3_GetWindowSize(S3_Window* window, int* w, int* h);
extern bool S3_GetWindowSizeInPixels(S3_Window* window, int* w, int* h);
extern float S3_GetWindowPixelDensity(S3_Window* window);
extern float S3_GetWindowDisplayScale(S3_Window* window);
extern bool S3_SetWindowTitle(S3_Window* window, const char* title);
extern bool S3_SetWindowIcon(S3_Window* window, S3_Surface* icon);
extern bool S3_SetWindowPosition(S3_Window* window, int x, int y);
extern bool S3_SetWindowSize(S3_Window* window, int w, int h);
extern bool S3_SetWindowMinimumSize(S3_Window* window, int min_w, int min_h);
extern bool S3_SetWindowFullscreen(S3_Window* window, bool fullscreen);
extern bool S3_ShowWindow(S3_Window* window);
extern bool S3_HideWindow(S3_Window* window);
extern bool S3_RaiseWindow(S3_Window* window);
extern bool S3_RestoreWindow(S3_Window* window);
extern bool S3_HideCursor(void);
extern bool S3_ShowCursor(void);
extern bool S3_SetWindowRelativeMouseMode(S3_Window* window, bool enabled);

typedef Uint32 S3_BlendMode;

#define S3_BLENDMODE_NONE 0x00000000u
#define S3_BLENDMODE_BLEND 0x00000001u
#define S3_BLENDMODE_ADD 0x00000002u
#define S3_BLENDMODE_MOD 0x00000004u
#define S3_BLENDMODE_MUL 0x00000008u
#define S3_BLENDMODE_BLEND_PREMULTIPLIED 0x00000010u
#define S3_BLENDMODE_ADD_PREMULTIPLIED 0x00000020u
#define S3_BLENDMODE_INVALID 0x7FFFFFFFu

#define S3_RENDERER_VSYNC_DISABLED 0
#define S3_RENDERER_VSYNC_ADAPTIVE (-1)

typedef enum S3_TextureAccess {
    S3_TEXTUREACCESS_STATIC,
    S3_TEXTUREACCESS_STREAMING,
    S3_TEXTUREACCESS_TARGET,
} S3_TextureAccess;

typedef enum S3_RendererLogicalPresentation {
    S3_LOGICAL_PRESENTATION_DISABLED,
    S3_LOGICAL_PRESENTATION_STRETCH,
    S3_LOGICAL_PRESENTATION_LETTERBOX,
    S3_LOGICAL_PRESENTATION_OVERSCAN,
    S3_LOGICAL_PRESENTATION_INTEGER_SCALE,
} S3_RendererLogicalPresentation;

extern S3_Renderer* S3_CreateRendererWithProperties(S3_PropertiesID props);
extern void S3_DestroyRenderer(S3_Renderer* renderer);
extern bool S3_SetRenderVSync(S3_Renderer* renderer, int vsync);
extern bool S3_SetRenderLogicalPresentation(S3_Renderer* renderer, int w, int h, S3_RendererLogicalPresentation mode);
extern bool S3_SetRenderScale(S3_Renderer* renderer, float scaleX, float scaleY);
extern S3_Texture* S3_CreateTexture(S3_Renderer* renderer, S3_PixelFormat format, int access, int w, int h);
extern void S3_DestroyTexture(S3_Texture* texture);
extern bool S3_UpdateTexture(S3_Texture* texture, const S3_Rect* rect, const void* pixels, int pitch);
extern bool S3_RenderClear(S3_Renderer* renderer);
extern bool S3_RenderTexture(S3_Renderer* renderer, S3_Texture* texture, const S3_FRect* srcrect, const S3_FRect* dstrect);
extern bool S3_RenderFillRect(S3_Renderer* renderer, const S3_FRect* rect);
extern bool S3_RenderPresent(S3_Renderer* renderer);
extern bool S3_SetRenderDrawColor(S3_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
extern bool S3_SetRenderDrawBlendMode(S3_Renderer* renderer, S3_BlendMode blend_mode);
extern bool S3_GetRenderDrawBlendMode(S3_Renderer* renderer, S3_BlendMode* blend_mode);
extern bool S3_RenderDebugTextFormat(S3_Renderer* renderer, float x, float y, const char* fmt, ...) S3_PRINTF_ATTR(4, 5);
extern S3_PropertiesID S3_GetRendererProperties(S3_Renderer* renderer);
extern S3_PropertiesID S3_GetTextureProperties(S3_Texture* texture);

// Scancode and keymod values match SDL2's in the shared range and pass through.

typedef int S3_Scancode;
typedef Uint32 S3_Keycode;
typedef Uint16 S3_Keymod;

extern const bool* S3_GetKeyboardState(int* numkeys);
extern S3_Keymod S3_GetModState(void);
extern S3_Scancode S3_GetScancodeFromName(const char* name);
extern const char* S3_GetScancodeName(S3_Scancode scancode);
extern S3_Keycode S3_GetKeyFromScancode(S3_Scancode scancode, S3_Keymod modstate, bool key_event);
extern const char* S3_GetKeyName(S3_Keycode key);
extern bool S3_StartTextInput(S3_Window* window);
extern bool S3_StopTextInput(S3_Window* window);

typedef Uint32 S3_MouseButtonFlags;

extern S3_MouseButtonFlags S3_GetMouseState(float* x, float* y);
extern S3_MouseButtonFlags S3_GetGlobalMouseState(float* x, float* y);
extern S3_MouseButtonFlags S3_GetRelativeMouseState(float* x, float* y);

// Instance ids are SDL2's plus one, so 0 stays invalid as in SDL3.

typedef struct S3_PUBLIC_TAG(SDL_Joystick, S3_Joystick) S3_Joystick;
typedef struct S3_PUBLIC_TAG(SDL_Gamepad, S3_Gamepad) S3_Gamepad;
typedef struct S3_PUBLIC_TAG(SDL_Sensor, S3_Sensor) S3_Sensor;

typedef Uint32 S3_JoystickID;
typedef Uint32 S3_SensorID;

typedef struct S3_GUID {
    Uint8 data[16];
} S3_GUID;

#define S3_JOYSTICK_AXIS_MAX 32767
#define S3_JOYSTICK_AXIS_MIN -32768

typedef enum S3_JoystickType {
    S3_JOYSTICK_TYPE_UNKNOWN,
    S3_JOYSTICK_TYPE_GAMEPAD,
    S3_JOYSTICK_TYPE_WHEEL,
    S3_JOYSTICK_TYPE_ARCADE_STICK,
    S3_JOYSTICK_TYPE_FLIGHT_STICK,
    S3_JOYSTICK_TYPE_DANCE_PAD,
    S3_JOYSTICK_TYPE_GUITAR,
    S3_JOYSTICK_TYPE_DRUM_KIT,
    S3_JOYSTICK_TYPE_ARCADE_PAD,
    S3_JOYSTICK_TYPE_THROTTLE,
    S3_JOYSTICK_TYPE_COUNT,
} S3_JoystickType;

typedef enum S3_PowerState {
    S3_POWERSTATE_ERROR = -1,
    S3_POWERSTATE_UNKNOWN,
    S3_POWERSTATE_ON_BATTERY,
    S3_POWERSTATE_NO_BATTERY,
    S3_POWERSTATE_CHARGING,
    S3_POWERSTATE_CHARGED,
} S3_PowerState;

typedef enum S3_SensorType {
    S3_SENSOR_INVALID = -1,
    S3_SENSOR_UNKNOWN,
    S3_SENSOR_ACCEL,
    S3_SENSOR_GYRO,
    S3_SENSOR_ACCEL_L,
    S3_SENSOR_GYRO_L,
    S3_SENSOR_ACCEL_R,
    S3_SENSOR_GYRO_R,
    S3_SENSOR_COUNT,
} S3_SensorType;

typedef enum S3_GamepadType {
    S3_GAMEPAD_TYPE_UNKNOWN = 0,
    S3_GAMEPAD_TYPE_STANDARD,
    S3_GAMEPAD_TYPE_XBOX360,
    S3_GAMEPAD_TYPE_XBOXONE,
    S3_GAMEPAD_TYPE_PS3,
    S3_GAMEPAD_TYPE_PS4,
    S3_GAMEPAD_TYPE_PS5,
    S3_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,
    S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,
    S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT,
    S3_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR,
    S3_GAMEPAD_TYPE_GAMECUBE,
    S3_GAMEPAD_TYPE_COUNT,
} S3_GamepadType;

typedef enum S3_GamepadButton {
    S3_GAMEPAD_BUTTON_INVALID = -1,
    S3_GAMEPAD_BUTTON_SOUTH,
    S3_GAMEPAD_BUTTON_EAST,
    S3_GAMEPAD_BUTTON_WEST,
    S3_GAMEPAD_BUTTON_NORTH,
    S3_GAMEPAD_BUTTON_BACK,
    S3_GAMEPAD_BUTTON_GUIDE,
    S3_GAMEPAD_BUTTON_START,
    S3_GAMEPAD_BUTTON_LEFT_STICK,
    S3_GAMEPAD_BUTTON_RIGHT_STICK,
    S3_GAMEPAD_BUTTON_LEFT_SHOULDER,
    S3_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    S3_GAMEPAD_BUTTON_DPAD_UP,
    S3_GAMEPAD_BUTTON_DPAD_DOWN,
    S3_GAMEPAD_BUTTON_DPAD_LEFT,
    S3_GAMEPAD_BUTTON_DPAD_RIGHT,
    S3_GAMEPAD_BUTTON_MISC1,
    S3_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    S3_GAMEPAD_BUTTON_LEFT_PADDLE1,
    S3_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    S3_GAMEPAD_BUTTON_LEFT_PADDLE2,
    S3_GAMEPAD_BUTTON_TOUCHPAD,
    S3_GAMEPAD_BUTTON_MISC2,
    S3_GAMEPAD_BUTTON_MISC3,
    S3_GAMEPAD_BUTTON_MISC4,
    S3_GAMEPAD_BUTTON_MISC5,
    S3_GAMEPAD_BUTTON_MISC6,
    S3_GAMEPAD_BUTTON_COUNT,
} S3_GamepadButton;

typedef enum S3_GamepadButtonLabel {
    S3_GAMEPAD_BUTTON_LABEL_UNKNOWN,
    S3_GAMEPAD_BUTTON_LABEL_A,
    S3_GAMEPAD_BUTTON_LABEL_B,
    S3_GAMEPAD_BUTTON_LABEL_X,
    S3_GAMEPAD_BUTTON_LABEL_Y,
    S3_GAMEPAD_BUTTON_LABEL_CROSS,
    S3_GAMEPAD_BUTTON_LABEL_CIRCLE,
    S3_GAMEPAD_BUTTON_LABEL_SQUARE,
    S3_GAMEPAD_BUTTON_LABEL_TRIANGLE,
} S3_GamepadButtonLabel;

typedef enum S3_GamepadAxis {
    S3_GAMEPAD_AXIS_INVALID = -1,
    S3_GAMEPAD_AXIS_LEFTX,
    S3_GAMEPAD_AXIS_LEFTY,
    S3_GAMEPAD_AXIS_RIGHTX,
    S3_GAMEPAD_AXIS_RIGHTY,
    S3_GAMEPAD_AXIS_LEFT_TRIGGER,
    S3_GAMEPAD_AXIS_RIGHT_TRIGGER,
    S3_GAMEPAD_AXIS_COUNT,
} S3_GamepadAxis;

typedef struct S3_VirtualJoystickTouchpadDesc {
    Uint16 nfingers;
    Uint16 padding[3];
} S3_VirtualJoystickTouchpadDesc;

typedef struct S3_VirtualJoystickSensorDesc {
    S3_SensorType type;
    float rate;
} S3_VirtualJoystickSensorDesc;

typedef struct S3_VirtualJoystickDesc {
    Uint32 version;
    Uint16 type;
    Uint16 padding;
    Uint16 vendor_id;
    Uint16 product_id;
    Uint16 naxes;
    Uint16 nbuttons;
    Uint16 nballs;
    Uint16 nhats;
    Uint16 ntouchpads;
    Uint16 nsensors;
    Uint16 padding2[2];
    Uint32 button_mask;
    Uint32 axis_mask;
    const char* name;
    const S3_VirtualJoystickTouchpadDesc* touchpads;
    const S3_VirtualJoystickSensorDesc* sensors;
    void* userdata;
    void(S3_CALL* Update)(void* userdata);
    void(S3_CALL* SetPlayerIndex)(void* userdata, int player_index);
    bool(S3_CALL* Rumble)(void* userdata, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble);
    bool(S3_CALL* RumbleTriggers)(void* userdata, Uint16 left_rumble, Uint16 right_rumble);
    bool(S3_CALL* SetLED)(void* userdata, Uint8 red, Uint8 green, Uint8 blue);
    bool(S3_CALL* SendEffect)(void* userdata, const void* data, int size);
    bool(S3_CALL* SetSensorsEnabled)(void* userdata, bool enabled);
    void(S3_CALL* Cleanup)(void* userdata);
} S3_VirtualJoystickDesc;

extern void S3_GUIDToString(S3_GUID guid, char* pszGUID, int cbGUID);
extern S3_GUID S3_StringToGUID(const char* pchGUID);
extern void S3_GetJoystickGUIDInfo(S3_GUID guid, Uint16* vendor, Uint16* product, Uint16* version, Uint16* crc16);

extern void S3_LockJoysticks(void);
extern void S3_UnlockJoysticks(void);
extern S3_Joystick* S3_OpenJoystick(S3_JoystickID instance_id);
extern S3_JoystickID S3_GetJoystickID(S3_Joystick* joystick);
extern S3_PowerState S3_GetJoystickPowerInfo(S3_Joystick* joystick, int* percent);
extern S3_JoystickID S3_AttachVirtualJoystick(const S3_VirtualJoystickDesc* desc);
extern bool S3_SetJoystickVirtualButton(S3_Joystick* joystick, int button, bool down);

extern S3_Gamepad* S3_OpenGamepad(S3_JoystickID instance_id);
extern void S3_CloseGamepad(S3_Gamepad* gamepad);
extern S3_JoystickID S3_GetGamepadID(S3_Gamepad* gamepad);
extern S3_Joystick* S3_GetGamepadJoystick(S3_Gamepad* gamepad);
extern S3_PropertiesID S3_GetGamepadProperties(S3_Gamepad* gamepad);
extern const char* S3_GetGamepadName(S3_Gamepad* gamepad);
extern const char* S3_GetGamepadNameForID(S3_JoystickID instance_id);
extern S3_Gamepad* S3_GetGamepadFromID(S3_JoystickID instance_id);
extern const char* S3_GetGamepadSerial(S3_Gamepad* gamepad);
extern S3_GamepadType S3_GetGamepadType(S3_Gamepad* gamepad);
extern Uint16 S3_GetGamepadVendor(S3_Gamepad* gamepad);
extern Uint16 S3_GetGamepadProduct(S3_Gamepad* gamepad);
extern S3_GUID S3_GetGamepadGUIDForID(S3_JoystickID instance_id);
extern int S3_GetGamepadPlayerIndex(S3_Gamepad* gamepad);
extern int S3_GetGamepadPlayerIndexForID(S3_JoystickID instance_id);
extern bool S3_SetGamepadPlayerIndex(S3_Gamepad* gamepad, int player_index);
extern S3_PowerState S3_GetGamepadPowerInfo(S3_Gamepad* gamepad, int* percent);
extern Sint16 S3_GetGamepadAxis(S3_Gamepad* gamepad, S3_GamepadAxis axis);
extern bool S3_GetGamepadButton(S3_Gamepad* gamepad, S3_GamepadButton button);
extern S3_GamepadButton S3_GetGamepadButtonFromString(const char* str);
extern const char* S3_GetGamepadStringForButton(S3_GamepadButton button);
extern const char* S3_GetGamepadStringForAxis(S3_GamepadAxis axis);
extern S3_GamepadButtonLabel S3_GetGamepadButtonLabel(S3_Gamepad* gamepad, S3_GamepadButton button);
extern S3_GamepadButtonLabel S3_GetGamepadButtonLabelForType(S3_GamepadType type, S3_GamepadButton button);
extern bool S3_RumbleGamepad(S3_Gamepad* gamepad, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms);
extern bool S3_SetGamepadLED(S3_Gamepad* gamepad, Uint8 red, Uint8 green, Uint8 blue);
extern bool S3_SendGamepadEffect(S3_Gamepad* gamepad, const void* data, int size);
extern bool S3_GamepadHasSensor(S3_Gamepad* gamepad, S3_SensorType type);
extern bool S3_SetGamepadSensorEnabled(S3_Gamepad* gamepad, S3_SensorType type, bool enabled);
extern bool S3_GetGamepadSensorData(S3_Gamepad* gamepad, S3_SensorType type, float* data, int num_values);

extern S3_SensorID* S3_GetSensors(int* count);
extern S3_SensorType S3_GetSensorTypeForID(S3_SensorID instance_id);
extern S3_Sensor* S3_OpenSensor(S3_SensorID instance_id);
extern bool S3_GetSensorData(S3_Sensor* sensor, float* data, int num_values);
extern void S3_CloseSensor(S3_Sensor* sensor);
extern void S3_UpdateSensors(void);

// SDL3's layout, translated from SDL2's when polled.

typedef Uint32 S3_EventType;

#define S3_EVENT_FIRST 0u
#define S3_EVENT_QUIT 0x100u
#define S3_EVENT_WILL_ENTER_BACKGROUND 0x103u
#define S3_EVENT_DID_ENTER_BACKGROUND 0x104u
#define S3_EVENT_WILL_ENTER_FOREGROUND 0x105u
#define S3_EVENT_DID_ENTER_FOREGROUND 0x106u
#define S3_EVENT_DISPLAY_CURRENT_MODE_CHANGED 0x156u
#define S3_EVENT_WINDOW_SHOWN 0x202u
#define S3_EVENT_WINDOW_HIDDEN 0x203u
#define S3_EVENT_WINDOW_EXPOSED 0x204u
#define S3_EVENT_WINDOW_MOVED 0x205u
#define S3_EVENT_WINDOW_RESIZED 0x206u
#define S3_EVENT_WINDOW_PIXEL_SIZE_CHANGED 0x207u
#define S3_EVENT_WINDOW_MINIMIZED 0x209u
#define S3_EVENT_WINDOW_MAXIMIZED 0x20Au
#define S3_EVENT_WINDOW_RESTORED 0x20Bu
#define S3_EVENT_WINDOW_MOUSE_ENTER 0x20Cu
#define S3_EVENT_WINDOW_MOUSE_LEAVE 0x20Du
#define S3_EVENT_WINDOW_FOCUS_GAINED 0x20Eu
#define S3_EVENT_WINDOW_FOCUS_LOST 0x20Fu
#define S3_EVENT_WINDOW_CLOSE_REQUESTED 0x210u
#define S3_EVENT_WINDOW_DISPLAY_CHANGED 0x213u
#define S3_EVENT_WINDOW_DISPLAY_SCALE_CHANGED 0x214u
#define S3_EVENT_WINDOW_FIRST 0x202u
#define S3_EVENT_WINDOW_LAST 0x21Au
#define S3_EVENT_KEY_DOWN 0x300u
#define S3_EVENT_KEY_UP 0x301u
#define S3_EVENT_TEXT_EDITING 0x302u
#define S3_EVENT_TEXT_INPUT 0x303u
#define S3_EVENT_MOUSE_MOTION 0x400u
#define S3_EVENT_MOUSE_BUTTON_DOWN 0x401u
#define S3_EVENT_MOUSE_BUTTON_UP 0x402u
#define S3_EVENT_MOUSE_WHEEL 0x403u
#define S3_EVENT_GAMEPAD_AXIS_MOTION 0x650u
#define S3_EVENT_GAMEPAD_BUTTON_DOWN 0x651u
#define S3_EVENT_GAMEPAD_BUTTON_UP 0x652u
#define S3_EVENT_GAMEPAD_ADDED 0x653u
#define S3_EVENT_GAMEPAD_REMOVED 0x654u
#define S3_EVENT_GAMEPAD_REMAPPED 0x655u
#define S3_EVENT_FINGER_DOWN 0x700u
#define S3_EVENT_FINGER_UP 0x701u
#define S3_EVENT_FINGER_MOTION 0x702u
#define S3_EVENT_FINGER_CANCELED 0x703u
#define S3_EVENT_DROP_FILE 0x1000u
#define S3_EVENT_DROP_TEXT 0x1001u
#define S3_EVENT_DROP_BEGIN 0x1002u
#define S3_EVENT_DROP_COMPLETE 0x1003u
#define S3_EVENT_DROP_POSITION 0x1004u
#define S3_EVENT_USER 0x8000u
#define S3_EVENT_LAST 0xFFFFu

typedef struct S3_CommonEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
} S3_CommonEvent;

typedef struct S3_WindowEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Sint32 data1;
    Sint32 data2;
} S3_WindowEvent;

typedef struct S3_KeyboardEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Uint32 which;
    S3_Scancode scancode;
    S3_Keycode key;
    S3_Keymod mod;
    Uint16 raw;
    bool down;
    bool repeat;
} S3_KeyboardEvent;

typedef struct S3_TextInputEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    const char* text;
} S3_TextInputEvent;

typedef struct S3_TextEditingEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    const char* text;
    Sint32 start;
    Sint32 length;
} S3_TextEditingEvent;

typedef struct S3_MouseMotionEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Uint32 which;
    Uint32 state;
    float x;
    float y;
    float xrel;
    float yrel;
} S3_MouseMotionEvent;

typedef struct S3_MouseButtonEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Uint32 which;
    Uint8 button;
    bool down;
    Uint8 clicks;
    Uint8 padding;
    float x;
    float y;
} S3_MouseButtonEvent;

typedef enum S3_MouseWheelDirection {
    S3_MOUSEWHEEL_NORMAL,
    S3_MOUSEWHEEL_FLIPPED,
} S3_MouseWheelDirection;

typedef struct S3_MouseWheelEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Uint32 which;
    float x;
    float y;
    Uint32 direction;
    float mouse_x;
    float mouse_y;
    Sint32 integer_x;
    Sint32 integer_y;
} S3_MouseWheelEvent;

typedef struct S3_GamepadDeviceEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_JoystickID which;
} S3_GamepadDeviceEvent;

typedef struct S3_GamepadAxisEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_JoystickID which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
} S3_GamepadAxisEvent;

typedef struct S3_GamepadButtonEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_JoystickID which;
    Uint8 button;
    bool down;
    Uint8 padding1;
    Uint8 padding2;
} S3_GamepadButtonEvent;

typedef struct S3_TouchFingerEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    Uint64 touchID;
    Uint64 fingerID;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
    S3_WindowID windowID;
} S3_TouchFingerEvent;

#define S3_TOUCH_MOUSEID ((Uint32)-1)

typedef struct S3_DropEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    float x;
    float y;
    const char* source;
    const char* data;
} S3_DropEvent;

typedef struct S3_QuitEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
} S3_QuitEvent;

typedef struct S3_UserEvent {
    Uint32 type;
    Uint32 reserved;
    Uint64 timestamp;
    S3_WindowID windowID;
    Sint32 code;
    void* data1;
    void* data2;
} S3_UserEvent;

typedef union S3_PUBLIC_TAG(SDL_Event, S3_Event) {
    Uint32 type;
    S3_CommonEvent common;
    S3_WindowEvent window;
    S3_KeyboardEvent key;
    S3_TextEditingEvent edit;
    S3_TextInputEvent text;
    S3_MouseMotionEvent motion;
    S3_MouseButtonEvent button;
    S3_MouseWheelEvent wheel;
    S3_GamepadDeviceEvent gdevice;
    S3_GamepadAxisEvent gaxis;
    S3_GamepadButtonEvent gbutton;
    S3_TouchFingerEvent tfinger;
    S3_DropEvent drop;
    S3_QuitEvent quit;
    S3_UserEvent user;
    Uint8 padding[128];
} S3_Event;

typedef bool(S3_CALL* S3_EventFilter)(void* userdata, S3_Event* event);

extern bool S3_PollEvent(S3_Event* event);
extern bool S3_WaitEvent(S3_Event* event);
extern bool S3_WaitEventTimeout(S3_Event* event, Sint32 timeoutMS);
extern void S3_PumpEvents(void);
typedef enum S3_EventAction {
    S3_ADDEVENT,
    S3_PEEKEVENT,
    S3_GETEVENT,
} S3_EventAction;
extern int S3_PeepEvents(S3_Event* events, int numevents, S3_EventAction action, Uint32 minType, Uint32 maxType);
extern bool S3_PushEvent(S3_Event* event);
extern Uint32 S3_RegisterEvents(int numevents);
extern bool S3_AddEventWatch(S3_EventFilter filter, void* userdata);
extern void S3_RemoveEventWatch(S3_EventFilter filter, void* userdata);
extern S3_Window* S3_GetWindowFromEvent(const S3_Event* event);
extern bool S3_ConvertEventToRenderCoordinates(S3_Renderer* renderer, S3_Event* event);

typedef struct SDL_mutex S3_Mutex;

extern S3_Mutex* S3_CreateMutex(void);
extern void S3_LockMutex(S3_Mutex* mutex);
extern void S3_UnlockMutex(S3_Mutex* mutex);
extern void S3_DestroyMutex(S3_Mutex* mutex);

typedef Uint32 S3_AudioDeviceID;

#define S3_AUDIO_DEVICE_DEFAULT_PLAYBACK 0xFFFFFFFFu
#define S3_AUDIO_DEVICE_DEFAULT_RECORDING 0xFFFFFFFEu

typedef Uint32 S3_AudioFormat;

#define S3_AUDIO_U8 0x0008u
#define S3_AUDIO_S8 0x8008u
#define S3_AUDIO_S16LE 0x8010u
#define S3_AUDIO_S16BE 0x9010u
#define S3_AUDIO_S32LE 0x8020u
#define S3_AUDIO_F32LE 0x8120u

#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define S3_AUDIO_S16 S3_AUDIO_S16BE
#else
#define S3_AUDIO_S16 S3_AUDIO_S16LE
#endif

typedef struct S3_AudioSpec {
    S3_AudioFormat format;
    int channels;
    int freq;
} S3_AudioSpec;

typedef struct S3_AudioStream S3_AudioStream;

typedef void(S3_CALL* S3_AudioStreamCallback)(void* userdata, S3_AudioStream* stream, int additional_amount, int total_amount);

extern S3_AudioStream* S3_CreateAudioStream(const S3_AudioSpec* src_spec, const S3_AudioSpec* dst_spec);
extern S3_AudioStream* S3_OpenAudioDeviceStream(S3_AudioDeviceID devid, const S3_AudioSpec* spec, S3_AudioStreamCallback callback, void* userdata);
extern S3_AudioDeviceID S3_GetAudioStreamDevice(S3_AudioStream* stream);
extern bool S3_ResumeAudioStreamDevice(S3_AudioStream* stream);
extern bool S3_GetAudioDeviceFormat(S3_AudioDeviceID devid, S3_AudioSpec* spec, int* sample_frames);
extern const char* S3_GetAudioDeviceName(S3_AudioDeviceID devid);
extern bool S3_PutAudioStreamData(S3_AudioStream* stream, const void* buf, int len);
extern int S3_GetAudioStreamQueued(S3_AudioStream* stream);
extern int S3_GetAudioStreamData(S3_AudioStream* stream, void* buf, int len);
extern void S3_DestroyAudioStream(S3_AudioStream* stream);

typedef Uint32 S3_MessageBoxFlags;

#define S3_MESSAGEBOX_ERROR 0x00000010u
#define S3_MESSAGEBOX_WARNING 0x00000020u
#define S3_MESSAGEBOX_INFORMATION 0x00000040u

extern bool S3_ShowSimpleMessageBox(S3_MessageBoxFlags flags, const char* title, const char* message, S3_Window* window);
extern int S3_GetSystemRAM(void);

#undef S3_PUBLIC_TAG

#ifdef __cplusplus
}
#endif

#endif
