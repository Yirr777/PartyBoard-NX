#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "s3_internal.h"

#ifdef _WIN32
#include <windows.h>

#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

// SDL2's RWops has no status, no flush and counts objects, so the layer stream carries those.
struct S3_IOStream {
    SDL_RWops* rwops;
    S3_IOStatus status;
    bool has_iface;
    S3_IOStreamInterface iface;
    void* userdata;
};

static S3_IOStream* S3_WrapIO(SDL_RWops* rwops)
{
    S3_IOStream* stream;

    if (rwops == NULL) {
        return NULL;
    }

    stream = (S3_IOStream*)SDL_calloc(1, sizeof(*stream));
    if (stream == NULL) {
        SDL_RWclose(rwops);
        SDL_SetError("Out of memory");
        return NULL;
    }

    stream->rwops = rwops;
    stream->status = S3_IO_STATUS_READY;

    return stream;
}

SDL_RWops* S3_UnwrapIO(S3_IOStream* stream)
{
    return stream != NULL ? stream->rwops : NULL;
}

static S3_IOStream* S3_IOContext(SDL_RWops* context)
{
    return (S3_IOStream*)context->hidden.unknown.data1;
}

static Sint64 SDLCALL S3_IOSize(SDL_RWops* context)
{
    S3_IOStream* stream = S3_IOContext(context);

    if (stream->iface.size == NULL) {
        return -1;
    }

    return stream->iface.size(stream->userdata);
}

static Sint64 SDLCALL S3_IOSeek(SDL_RWops* context, Sint64 offset, int whence)
{
    S3_IOStream* stream = S3_IOContext(context);

    if (stream->iface.seek == NULL) {
        return SDL_SetError("Stream is not seekable");
    }

    return stream->iface.seek(stream->userdata, offset, (S3_IOWhence)whence);
}

static size_t SDLCALL S3_IORead(SDL_RWops* context, void* ptr, size_t size, size_t maxnum)
{
    S3_IOStream* stream = S3_IOContext(context);

    if (stream->iface.read == NULL || size == 0 || maxnum == 0) {
        return 0;
    }

    return stream->iface.read(stream->userdata, ptr, size * maxnum, &stream->status) / size;
}

static size_t SDLCALL S3_IOWrite(SDL_RWops* context, const void* ptr, size_t size, size_t num)
{
    S3_IOStream* stream = S3_IOContext(context);

    if (stream->iface.write == NULL || size == 0 || num == 0) {
        return 0;
    }

    return stream->iface.write(stream->userdata, ptr, size * num, &stream->status) / size;
}

static int SDLCALL S3_IOClose(SDL_RWops* context)
{
    S3_IOStream* stream = S3_IOContext(context);
    bool result = true;

    if (stream->iface.flush != NULL) {
        result = stream->iface.flush(stream->userdata, &stream->status);
    }

    if (stream->iface.close != NULL && !stream->iface.close(stream->userdata)) {
        result = false;
    }

    SDL_FreeRW(context);

    return result ? 0 : -1;
}

