#include <assert.h>

#include "s3_internal.h"

// Rects, colors and palettes are cast between SDL2 and the layer, not converted.
static_assert(sizeof(S3_Rect) == sizeof(SDL_Rect), "S3_Rect size");
static_assert(offsetof(S3_Rect, x) == offsetof(SDL_Rect, x), "S3_Rect::x");
static_assert(offsetof(S3_Rect, y) == offsetof(SDL_Rect, y), "S3_Rect::y");
static_assert(offsetof(S3_Rect, w) == offsetof(SDL_Rect, w), "S3_Rect::w");
static_assert(offsetof(S3_Rect, h) == offsetof(SDL_Rect, h), "S3_Rect::h");

static_assert(sizeof(S3_FRect) == sizeof(SDL_FRect), "S3_FRect size");
static_assert(offsetof(S3_FRect, x) == offsetof(SDL_FRect, x), "S3_FRect::x");
static_assert(offsetof(S3_FRect, y) == offsetof(SDL_FRect, y), "S3_FRect::y");
static_assert(offsetof(S3_FRect, w) == offsetof(SDL_FRect, w), "S3_FRect::w");
static_assert(offsetof(S3_FRect, h) == offsetof(SDL_FRect, h), "S3_FRect::h");

static_assert(sizeof(S3_Color) == sizeof(SDL_Color), "S3_Color size");
static_assert(offsetof(S3_Color, r) == offsetof(SDL_Color, r), "S3_Color::r");
static_assert(offsetof(S3_Color, g) == offsetof(SDL_Color, g), "S3_Color::g");
static_assert(offsetof(S3_Color, b) == offsetof(SDL_Color, b), "S3_Color::b");
static_assert(offsetof(S3_Color, a) == offsetof(SDL_Color, a), "S3_Color::a");

static_assert(sizeof(S3_Palette) == sizeof(SDL_Palette), "S3_Palette size");
static_assert(offsetof(S3_Palette, ncolors) == offsetof(SDL_Palette, ncolors), "S3_Palette::ncolors");
static_assert(offsetof(S3_Palette, colors) == offsetof(SDL_Palette, colors), "S3_Palette::colors");
static_assert(offsetof(S3_Palette, version) == offsetof(SDL_Palette, version), "S3_Palette::version");
static_assert(offsetof(S3_Palette, refcount) == offsetof(SDL_Palette, refcount), "S3_Palette::refcount");

static_assert((int)S3_PIXELFORMAT_INDEX8 == (int)SDL_PIXELFORMAT_INDEX8, "INDEX8");
static_assert((int)S3_PIXELFORMAT_RGB24 == (int)SDL_PIXELFORMAT_RGB24, "RGB24");
static_assert((int)S3_PIXELFORMAT_XRGB8888 == (int)SDL_PIXELFORMAT_XRGB8888, "XRGB8888");
static_assert((int)S3_PIXELFORMAT_ARGB8888 == (int)SDL_PIXELFORMAT_ARGB8888, "ARGB8888");
static_assert((int)S3_PIXELFORMAT_RGBA8888 == (int)SDL_PIXELFORMAT_RGBA8888, "RGBA8888");
static_assert((int)S3_PIXELFORMAT_ABGR8888 == (int)SDL_PIXELFORMAT_ABGR8888, "ABGR8888");
static_assert((int)S3_PIXELFORMAT_RGBA32 == (int)SDL_PIXELFORMAT_RGBA32, "RGBA32");

// Surface flags match bit for bit except SDL2's SDL_DONTFREE, which is SDL3's SDL_SURFACE_LOCKED.
static_assert(S3_SURFACE_PREALLOCATED == SDL_PREALLOC, "SDL_PREALLOC");
static_assert(S3_SURFACE_LOCK_NEEDED == SDL_RLEACCEL, "SDL_RLEACCEL");
static_assert(S3_SURFACE_LOCKED == SDL_DONTFREE, "SDL_DONTFREE");
static_assert(S3_SURFACE_SIMD_ALIGNED == SDL_SIMD_ALIGNED, "SDL_SIMD_ALIGNED");
static_assert((int)S3_IO_SEEK_SET == RW_SEEK_SET, "RW_SEEK_SET");
static_assert((int)S3_IO_SEEK_CUR == RW_SEEK_CUR, "RW_SEEK_CUR");
static_assert((int)S3_IO_SEEK_END == RW_SEEK_END, "RW_SEEK_END");
static_assert((int)S3_TEXTUREACCESS_STREAMING == (int)SDL_TEXTUREACCESS_STREAMING, "TEXTUREACCESS_STREAMING");
static_assert(S3_MESSAGEBOX_ERROR == (Uint32)SDL_MESSAGEBOX_ERROR, "MESSAGEBOX_ERROR");
static_assert(S3_AUDIO_S16LE == (Uint32)AUDIO_S16LSB, "AUDIO_S16LE");
static_assert(S3_WINDOWPOS_UNDEFINED_MASK == SDL_WINDOWPOS_UNDEFINED_MASK, "SDL_WINDOWPOS_UNDEFINED_MASK");
static_assert(S3_WINDOWPOS_CENTERED_MASK == SDL_WINDOWPOS_CENTERED_MASK, "SDL_WINDOWPOS_CENTERED_MASK");
static_assert((int)S3_ORIENTATION_PORTRAIT_FLIPPED == (int)SDL_ORIENTATION_PORTRAIT_FLIPPED, "SDL_ORIENTATION_PORTRAIT_FLIPPED");

