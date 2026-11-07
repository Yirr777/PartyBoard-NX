#include "s3_internal.h"

void* S3_malloc(size_t size)
{
    return SDL_malloc(size);
}

void S3_free(void* mem)
{
    SDL_free(mem);
}

int S3_snprintf(char* text, size_t maxlen, const char* fmt, ...)
{
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = SDL_vsnprintf(text, maxlen, fmt, ap);
    va_end(ap);

    return result;
}

int S3_vsnprintf(char* text, size_t maxlen, const char* fmt, va_list ap)
{
    return SDL_vsnprintf(text, maxlen, fmt, ap);
}

int S3_strcmp(const char* str1, const char* str2)
{
    return SDL_strcmp(str1, str2);
}

int S3_strcasecmp(const char* str1, const char* str2)
{
    return SDL_strcasecmp(str1, str2);
}

int S3_strncasecmp(const char* str1, const char* str2, size_t maxlen)
{
    return SDL_strncasecmp(str1, str2, maxlen);
}

size_t S3_strlcpy(char* dst, const char* src, size_t maxlen)
{
    return SDL_strlcpy(dst, src, maxlen);
}

size_t S3_strlcat(char* dst, const char* src, size_t maxlen)
{
    return SDL_strlcat(dst, src, maxlen);
}

char* S3_strlwr(char* str)
{
    return SDL_strlwr(str);
}

char* S3_strupr(char* str)
{
    return SDL_strupr(str);
}

char* S3_strrev(char* str)
{
    return SDL_strrev(str);
}

char* S3_itoa(int value, char* str, int radix)
{
    return SDL_itoa(value, str, radix);
}

char* S3_lltoa(Sint64 value, char* str, int radix)
{
    return SDL_lltoa(value, str, radix);
}

char* S3_ulltoa(Uint64 value, char* str, int radix)
{
    return SDL_ulltoa(value, str, radix);
}

Sint64 S3_strtoll(const char* str, char** endp, int base)
{
    return SDL_strtoll(str, endp, base);
}

// SDL3's ctype helpers are locale-free ASCII over the whole int range; SDL2's forward to <ctype.h>.
int S3_isalpha(int x)
{
    return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z');
}

int S3_isdigit(int x)
{
    return x >= '0' && x <= '9';
}

int S3_isspace(int x)
{
    return x == ' ' || x == '\t' || x == '\r' || x == '\n' || x == '\f' || x == '\v';
}

int S3_toupper(int x)
{
    return x >= 'a' && x <= 'z' ? 'A' + (x - 'a') : x;
}

int S3_tolower(int x)
{
    return x >= 'A' && x <= 'Z' ? 'a' + (x - 'A') : x;
}

static Uint64 s3_rand_state;
static bool s3_rand_initialized;

Uint32 S3_rand_bits(void)
{
    if (!s3_rand_initialized) {
        s3_rand_state = SDL_GetPerformanceCounter();
        s3_rand_initialized = true;
    }

    s3_rand_state = s3_rand_state * 0xff1cd035ul + 0x05;

    return (Uint32)(s3_rand_state >> 32);
}

const char* S3_GetError(void)
{
    return SDL_GetError();
}

bool S3_SetError(const char* fmt, ...)
{
    char buffer[1024];
    va_list ap;

    if (fmt == NULL) {
        return false;
    }

    va_start(ap, fmt);
    SDL_vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    SDL_SetError("%s", buffer);

    return false;
}

void S3_Log(const char* fmt, ...)
{
    char buffer[1024];
    va_list ap;

    va_start(ap, fmt);
    SDL_vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    SDL_Log("%s", buffer);
}