S3_IOStream* S3_OpenIO(const S3_IOStreamInterface* iface, void* userdata)
{
    S3_IOStream* stream;
    SDL_RWops* rwops;

    if (iface == NULL) {
        SDL_SetError("Invalid parameter 'iface'");
        return NULL;
    }

    if (iface->version < sizeof(*iface)) {
        SDL_SetError("Invalid interface, should be initialized with SDL_INIT_INTERFACE()");
        return NULL;
    }

    stream = (S3_IOStream*)SDL_calloc(1, sizeof(*stream));
    if (stream == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    rwops = SDL_AllocRW();
    if (rwops == NULL) {
        SDL_free(stream);
        return NULL;
    }

    SDL_memcpy(&stream->iface, iface, sizeof(stream->iface));
    stream->userdata = userdata;
    stream->has_iface = true;
    stream->status = S3_IO_STATUS_READY;
    stream->rwops = rwops;

    rwops->size = S3_IOSize;
    rwops->seek = S3_IOSeek;
    rwops->read = S3_IORead;
    rwops->write = S3_IOWrite;
    rwops->close = S3_IOClose;
    rwops->type = SDL_RWOPS_UNKNOWN;
    rwops->hidden.unknown.data1 = stream;
    rwops->hidden.unknown.data2 = NULL;

    return stream;
}

S3_IOStream* S3_IOFromFile(const char* file, const char* mode)
{
    if (file == NULL || *file == '\0') {
        SDL_SetError("Parameter 'file' is invalid");
        return NULL;
    }

    if (mode == NULL || *mode == '\0') {
        SDL_SetError("Parameter 'mode' is invalid");
        return NULL;
    }

    return S3_WrapIO(SDL_RWFromFile(file, mode));
}

S3_IOStream* S3_IOFromConstMem(const void* mem, size_t size)
{
    if (size == 0) {
        // SDL2 rejects an empty block; SDL3 returns a stream already at EOF.
        static const Uint8 empty = 0;
        SDL_RWops* rwops = SDL_RWFromConstMem(&empty, 1);

        if (rwops != NULL) {
            rwops->hidden.mem.stop = rwops->hidden.mem.base;
        }

        return S3_WrapIO(rwops);
    }

    return S3_WrapIO(SDL_RWFromConstMem(mem, (int)size));
}

bool S3_CloseIO(S3_IOStream* context)
{
    bool result;

    if (context == NULL) {
        return true;
    }

    result = SDL_RWclose(context->rwops) == 0;
    SDL_free(context);

    return result;
}

S3_IOStatus S3_GetIOStatus(S3_IOStream* context)
{
    if (context == NULL) {
        SDL_SetError("Parameter 'context' is invalid");
        return S3_IO_STATUS_ERROR;
    }

    return context->status;
}

Sint64 S3_GetIOSize(S3_IOStream* context)
{
    if (context == NULL) {
        return SDL_SetError("Parameter 'context' is invalid");
    }

    return SDL_RWsize(context->rwops);
}

Sint64 S3_SeekIO(S3_IOStream* context, Sint64 offset, S3_IOWhence whence)
{
    if (context == NULL) {
        return SDL_SetError("Parameter 'context' is invalid");
    }

    return SDL_RWseek(context->rwops, offset, (int)whence);
}

Sint64 S3_TellIO(S3_IOStream* context)
{
    return S3_SeekIO(context, 0, S3_IO_SEEK_CUR);
}

size_t S3_ReadIO(S3_IOStream* context, void* ptr, size_t size)
{
    size_t bytes;

    if (context == NULL) {
        SDL_SetError("Parameter 'context' is invalid");
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    context->status = S3_IO_STATUS_READY;
    SDL_ClearError();

    bytes = SDL_RWread(context->rwops, ptr, 1, size);

    if (bytes < size && context->status == S3_IO_STATUS_READY && !context->has_iface) {
#ifdef HAVE_STDIO_H
        if (context->rwops->type == SDL_RWOPS_STDFILE) {
            context->status = ferror(context->rwops->hidden.stdio.fp) ? S3_IO_STATUS_ERROR : S3_IO_STATUS_EOF;
            return bytes;
        }
#endif
        // SDL2 reports a short read without saying why; only a failure sets the error.
        context->status = *SDL_GetError() != '\0' ? S3_IO_STATUS_ERROR : S3_IO_STATUS_EOF;
    }

    return bytes;
}

size_t S3_WriteIO(S3_IOStream* context, const void* ptr, size_t size)
{
    size_t bytes;

    if (context == NULL) {
        SDL_SetError("Parameter 'context' is invalid");
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    context->status = S3_IO_STATUS_READY;
    SDL_ClearError();

    bytes = SDL_RWwrite(context->rwops, ptr, 1, size);

    if (bytes < size && context->status == S3_IO_STATUS_READY && !context->has_iface) {
        context->status = S3_IO_STATUS_ERROR;
    }

    return bytes;
}

bool S3_FlushIO(S3_IOStream* context)
{
    bool result = true;

    if (context == NULL) {
        SDL_SetError("Parameter 'context' is invalid");
        return false;
    }

    context->status = S3_IO_STATUS_READY;
    SDL_ClearError();

    if (context->has_iface) {
        if (context->iface.flush != NULL) {
            result = context->iface.flush(context->userdata, &context->status);
        }
#ifdef HAVE_STDIO_H
    } else if (context->rwops->type == SDL_RWOPS_STDFILE) {
        if (fflush(context->rwops->hidden.stdio.fp) != 0) {
            SDL_SetError("Error flushing datastream: %s", strerror(errno));
            result = false;
        }
#endif
    }

    if (!result && context->status == S3_IO_STATUS_READY) {
        context->status = S3_IO_STATUS_ERROR;
    }

    return result;
}

bool S3_ReadU8(S3_IOStream* src, Uint8* value)
{
    Uint8 data = 0;
    const bool result = S3_ReadIO(src, &data, sizeof(data)) == sizeof(data);

    if (value != NULL) {
        *value = data;
    }

    return result;
}

bool S3_ReadU16LE(S3_IOStream* src, Uint16* value)
{
    Uint16 data = 0;
    const bool result = S3_ReadIO(src, &data, sizeof(data)) == sizeof(data);

    if (value != NULL) {
        *value = SDL_SwapLE16(data);
    }

    return result;
}

bool S3_ReadU32LE(S3_IOStream* src, Uint32* value)
{
    Uint32 data = 0;
    const bool result = S3_ReadIO(src, &data, sizeof(data)) == sizeof(data);

    if (value != NULL) {
        *value = SDL_SwapLE32(data);
    }

    return result;
}

bool S3_WriteU8(S3_IOStream* dst, Uint8 value)
{
    return S3_WriteIO(dst, &value, sizeof(value)) == sizeof(value);
}

bool S3_WriteU16LE(S3_IOStream* dst, Uint16 value)
{
    const Uint16 swapped = SDL_SwapLE16(value);

    return S3_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped);
}

bool S3_WriteU32LE(S3_IOStream* dst, Uint32 value)
{
    const Uint32 swapped = SDL_SwapLE32(value);

    return S3_WriteIO(dst, &swapped, sizeof(swapped)) == sizeof(swapped);
}

bool S3_WriteS32LE(S3_IOStream* dst, Sint32 value)
{
    return S3_WriteU32LE(dst, (Uint32)value);
}

S3_Surface* S3_LoadBMP_IO(S3_IOStream* src, bool closeio)
{
    S3_Surface* surface;

    if (src == NULL) {
        SDL_SetError("Parameter 'src' is invalid");
        return NULL;
    }

    surface = S3_WrapSurface(SDL_LoadBMP_RW(src->rwops, 0));

    if (closeio) {
        S3_CloseIO(src);
    }

    return surface;
}

bool S3_SaveBMP_IO(S3_Surface* surface, S3_IOStream* dst, bool closeio)
{
    bool ok;

    // SDL3 returns before touching the stream here, so it is not closed.
    if (surface == NULL) {
        SDL_SetError("Passed a NULL surface");
        return false;
    }

    if (dst == NULL) {
        SDL_SetError("Parameter 'dst' is invalid");
        return false;
    }

    // SDL3 folds a failed close into the result; SDL2 ignores it.
    ok = SDL_SaveBMP_RW(S3_UnwrapSurface(surface), dst->rwops, 0) == 0;

    if (closeio && !S3_CloseIO(dst)) {
        ok = false;
    }

    return ok;
}

#ifdef _WIN32
#define S3_STAT_STRUCT struct _stat64
#define S3_STAT _stat64
#define S3_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#define S3_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#else
#define S3_STAT_STRUCT struct stat
#define S3_STAT stat
#define S3_ISDIR(mode) S_ISDIR(mode)
#define S3_ISREG(mode) S_ISREG(mode)
#endif

#define S3_NS_PER_SECOND INT64_C(1000000000)

bool S3_GetPathInfo(const char* path, S3_PathInfo* info)
{
    S3_STAT_STRUCT st;

    if (info != NULL) {
        SDL_memset(info, 0, sizeof(*info));
    }

    if (path == NULL || *path == '\0') {
        SDL_SetError("Invalid path");
        return false;
    }

    if (S3_STAT(path, &st) != 0) {
        SDL_SetError("%s", strerror(errno));
        return false;
    }

    if (info != NULL) {
        if (S3_ISDIR(st.st_mode)) {
            info->type = S3_PATHTYPE_DIRECTORY;
        } else if (S3_ISREG(st.st_mode)) {
            info->type = S3_PATHTYPE_FILE;
        } else {
            info->type = S3_PATHTYPE_OTHER;
        }

        info->size = S3_ISDIR(st.st_mode) ? 0 : (Uint64)st.st_size;

        // SDL_Time is nanoseconds since the Unix epoch.
        info->create_time = (S3_Time)st.st_ctime * S3_NS_PER_SECOND;
        info->modify_time = (S3_Time)st.st_mtime * S3_NS_PER_SECOND;
        info->access_time = (S3_Time)st.st_atime * S3_NS_PER_SECOND;
    }

    return true;
}

static bool S3_GlobMatch(const char* pattern, const char* name, bool case_insensitive)
{
    const char* star = NULL;
    const char* star_name = NULL;

    while (*name != '\0') {
        char p = *pattern;
        char n = *name;

        if (case_insensitive) {
            p = (char)SDL_tolower((unsigned char)p);
            n = (char)SDL_tolower((unsigned char)n);
        }

        if (*pattern == '?' || (*pattern != '\0' && *pattern != '*' && p == n)) {
            pattern++;
            name++;
        } else if (*pattern == '*') {
            star = pattern++;
            star_name = name;
        } else if (star != NULL) {
            pattern = star + 1;
            name = ++star_name;
        } else {
            return false;
        }
    }

    while (*pattern == '*') {
        pattern++;
    }

    return *pattern == '\0';
}

typedef struct S3_GlobResult {
    char** names;
    int count;
    int capacity;
    size_t total_size;
} S3_GlobResult;

static bool S3_GlobAppend(S3_GlobResult* result, const char* name)
{
    char* copy;

    if (result->count == result->capacity) {
        const int wanted = result->capacity != 0 ? result->capacity * 2 : 32;
        char** grown = (char**)SDL_realloc(result->names, sizeof(*grown) * (size_t)wanted);

        if (grown == NULL) {
            SDL_SetError("Out of memory");
            return false;
        }

        result->names = grown;
        result->capacity = wanted;
    }

    copy = SDL_strdup(name);
    if (copy == NULL) {
        SDL_SetError("Out of memory");
        return false;
    }

    result->names[result->count++] = copy;
    result->total_size += SDL_strlen(name) + 1;

    return true;
}

static bool S3_GlobScan(const char* path, const char* pattern, bool case_insensitive, S3_GlobResult* result)
{
    bool ok = true;

#ifdef _WIN32
    struct _finddata_t find_data;
    intptr_t handle;
    char search[1024];

    SDL_snprintf(search, sizeof(search), "%s\\*", path);

    handle = _findfirst(search, &find_data);
    if (handle == -1) {
        SDL_SetError("%s", strerror(errno));
        return false;
    }

    do {
        const char* name = find_data.name;
#else
    DIR* dir;
    struct dirent* entry;

    dir = opendir(path);
    if (dir == NULL) {
        SDL_SetError("%s", strerror(errno));
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        const char* name = entry->d_name;
#endif

        if (SDL_strcmp(name, ".") == 0 || SDL_strcmp(name, "..") == 0) {
            continue;
        }

        if (pattern != NULL && !S3_GlobMatch(pattern, name, case_insensitive)) {
            continue;
        }

        // A truncated list would pass for a smaller directory, so fail the scan instead.
        if (!S3_GlobAppend(result, name)) {
            ok = false;
            break;
        }

#ifdef _WIN32
    } while (_findnext(handle, &find_data) == 0);

    _findclose(handle);
#else
    }

    closedir(dir);
#endif

    return ok;
}

char** S3_GlobDirectory(const char* path, const char* pattern, S3_GlobFlags flags, int* count)
{
    S3_GlobResult scan;
    char** result;
    char* text;
    int index;

    if (count != NULL) {
        *count = 0;
    }

    if (path == NULL) {
        SDL_SetError("Invalid path");
        return NULL;
    }

    SDL_memset(&scan, 0, sizeof(scan));

    if (!S3_GlobScan(path, pattern, (flags & S3_GLOB_CASEINSENSITIVE) != 0, &scan)) {
        for (index = 0; index < scan.count; index++) {
            SDL_free(scan.names[index]);
        }

        SDL_free(scan.names);
        return NULL;
    }

    // SDL3 returns one allocation: the NULL-terminated pointer array followed by the strings.
    result = (char**)SDL_malloc(sizeof(char*) * ((size_t)scan.count + 1) + scan.total_size);
    if (result == NULL) {
        for (index = 0; index < scan.count; index++) {
            SDL_free(scan.names[index]);
        }

        SDL_free(scan.names);
        SDL_SetError("Out of memory");

        return NULL;
    }

    text = (char*)(result + scan.count + 1);

    for (index = 0; index < scan.count; index++) {
        size_t length = SDL_strlen(scan.names[index]) + 1;

        result[index] = text;
        SDL_memcpy(text, scan.names[index], length);
        text += length;

        SDL_free(scan.names[index]);
    }

    result[scan.count] = NULL;
    SDL_free(scan.names);

    if (count != NULL) {
        *count = scan.count;
    }

    return result;
}

static bool s3_mkdir_one(const char* path)
{
    S3_STAT_STRUCT st;

#ifdef _WIN32
    if (_mkdir(path) == 0) {
#else
    if (mkdir(path, 0777) == 0) {
#endif
        return true;
    }

    if (errno != EEXIST) {
        return false;
    }

    if (S3_STAT(path, &st) != 0 || !S3_ISDIR(st.st_mode)) {
        errno = EEXIST;
        return false;
    }

    return true;
}

bool S3_CreateDirectory(const char* path)
{
    char tmp[1024];
    size_t len;
    size_t i;

    if (path == NULL) {
        SDL_SetError("Invalid parameter 'path'");
        return false;
    }

    len = SDL_strlcpy(tmp, path, sizeof(tmp));
    if (len >= sizeof(tmp)) {
        SDL_SetError("Path too long");
        return false;
    }

    // SDL3 creates missing parents; skip a drive or device prefix and leading separators first.
    i = 0;
    while (i < len && tmp[i] != ':' && tmp[i] != '/' && tmp[i] != '\\') {
        i++;
    }
    if (i >= len || tmp[i] != ':') {
        i = 0;
    } else {
        i++;
    }
    while (tmp[i] == '/' || tmp[i] == '\\') {
        i++;
    }

    for (; tmp[i] != '\0'; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char sep = tmp[i];
            tmp[i] = '\0';
            if (!s3_mkdir_one(tmp)) {
                SDL_SetError("%s", strerror(errno));
                return false;
            }
            tmp[i] = sep;
        }
    }

    if (!s3_mkdir_one(tmp)) {
        SDL_SetError("%s", strerror(errno));
        return false;
    }

    return true;
}

bool S3_RemovePath(const char* path)
{
    if (path == NULL) {
        SDL_SetError("Invalid parameter 'path'");
        return false;
    }

    if (remove(path) == 0) {
        return true;
    }

    if (errno == ENOENT) {
        return true;
    }

#ifdef _WIN32
    // Windows' remove() refuses directories.
    if (_rmdir(path) == 0) {
        return true;
    }

    if (errno == ENOENT) {
        return true;
    }
#endif

    SDL_SetError("%s", strerror(errno));

    return false;
}

#ifdef _WIN32
static WCHAR* s3_utf8_to_wide(const char* utf8)
{
    WCHAR* wide;
    int chars = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);

    if (chars <= 0) {
        return NULL;
    }

    wide = (WCHAR*)SDL_malloc((size_t)chars * sizeof(*wide));
    if (wide == NULL) {
        return NULL;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, chars) <= 0) {
        SDL_free(wide);
        return NULL;
    }

    return wide;
}
#endif