SDL_Surface* S3_UnwrapSurface(S3_Surface* surface)
{
    return surface != NULL ? (SDL_Surface*)surface->reserved : NULL;
}

void S3_RefreshSurface(S3_Surface* surface)
{
    SDL_Surface* inner = (SDL_Surface*)surface->reserved;

    // The layer owns the LOCKED bit; SDL2's DONTFREE is masked out of it.
    surface->flags = (inner->flags & ~(S3_SurfaceFlags)S3_SURFACE_LOCKED)
        | (surface->flags & S3_SURFACE_LOCKED);
    surface->format = inner->format->format;
    surface->w = inner->w;
    surface->h = inner->h;
    surface->pitch = inner->pitch;
    surface->pixels = inner->pixels;
    surface->refcount = inner->refcount;
}

S3_Surface* S3_WrapSurface(SDL_Surface* sdl2_surface)
{
    S3_Surface* surface;

    if (sdl2_surface == NULL) {
        return NULL;
    }

    surface = (S3_Surface*)SDL_calloc(1, sizeof(*surface));
    if (surface == NULL) {
        SDL_FreeSurface(sdl2_surface);
        SDL_SetError("Out of memory");
        return NULL;
    }

    surface->reserved = sdl2_surface;
    S3_RefreshSurface(surface);

    return surface;
}

S3_Surface* S3_CreateSurface(int width, int height, S3_PixelFormat format)
{
    return S3_WrapSurface(SDL_CreateRGBSurfaceWithFormat(0, width, height, (int)SDL_BITSPERPIXEL(format), (Uint32)format));
}

S3_Surface* S3_CreateSurfaceFrom(int width, int height, S3_PixelFormat format, void* pixels, int pitch)
{
    return S3_WrapSurface(SDL_CreateRGBSurfaceWithFormatFrom(pixels, width, height, (int)SDL_BITSPERPIXEL(format), pitch, (Uint32)format));
}

void S3_DestroySurface(S3_Surface* surface)
{
    SDL_Surface* inner;

    if (surface == NULL) {
        return;
    }

    inner = (SDL_Surface*)surface->reserved;

    // An SDL2 surface with other references outlives SDL_FreeSurface, so its wrapper must too.
    if (inner != NULL && inner->refcount > 1) {
        SDL_FreeSurface(inner);
        S3_RefreshSurface(surface);
        return;
    }

    SDL_FreeSurface(inner);
    SDL_free(surface);
}

S3_Surface* S3_ConvertSurface(S3_Surface* surface, S3_PixelFormat format)
{
    if (surface == NULL) {
        SDL_SetError("Parameter 'surface' is invalid");
        return NULL;
    }

    return S3_WrapSurface(SDL_ConvertSurfaceFormat(S3_UnwrapSurface(surface), (Uint32)format, 0));
}

