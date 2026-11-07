#ifndef SDL3ON2_SDL_STDINC_H_
#define SDL3ON2_SDL_STDINC_H_

#ifdef S3_IMPLEMENTATION
#error "sdl3on2 implementation files must include \"s3_internal.h\", never <SDL3/...>."
#endif

#include <SDL3/s3_defs.h>

#define SDL_malloc S3_malloc
#define SDL_free S3_free

#define SDL_snprintf S3_snprintf
#define SDL_vsnprintf S3_vsnprintf

#define SDL_strcmp S3_strcmp
#define SDL_strcasecmp S3_strcasecmp
#define SDL_strncasecmp S3_strncasecmp
#define SDL_strlcpy S3_strlcpy
#define SDL_strlcat S3_strlcat
#define SDL_strlwr S3_strlwr
#define SDL_strupr S3_strupr
#define SDL_strrev S3_strrev
#define SDL_itoa S3_itoa
#define SDL_lltoa S3_lltoa
#define SDL_ulltoa S3_ulltoa
#define SDL_strtoll S3_strtoll

#define SDL_isalpha S3_isalpha
#define SDL_isdigit S3_isdigit
#define SDL_isspace S3_isspace
#define SDL_toupper S3_toupper
#define SDL_tolower S3_tolower

#define SDL_rand_bits S3_rand_bits

#define SDL_arraysize(array) (sizeof(array) / sizeof((array)[0]))
#define SDL_min(x, y) (((x) < (y)) ? (x) : (y))
#define SDL_max(x, y) (((x) > (y)) ? (x) : (y))
#define SDL_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))

#define SDL_SINT64_C(c) INT64_C(c)
#define SDL_UINT64_C(c) UINT64_C(c)

#define SDL_FOURCC(a, b, c, d)                      \
    ((Uint32)(Uint8)(a) | ((Uint32)(Uint8)(b) << 8) \
        | ((Uint32)(Uint8)(c) << 16) | ((Uint32)(Uint8)(d) << 24))

// SDL3 declares these in SDL_timer.h, which SDL_stdinc.h users reach transitively.
#define SDL_NS_PER_SECOND INT64_C(1000000000)
#define SDL_NS_PER_MS INT64_C(1000000)
#define SDL_NS_TO_SECONDS(NS) ((NS) / SDL_NS_PER_SECOND)
#define SDL_MS_TO_NS(MS) (((Uint64)(MS)) * SDL_NS_PER_MS)

#if defined(__cplusplus) && __cplusplus >= 201103L
#define SDL_NORETURN [[noreturn]]
#elif defined(__GNUC__) || defined(__clang__)
#define SDL_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define SDL_NORETURN __declspec(noreturn)
#else
#define SDL_NORETURN
#endif

#endif
