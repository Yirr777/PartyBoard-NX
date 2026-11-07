#include "s3_internal.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_FAILURE_USERMSG
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#define STBI_ASSERT(x) SDL_assert(x)

// Unused-function warnings fire at the end of the unit, so these stay off for the whole file.
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "stb_image.h"

// SDL2 frees surface pixels with SDL_free unless SDL_PREALLOC is set, so stb's buffer passes over.
static SDL_Surface* S3_AdoptPixels(stbi_uc* pixels, int w, int h, int bytes_per_pixel, Uint32 format)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, bytes_per_pixel * 8, w * bytes_per_pixel, format);

    if (surface == NULL) {
        stbi_image_free(pixels);
        return NULL;
    }

    surface->flags &= ~(Uint32)SDL_PREALLOC;

    return surface;
}

static S3_Surface* S3_LoadPNG_Mem(void* data, size_t size);

S3_Surface* S3_LoadPNG(const char* file)
{
    void* data;
    size_t size = 0;

    if (file == NULL || *file == '\0') {
        SDL_SetError("Parameter 'file' is invalid");
        return NULL;
    }

    data = SDL_LoadFile(file, &size);
    if (data == NULL) {
        return NULL;
    }
    return S3_LoadPNG_Mem(data, size);
}

// mp4-switch addition: SDL3's SDL_LoadPNG_IO, reading the whole stream.
S3_Surface* S3_LoadPNG_IO(S3_IOStream* src, bool closeio)
{
    void* data = NULL;
    size_t size = 0;
    size_t capacity = 0;

    if (src == NULL) {
        SDL_SetError("Parameter 'src' is invalid");
        return NULL;
    }
    for (;;) {
        size_t got;
        if (size == capacity) {
            size_t next = capacity ? capacity * 2 : 65536;
            void* grown = SDL_realloc(data, next);
            if (grown == NULL) {
                SDL_free(data);
                if (closeio) {
                    S3_CloseIO(src);
                }
                SDL_OutOfMemory();
                return NULL;
            }
            data = grown;
            capacity = next;
        }
        got = S3_ReadIO(src, (Uint8*)data + size, capacity - size);
        if (got == 0) {
            break;
        }
        size += got;
    }
    if (closeio) {
        S3_CloseIO(src);
    }
    return S3_LoadPNG_Mem(data, size);
}

// Takes ownership of data.
static S3_Surface* S3_LoadPNG_Mem(void* data, size_t size)
{
    static const Uint8 magic[4] = { 0x89, 'P', 'N', 'G' };
    SDL_Surface* surface = NULL;
    stbi_uc* pixels;
    int w;
    int h;
    int components;

    if (data == NULL || size < sizeof(magic) || SDL_memcmp(data, magic, sizeof(magic)) != 0 || size > (size_t)INT_MAX) {
        SDL_free(data);
        SDL_SetError("File is not a PNG file");
        return NULL;
    }

    // Paletted PNGs arrive as RGB24 or RGBA32 here, where SDL3 keeps them INDEX8.
    pixels = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &components, 0);
    SDL_free(data);

    if (pixels == NULL) {
        SDL_SetError("%s", stbi_failure_reason());
        return NULL;
    }

    switch (components) {
    case STBI_rgb_alpha:
        surface = S3_AdoptPixels(pixels, w, h, 4, SDL_PIXELFORMAT_RGBA32);
        break;

    case STBI_rgb:
        surface = S3_AdoptPixels(pixels, w, h, 3, SDL_PIXELFORMAT_RGB24);
        break;

    case STBI_grey:
        surface = S3_AdoptPixels(pixels, w, h, 1, SDL_PIXELFORMAT_INDEX8);
        if (surface != NULL && surface->format->palette != NULL) {
            SDL_Color colors[256];
            int index;

            for (index = 0; index < 256; index++) {
                colors[index].r = (Uint8)index;
                colors[index].g = (Uint8)index;
                colors[index].b = (Uint8)index;
                colors[index].a = SDL_ALPHA_OPAQUE;
            }

            SDL_SetPaletteColors(surface->format->palette, colors, 0, 256);
        }
        break;

    case STBI_grey_alpha:
        surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
        if (surface != NULL) {
            const stbi_uc* src = pixels;
            int row;
            int col;

            for (row = 0; row < h; row++) {
                Uint8* dst = (Uint8*)surface->pixels + row * surface->pitch;

                for (col = 0; col < w; col++) {
                    dst[0] = src[0];
                    dst[1] = src[0];
                    dst[2] = src[0];
                    dst[3] = src[1];
                    dst += 4;
                    src += 2;
                }
            }
        }
        stbi_image_free(pixels);
        break;

    default:
        stbi_image_free(pixels);
        SDL_SetError("Unknown image format: %d", components);
        break;
    }

    return S3_WrapSurface(surface);
}