bool S3_FillSurfaceRect(S3_Surface* dst, const S3_Rect* rect, Uint32 color)
{
    if (dst == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    return SDL_FillRect((SDL_Surface*)dst->reserved, (const SDL_Rect*)rect, color) == 0;
}

bool S3_BlitSurface(S3_Surface* src, const S3_Rect* srcrect, S3_Surface* dst, const S3_Rect* dstrect)
{
    SDL_Rect native_dst_rect;

    if (src == NULL || dst == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    // SDL2 writes the clipped rect back into dstrect; SDL3 leaves it alone.
    if (dstrect != NULL) {
        native_dst_rect = *(const SDL_Rect*)dstrect;
    }

    return SDL_UpperBlit((SDL_Surface*)src->reserved,
               (const SDL_Rect*)srcrect,
               (SDL_Surface*)dst->reserved,
               dstrect != NULL ? &native_dst_rect : NULL)
        == 0;
}

// SDL_SoftStretchLinear handles only same-format 32bpp surfaces without color key or blending.
static bool S3_CanSoftStretchLinear(SDL_Surface* src, SDL_Surface* dst)
{
    SDL_BlendMode blend_mode;
    Uint32 key;

    if (src->format->format != dst->format->format) {
        return false;
    }

    if (src->format->BytesPerPixel != 4) {
        return false;
    }

    if (SDL_GetColorKey(src, &key) == 0) {
        return false;
    }

    if (SDL_GetSurfaceBlendMode(src, &blend_mode) != 0 || blend_mode != SDL_BLENDMODE_NONE) {
        return false;
    }

    return true;
}

bool S3_BlitSurfaceScaled(S3_Surface* src, const S3_Rect* srcrect, S3_Surface* dst, const S3_Rect* dstrect, S3_ScaleMode scale_mode)
{
    SDL_Surface* native_src;
    SDL_Surface* native_dst;
    SDL_Rect native_dst_rect;

    if (src == NULL || dst == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    native_src = (SDL_Surface*)src->reserved;
    native_dst = (SDL_Surface*)dst->reserved;

    // PIXELART is a renderer-only filter in SDL3; surface blits treat it as nearest.
    if (scale_mode == S3_SCALEMODE_LINEAR
        && srcrect != NULL
        && dstrect != NULL
        && S3_CanSoftStretchLinear(native_src, native_dst)) {
        if (SDL_SoftStretchLinear(native_src, (const SDL_Rect*)srcrect, native_dst, (const SDL_Rect*)dstrect) == 0) {
            return true;
        }
    }

    if (dstrect != NULL) {
        native_dst_rect = *(const SDL_Rect*)dstrect;
    }

    return SDL_UpperBlitScaled(native_src,
               (const SDL_Rect*)srcrect,
               native_dst,
               dstrect != NULL ? &native_dst_rect : NULL)
        == 0;
}

bool S3_SetSurfaceColorKey(S3_Surface* surface, bool enabled, Uint32 key)
{
    bool ok;

    if (surface == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    ok = SDL_SetColorKey((SDL_Surface*)surface->reserved, enabled ? SDL_TRUE : SDL_FALSE, key) == 0;

    // SDL_SetColorKey can toggle SDL_RLEACCEL, the bit SDL_MUSTLOCK reads.
    S3_RefreshSurface(surface);

    return ok;
}

bool S3_LockSurface(S3_Surface* surface)
{
    if (surface == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    if (SDL_LockSurface((SDL_Surface*)surface->reserved) != 0) {
        return false;
    }

    S3_RefreshSurface(surface);
    surface->flags |= S3_SURFACE_LOCKED;

    return true;
}

void S3_UnlockSurface(S3_Surface* surface)
{
    if (surface == NULL) {
        return;
    }

    SDL_UnlockSurface((SDL_Surface*)surface->reserved);
    S3_RefreshSurface(surface);

    surface->flags &= ~(S3_SurfaceFlags)S3_SURFACE_LOCKED;
}

S3_Palette* S3_GetSurfacePalette(S3_Surface* surface)
{
    if (surface == NULL) {
        return NULL;
    }

    return (S3_Palette*)((SDL_Surface*)surface->reserved)->format->palette;
}

// SDL3's details pointers stay valid for the process, so entries are never recycled.
#define S3_PIXEL_FORMAT_CACHE_SIZE 64

static S3_PixelFormatDetails s3_pixel_format_cache[S3_PIXEL_FORMAT_CACHE_SIZE];
static int s3_pixel_format_cache_count;

const S3_PixelFormatDetails* S3_GetPixelFormatDetails(S3_PixelFormat format)
{
    S3_PixelFormatDetails* details;
    SDL_PixelFormat* native;
    int index;

    for (index = 0; index < s3_pixel_format_cache_count; index++) {
        if (s3_pixel_format_cache[index].format == format) {
            return &s3_pixel_format_cache[index];
        }
    }

    native = SDL_AllocFormat((Uint32)format);
    if (native == NULL) {
        return NULL;
    }

    if (s3_pixel_format_cache_count >= S3_PIXEL_FORMAT_CACHE_SIZE) {
        SDL_FreeFormat(native);
        SDL_SetError("Too many distinct pixel formats queried");
        return NULL;
    }

    details = &s3_pixel_format_cache[s3_pixel_format_cache_count++];
    SDL_memset(details, 0, sizeof(*details));

    details->format = format;
    details->bits_per_pixel = native->BitsPerPixel;
    details->bytes_per_pixel = native->BytesPerPixel;
    details->Rmask = native->Rmask;
    details->Gmask = native->Gmask;
    details->Bmask = native->Bmask;
    details->Amask = native->Amask;
    details->Rbits = (Uint8)(8 - native->Rloss);
    details->Gbits = (Uint8)(8 - native->Gloss);
    details->Bbits = (Uint8)(8 - native->Bloss);
    details->Abits = (Uint8)(8 - native->Aloss);
    details->Rshift = native->Rshift;
    details->Gshift = native->Gshift;
    details->Bshift = native->Bshift;
    details->Ashift = native->Ashift;

    SDL_FreeFormat(native);

    return details;
}

bool S3_GetMasksForPixelFormat(S3_PixelFormat format, int* bpp, Uint32* rmask, Uint32* gmask, Uint32* bmask, Uint32* amask)
{
    return SDL_PixelFormatEnumToMasks((Uint32)format, bpp, rmask, gmask, bmask, amask) == SDL_TRUE;
}

S3_PixelFormat S3_GetPixelFormatForMasks(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    return (S3_PixelFormat)SDL_MasksToPixelFormatEnum(bpp, Rmask, Gmask, Bmask, Amask);
}

bool S3_HideCursor(void)
{
    return SDL_ShowCursor(SDL_DISABLE) >= 0;
}

bool S3_ShowCursor(void)
{
    return SDL_ShowCursor(SDL_ENABLE) >= 0;
}

bool S3_SetWindowRelativeMouseMode(S3_Window* window, bool enabled)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    return SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) == 0;
}

#ifndef __SWITCH__

// SDL3 display ids are 1-based; id N is SDL2's display index N-1.
static int S3_DisplayIndex(S3_DisplayID display_id)
{
    if (display_id == 0 || (int)display_id > SDL_GetNumVideoDisplays()) {
        SDL_SetError("Invalid display");
        return -1;
    }

    return (int)display_id - 1;
}

S3_DisplayID* S3_GetDisplays(int* count)
{
    const int total = SDL_GetNumVideoDisplays();
    S3_DisplayID* displays;
    int index;

    if (count != NULL) {
        *count = 0;
    }

    if (total < 0) {
        return NULL;
    }

    displays = (S3_DisplayID*)SDL_malloc(sizeof(*displays) * ((size_t)total + 1));
    if (displays == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    for (index = 0; index < total; index++) {
        displays[index] = (S3_DisplayID)index + 1;
    }
    displays[total] = 0;

    if (count != NULL) {
        *count = total;
    }

    return displays;
}

S3_DisplayID S3_GetPrimaryDisplay(void)
{
    if (SDL_GetNumVideoDisplays() <= 0) {
        SDL_SetError("Video subsystem has not been initialized");
        return 0;
    }

    return 1;
}

S3_DisplayID S3_GetDisplayForWindow(S3_Window* window)
{
    const int index = SDL_GetWindowDisplayIndex(window);

    return index < 0 ? 0 : (S3_DisplayID)index + 1;
}

float S3_GetDisplayContentScale(S3_DisplayID display_id)
{
#ifdef __ANDROID__
    float ddpi;

    if (SDL_GetDisplayDPI((int)display_id - 1, &ddpi, NULL, NULL) == 0 && ddpi > 0.0f) {
        return ddpi / 160.0f;
    }
#else
    (void)display_id;
#endif

    // SDL3's desktop backends report 1.0 and express HiDPI through pixel density.
    return 1.0f;
}

bool S3_GetDisplayBounds(S3_DisplayID display_id, S3_Rect* rect)
{
    if (rect != NULL) {
        SDL_zerop((SDL_Rect*)rect);
    }

    return SDL_GetDisplayBounds((int)display_id - 1, (SDL_Rect*)rect) == 0;
}

bool S3_GetDisplayUsableBounds(S3_DisplayID display_id, S3_Rect* rect)
{
    if (rect != NULL) {
        SDL_zerop((SDL_Rect*)rect);
    }

    return SDL_GetDisplayUsableBounds((int)display_id - 1, (SDL_Rect*)rect) == 0;
}

// SDL3 hands out pointers into per-display storage that stays valid until the modes change.
#define S3_DISPLAY_MODE_CACHE_SIZE 16

static S3_DisplayMode s3_current_modes[S3_DISPLAY_MODE_CACHE_SIZE];
static S3_DisplayMode s3_desktop_modes[S3_DISPLAY_MODE_CACHE_SIZE];

static const S3_DisplayMode* S3_FillDisplayMode(S3_DisplayID display_id, bool desktop)
{
    const int index = S3_DisplayIndex(display_id);
    S3_DisplayMode* mode;
    SDL_DisplayMode native;

    if (index < 0) {
        return NULL;
    }

    if (index >= S3_DISPLAY_MODE_CACHE_SIZE) {
        SDL_SetError("Too many displays");
        return NULL;
    }

    if ((desktop ? SDL_GetDesktopDisplayMode(index, &native) : SDL_GetCurrentDisplayMode(index, &native)) != 0) {
        return NULL;
    }

    mode = desktop ? &s3_desktop_modes[index] : &s3_current_modes[index];
    SDL_zerop(mode);
    mode->displayID = display_id;
    mode->format = (S3_PixelFormat)native.format;
    mode->w = native.w;
    mode->h = native.h;
    mode->pixel_density = 1.0f;
    mode->refresh_rate = (float)native.refresh_rate;
    mode->refresh_rate_numerator = native.refresh_rate;
    mode->refresh_rate_denominator = native.refresh_rate != 0 ? 1 : 0;

    return mode;
}

const S3_DisplayMode* S3_GetCurrentDisplayMode(S3_DisplayID display_id)
{
    return S3_FillDisplayMode(display_id, false);
}

const S3_DisplayMode* S3_GetDesktopDisplayMode(S3_DisplayID display_id)
{
    return S3_FillDisplayMode(display_id, true);
}

S3_DisplayOrientation S3_GetNaturalDisplayOrientation(S3_DisplayID display_id)
{
    const S3_DisplayMode* mode = S3_GetDesktopDisplayMode(display_id);

    // SDL2 has no natural orientation; SDL3's desktop backends derive it from the desktop mode.
    if (mode == NULL) {
        return S3_ORIENTATION_UNKNOWN;
    }

    return mode->w >= mode->h ? S3_ORIENTATION_LANDSCAPE : S3_ORIENTATION_PORTRAIT;
}

S3_DisplayOrientation S3_GetCurrentDisplayOrientation(S3_DisplayID display_id)
{
    const int index = S3_DisplayIndex(display_id);

    return index < 0 ? S3_ORIENTATION_UNKNOWN : (S3_DisplayOrientation)SDL_GetDisplayOrientation(index);
}

const char* S3_GetCurrentVideoDriver(void)
{
    return SDL_GetCurrentVideoDriver();
}

bool S3_DisableScreenSaver(void)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        SDL_SetError("Video subsystem has not been initialized");
        return false;
    }

    SDL_DisableScreenSaver();

    return true;
}

bool S3_EnableScreenSaver(void)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        SDL_SetError("Video subsystem has not been initialized");
        return false;
    }

    SDL_EnableScreenSaver();

    return true;
}