bool S3_RenamePath(const char* oldpath, const char* newpath)
{
#ifdef _WIN32
    WCHAR* wide_old;
    WCHAR* wide_new;
    BOOL moved;
#endif

    if (oldpath == NULL) {
        SDL_SetError("Invalid parameter 'oldpath'");
        return false;
    }

    if (newpath == NULL) {
        SDL_SetError("Invalid parameter 'newpath'");
        return false;
    }

#ifdef _WIN32
    // SDL3 replaces an existing target; the CRT's rename() on Windows refuses to.
    wide_old = s3_utf8_to_wide(oldpath);
    if (wide_old == NULL) {
        SDL_SetError("Could not convert path to UTF-16");
        return false;
    }

    wide_new = s3_utf8_to_wide(newpath);
    if (wide_new == NULL) {
        SDL_free(wide_old);
        SDL_SetError("Could not convert path to UTF-16");
        return false;
    }

    moved = MoveFileExW(wide_old, wide_new, MOVEFILE_REPLACE_EXISTING);

    SDL_free(wide_new);
    SDL_free(wide_old);

    if (!moved) {
        SDL_SetError("Couldn't rename path (%lu)", (unsigned long)GetLastError());
        return false;
    }
#else
    if (rename(oldpath, newpath) != 0) {
#ifdef __SWITCH__
        // SDL3 replaces an existing target; Horizon's file system refuses to rename onto one.
        struct stat target;
        if (stat(newpath, &target) == 0 && !S_ISDIR(target.st_mode) && remove(newpath) == 0 &&
            rename(oldpath, newpath) == 0) {
            return true;
        }
#endif
        SDL_SetError("%s", strerror(errno));
        return false;
    }
#endif

    return true;
}