static Uint32 S3_ToSDL2WindowFlags(S3_WindowFlags flags)
{
    Uint32 native = 0;

    // SDL3's fullscreen without a mode is SDL2's desktop fullscreen.
    if ((flags & S3_WINDOW_FULLSCREEN) != 0) {
        native |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    if ((flags & S3_WINDOW_HIGH_PIXEL_DENSITY) != 0) {
        native |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    if ((flags & S3_WINDOW_OPENGL) != 0) {
        native |= SDL_WINDOW_OPENGL;
    }

    if ((flags & S3_WINDOW_VULKAN) != 0) {
        native |= SDL_WINDOW_VULKAN;
    }

    if ((flags & S3_WINDOW_METAL) != 0) {
        native |= SDL_WINDOW_METAL;
    }

    if ((flags & S3_WINDOW_HIDDEN) != 0) {
        native |= SDL_WINDOW_HIDDEN;
    }

    if ((flags & S3_WINDOW_BORDERLESS) != 0) {
        native |= SDL_WINDOW_BORDERLESS;
    }

    if ((flags & S3_WINDOW_RESIZABLE) != 0) {
        native |= SDL_WINDOW_RESIZABLE;
    }

    if ((flags & S3_WINDOW_MINIMIZED) != 0) {
        native |= SDL_WINDOW_MINIMIZED;
    }

    if ((flags & S3_WINDOW_MAXIMIZED) != 0) {
        native |= SDL_WINDOW_MAXIMIZED;
    }

    if ((flags & S3_WINDOW_MOUSE_GRABBED) != 0) {
        native |= SDL_WINDOW_MOUSE_GRABBED;
    }

    if ((flags & S3_WINDOW_ALWAYS_ON_TOP) != 0) {
        native |= SDL_WINDOW_ALWAYS_ON_TOP;
    }

    if ((flags & S3_WINDOW_UTILITY) != 0) {
        native |= SDL_WINDOW_UTILITY;
    }

    if ((flags & S3_WINDOW_TOOLTIP) != 0) {
        native |= SDL_WINDOW_TOOLTIP;
    }

    if ((flags & S3_WINDOW_POPUP_MENU) != 0) {
        native |= SDL_WINDOW_POPUP_MENU;
    }

    if ((flags & S3_WINDOW_KEYBOARD_GRABBED) != 0) {
        native |= SDL_WINDOW_KEYBOARD_GRABBED;
    }

    return native;
}

static bool s3_text_input_defaulted;

static void S3_DefaultTextInputOff(void)
{
    // SDL2 enables text input when video starts; SDL3 waits for SDL_StartTextInput.
    if (!s3_text_input_defaulted) {
        s3_text_input_defaulted = true;
        SDL_StopTextInput();
    }
}

static S3_Window* S3_CreateNativeWindow(const char* title, int x, int y, int w, int h, S3_WindowFlags flags)
{
    SDL_Window* window = SDL_CreateWindow(title != NULL ? title : "", x, y, w, h, S3_ToSDL2WindowFlags(flags));

    if (window != NULL) {
        S3_DefaultTextInputOff();
    }

    return window;
}

S3_Window* S3_CreateWindow(const char* title, int w, int h, S3_WindowFlags flags)
{
    return S3_CreateNativeWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
}

S3_Window* S3_CreateWindowWithProperties(S3_PropertiesID props)
{
    return S3_CreateNativeWindow(S3_GetStringProperty(props, S3_PROP_WINDOW_CREATE_TITLE_STRING, NULL),
        (int)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_UNDEFINED),
        (int)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_UNDEFINED),
        (int)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_WIDTH_NUMBER, 0),
        (int)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 0),
        (S3_WindowFlags)S3_GetNumberProperty(props, S3_PROP_WINDOW_CREATE_FLAGS_NUMBER, 0));
}

void S3_DestroyWindow(S3_Window* window)
{
    if (window == NULL) {
        return;
    }

    S3_ReleaseObjectProperties(window);
    SDL_DestroyWindow(window);
}

S3_WindowID S3_GetWindowID(S3_Window* window)
{
    return SDL_GetWindowID(window);
}

S3_Window* S3_GetWindowFromID(S3_WindowID id)
{
    return SDL_GetWindowFromID(id);
}

S3_PropertiesID S3_GetWindowProperties(S3_Window* window)
{
    bool created;
    S3_PropertiesID props;

    if (window == NULL) {
        SDL_SetError("Invalid window");
        return 0;
    }

    props = S3_AcquireObjectProperties(window, &created);

    // SDL2 on Android replaces the native window with each new surface, so it is read every call.
    if (props != 0) {
        S3_PublishNativeWindowProperties(window, props);
    }

    return props;
}

S3_WindowFlags S3_GetWindowFlags(S3_Window* window)
{
    const Uint32 native = SDL_GetWindowFlags(window);
    S3_WindowFlags flags = native
        & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS
            | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MOUSE_GRABBED
            | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_UTILITY
            | SDL_WINDOW_TOOLTIP | SDL_WINDOW_POPUP_MENU | SDL_WINDOW_KEYBOARD_GRABBED | SDL_WINDOW_VULKAN
            | SDL_WINDOW_METAL);

    if ((native & SDL_WINDOW_ALLOW_HIGHDPI) != 0) {
        flags |= S3_WINDOW_HIGH_PIXEL_DENSITY;
    }

    if ((native & SDL_WINDOW_ALWAYS_ON_TOP) != 0) {
        flags |= S3_WINDOW_ALWAYS_ON_TOP;
    }

    if ((native & SDL_WINDOW_FOREIGN) != 0) {
        flags |= S3_WINDOW_EXTERNAL;
    }

    return flags;
}