const char* S3_GetBasePath(void)
{
    // SDL3 owns the returned string; SDL2 allocates a new one per call. Only success is cached.
    static char* base_path;

    if (base_path == NULL) {
        base_path = SDL_GetBasePath();
    }

    return base_path;
}

char* S3_GetPrefPath(const char* org, const char* app)
{
#if defined(__SWITCH__)
    // SDL2's Switch build has no pref path and returns NULL; SDL3 promises a created directory.
    char path[1024];
    (void)org;
    SDL_snprintf(path, sizeof path, "sdmc:/switch/%s/", (app != NULL && *app != '\0') ? app : "SDL");
    mkdir(path, 0777);
    return SDL_strdup(path);
#else
    return SDL_GetPrefPath(org, app);
#endif
}

const char* S3_GetUserFolder(S3_Folder folder)
{
    (void)folder;

    SDL_SetError("SDL_GetUserFolder is not supported by sdl3on2");

    return NULL;
}

bool S3_GetDateTimeLocalePreferences(S3_DateFormat* date_format, S3_TimeFormat* time_format)
{
    // SDL2 has no locale date or time preference, so these are SDL3's defaults.
    if (date_format != NULL) {
        *date_format = S3_DATE_FORMAT_YYYYMMDD;
    }

    if (time_format != NULL) {
        *time_format = S3_TIME_FORMAT_24HR;
    }

    return true;
}