bool S3_GetWindowSize(S3_Window* window, int* w, int* h)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_GetWindowSize(window, w, h);

    return true;
}

bool S3_GetWindowSizeInPixels(S3_Window* window, int* w, int* h)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_GetWindowSizeInPixels(window, w, h);

    return true;
}

float S3_GetWindowPixelDensity(S3_Window* window)
{
    int w = 0;
    int pixel_w = 0;

    if (window == NULL) {
        SDL_SetError("Invalid window");
        return 0.0f;
    }

    SDL_GetWindowSize(window, &w, NULL);
    SDL_GetWindowSizeInPixels(window, &pixel_w, NULL);

    return w > 0 ? (float)pixel_w / (float)w : 1.0f;
}

float S3_GetWindowDisplayScale(S3_Window* window)
{
    // SDL2 has no display scale; SDL3's desktop answer without content scaling is 1.0.
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return 0.0f;
    }

    return 1.0f;
}

bool S3_SetWindowTitle(S3_Window* window, const char* title)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_SetWindowTitle(window, title);

    return true;
}

bool S3_SetWindowIcon(S3_Window* window, S3_Surface* icon)
{
    if (window == NULL || icon == NULL) {
        SDL_SetError(window == NULL ? "Invalid window" : "Parameter 'icon' is invalid");
        return false;
    }

    SDL_SetWindowIcon(window, S3_UnwrapSurface(icon));

    return true;
}

bool S3_SetWindowPosition(S3_Window* window, int x, int y)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_SetWindowPosition(window, x, y);

    return true;
}

bool S3_SetWindowSize(S3_Window* window, int w, int h)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_SetWindowSize(window, w, h);

    return true;
}

bool S3_SetWindowMinimumSize(S3_Window* window, int min_w, int min_h)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_SetWindowMinimumSize(window, min_w, min_h);

    return true;
}

bool S3_SetWindowFullscreen(S3_Window* window, bool fullscreen)
{
    return SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) == 0;
}

bool S3_ShowWindow(S3_Window* window)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_ShowWindow(window);

    return true;
}

bool S3_HideWindow(S3_Window* window)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_HideWindow(window);

    return true;
}

bool S3_RaiseWindow(S3_Window* window)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_RaiseWindow(window);

    return true;
}

bool S3_RestoreWindow(S3_Window* window)
{
    if (window == NULL) {
        SDL_SetError("Invalid window");
        return false;
    }

    SDL_RestoreWindow(window);

    return true;
}

#endif

static SDL_BlendMode S3_PremultipliedBlend(void)
{
    static SDL_BlendMode mode = SDL_BLENDMODE_INVALID;

    if (mode == SDL_BLENDMODE_INVALID) {
        mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE,
            SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ONE,
            SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            SDL_BLENDOPERATION_ADD);
    }

    return mode;
}

static SDL_BlendMode S3_PremultipliedAdd(void)
{
    static SDL_BlendMode mode = SDL_BLENDMODE_INVALID;

    if (mode == SDL_BLENDMODE_INVALID) {
        mode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE,
            SDL_BLENDFACTOR_ONE,
            SDL_BLENDOPERATION_ADD,
            SDL_BLENDFACTOR_ZERO,
            SDL_BLENDFACTOR_ONE,
            SDL_BLENDOPERATION_ADD);
    }

    return mode;
}

SDL_BlendMode S3_ToSDL2BlendMode(S3_BlendMode mode)
{
    switch (mode) {
    case S3_BLENDMODE_NONE:
        return SDL_BLENDMODE_NONE;
    case S3_BLENDMODE_BLEND:
        return SDL_BLENDMODE_BLEND;
    case S3_BLENDMODE_ADD:
        return SDL_BLENDMODE_ADD;
    case S3_BLENDMODE_MOD:
        return SDL_BLENDMODE_MOD;
    case S3_BLENDMODE_MUL:
        return SDL_BLENDMODE_MUL;
    case S3_BLENDMODE_BLEND_PREMULTIPLIED:
        return S3_PremultipliedBlend();
    case S3_BLENDMODE_ADD_PREMULTIPLIED:
        return S3_PremultipliedAdd();
    default:
        return (SDL_BlendMode)mode;
    }
}

S3_BlendMode S3_FromSDL2BlendMode(SDL_BlendMode mode)
{
    if (mode == S3_PremultipliedBlend()) {
        return S3_BLENDMODE_BLEND_PREMULTIPLIED;
    }

    if (mode == S3_PremultipliedAdd()) {
        return S3_BLENDMODE_ADD_PREMULTIPLIED;
    }

    switch (mode) {
    case SDL_BLENDMODE_NONE:
        return S3_BLENDMODE_NONE;
    case SDL_BLENDMODE_BLEND:
        return S3_BLENDMODE_BLEND;
    case SDL_BLENDMODE_ADD:
        return S3_BLENDMODE_ADD;
    case SDL_BLENDMODE_MOD:
        return S3_BLENDMODE_MOD;
    case SDL_BLENDMODE_MUL:
        return S3_BLENDMODE_MUL;
    default:
        return (S3_BlendMode)mode;
    }
}

#ifndef __SWITCH__
// SDL3's render-driver hint is a comma-separated preference list; SDL2 matches the whole string.
static SDL_Renderer* S3_CreateRendererFromHint(SDL_Window* window)
{
    const char* hint = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
    const char* attempt;
    bool tried = false;

    if (hint == NULL || *hint == '\0') {
        return SDL_CreateRenderer(window, -1, 0);
    }

    for (attempt = hint; attempt != NULL && *attempt != '\0';) {
        const char* separator = SDL_strchr(attempt, ',');
        const size_t length = separator != NULL ? (size_t)(separator - attempt) : SDL_strlen(attempt);
        const int num_drivers = SDL_GetNumRenderDrivers();
        int index;

        for (index = 0; index < num_drivers; index++) {
            SDL_RendererInfo info;
            SDL_Renderer* created;

            if (SDL_GetRenderDriverInfo(index, &info) != 0
                || SDL_strlen(info.name) != length
                || SDL_strncasecmp(info.name, attempt, length) != 0) {
                continue;
            }

            created = SDL_CreateRenderer(window, index, 0);
            if (created != NULL) {
                return created;
            }

            tried = true;
        }

        attempt = separator != NULL ? separator + 1 : NULL;
    }

    if (!tried) {
        SDL_SetError("%s not available", hint);
    }

    return NULL;
}
#endif

S3_Renderer* S3_CreateRendererWithProperties(S3_PropertiesID props)
{
#ifdef __SWITCH__
    // The console window has no SDL2 video behind it to render with.
    (void)props;
    SDL_SetError("SDL_Renderer is not supported by sdl3on2 on Switch");
    return NULL;
#else
    SDL_Window* window = (SDL_Window*)S3_GetPointerProperty(props, S3_PROP_RENDERER_CREATE_WINDOW_POINTER, NULL);
    const Sint64 vsync = S3_GetNumberProperty(props, S3_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 0);
    SDL_Renderer* renderer;

    if (window == NULL) {
        SDL_SetError("Software renderers without a window are not supported by sdl3on2");
        return NULL;
    }

    renderer = S3_CreateRendererFromHint(window);
    if (renderer != NULL && vsync != 0) {
        // SDL2 only knows on and off, so adaptive and every-Nth-frame requests become on.
        SDL_RenderSetVSync(renderer, 1);
    }

    return renderer;
#endif
}

bool S3_CreateWindowAndRenderer(const char* title, int width, int height, S3_WindowFlags window_flags, S3_Window** window, S3_Renderer** renderer)
{
#ifdef __SWITCH__
    (void)title;
    (void)width;
    (void)height;
    (void)window_flags;

    if (window != NULL) {
        *window = NULL;
    }

    if (renderer != NULL) {
        *renderer = NULL;
    }

    SDL_SetError("SDL_Renderer is not supported by sdl3on2 on Switch");

    return false;
#else
    SDL_Window* native_window;
    SDL_Renderer* native_renderer;

    if (window != NULL) {
        *window = NULL;
    }

    if (renderer != NULL) {
        *renderer = NULL;
    }

    native_window = S3_CreateNativeWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    if (native_window == NULL) {
        return false;
    }

    native_renderer = S3_CreateRendererFromHint(native_window);
    if (native_renderer == NULL) {
        SDL_DestroyWindow(native_window);
        return false;
    }

    if (window != NULL) {
        *window = native_window;
    }

    if (renderer != NULL) {
        *renderer = native_renderer;
    }

    return true;
#endif
}

void S3_DestroyRenderer(S3_Renderer* renderer)
{
    if (renderer == NULL) {
        return;
    }

    S3_ReleaseObjectProperties(renderer);
    SDL_DestroyRenderer(renderer);
}

bool S3_SetRenderVSync(S3_Renderer* renderer, int vsync)
{
    // SDL2 2.28 rejects any interval but 0 and 1.
    return SDL_RenderSetVSync(renderer, vsync) == 0;
}

bool S3_SetRenderLogicalPresentation(S3_Renderer* renderer, int w, int h, S3_RendererLogicalPresentation mode)
{
    if (mode == S3_LOGICAL_PRESENTATION_DISABLED) {
        // SDL2's integer scale is sticky across logical-size changes, so clear it explicitly.
        if (SDL_RenderSetIntegerScale(renderer, SDL_FALSE) != 0) {
            return false;
        }

        return SDL_RenderSetLogicalSize(renderer, 0, 0) == 0;
    }

    // SDL2's logical size always letterboxes, so stretch and overscan have no mapping.
    if (mode != S3_LOGICAL_PRESENTATION_LETTERBOX && mode != S3_LOGICAL_PRESENTATION_INTEGER_SCALE) {
        SDL_SetError("Unsupported logical presentation mode");
        return false;
    }

    if (SDL_RenderSetIntegerScale(renderer, mode == S3_LOGICAL_PRESENTATION_INTEGER_SCALE ? SDL_TRUE : SDL_FALSE) != 0) {
        return false;
    }

    return SDL_RenderSetLogicalSize(renderer, w, h) == 0;
}

bool S3_SetRenderScale(S3_Renderer* renderer, float scaleX, float scaleY)
{
    return SDL_RenderSetScale(renderer, scaleX, scaleY) == 0;
}

S3_Texture* S3_CreateTexture(S3_Renderer* renderer, S3_PixelFormat format, int access, int w, int h)
{
    SDL_Texture* texture = SDL_CreateTexture(renderer, (Uint32)format, access, w, h);

    // SDL3 textures default to linear filtering; SDL2's default is nearest.
    if (texture != NULL) {
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    }

    return texture;
}

void S3_DestroyTexture(S3_Texture* texture)
{
    if (texture == NULL) {
        return;
    }

    S3_ReleaseObjectProperties(texture);
    SDL_DestroyTexture(texture);
}

bool S3_UpdateTexture(S3_Texture* texture, const S3_Rect* rect, const void* pixels, int pitch)
{
    return SDL_UpdateTexture(texture, (const SDL_Rect*)rect, pixels, pitch) == 0;
}

bool S3_RenderClear(S3_Renderer* renderer)
{
    return SDL_RenderClear(renderer) == 0;
}

bool S3_RenderTexture(S3_Renderer* renderer, S3_Texture* texture, const S3_FRect* srcrect, const S3_FRect* dstrect)
{
    SDL_Rect native_src_rect;

    if (srcrect != NULL) {
        native_src_rect.x = (int)SDL_lroundf(srcrect->x);
        native_src_rect.y = (int)SDL_lroundf(srcrect->y);
        native_src_rect.w = (int)SDL_lroundf(srcrect->w);
        native_src_rect.h = (int)SDL_lroundf(srcrect->h);
    }

    return SDL_RenderCopyF(renderer, texture, srcrect != NULL ? &native_src_rect : NULL, (const SDL_FRect*)dstrect) == 0;
}

bool S3_RenderFillRect(S3_Renderer* renderer, const S3_FRect* rect)
{
    return SDL_RenderFillRectF(renderer, (const SDL_FRect*)rect) == 0;
}

bool S3_RenderPresent(S3_Renderer* renderer)
{
    if (renderer == NULL) {
        SDL_SetError("Invalid renderer");
        return false;
    }

    SDL_RenderPresent(renderer);

    return true;
}

bool S3_SetRenderDrawColor(S3_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return SDL_SetRenderDrawColor(renderer, r, g, b, a) == 0;
}

bool S3_SetRenderDrawBlendMode(S3_Renderer* renderer, S3_BlendMode blend_mode)
{
    if (SDL_SetRenderDrawBlendMode(renderer, S3_ToSDL2BlendMode(blend_mode)) == 0) {
        return true;
    }

    // SDL2's software renderer refuses custom blend modes; the built-ins match for opaque colors.
    if (blend_mode == S3_BLENDMODE_BLEND_PREMULTIPLIED) {
        return SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) == 0;
    }

    if (blend_mode == S3_BLENDMODE_ADD_PREMULTIPLIED) {
        return SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD) == 0;
    }

    return false;
}

bool S3_GetRenderDrawBlendMode(S3_Renderer* renderer, S3_BlendMode* blend_mode)
{
    SDL_BlendMode native;

    if (blend_mode != NULL) {
        *blend_mode = S3_BLENDMODE_INVALID;
    }

    if (SDL_GetRenderDrawBlendMode(renderer, &native) != 0) {
        return false;
    }

    if (blend_mode != NULL) {
        *blend_mode = S3_FromSDL2BlendMode(native);
    }

    return true;
}

bool S3_RenderDebugTextFormat(S3_Renderer* renderer, float x, float y, const char* fmt, ...)
{
    (void)renderer;
    (void)x;
    (void)y;
    (void)fmt;

    SDL_SetError("SDL_RenderDebugText is not supported by sdl3on2");

    return false;
}
