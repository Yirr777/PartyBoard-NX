#include <assert.h>

#include "s3_internal.h"

static_assert(S3_INIT_AUDIO == SDL_INIT_AUDIO, "SDL_INIT_AUDIO");
static_assert(S3_INIT_VIDEO == SDL_INIT_VIDEO, "SDL_INIT_VIDEO");
static_assert(S3_INIT_EVENTS == SDL_INIT_EVENTS, "SDL_INIT_EVENTS");
static_assert(S3_INIT_JOYSTICK == SDL_INIT_JOYSTICK, "SDL_INIT_JOYSTICK");
static_assert(S3_INIT_HAPTIC == SDL_INIT_HAPTIC, "SDL_INIT_HAPTIC");
static_assert(S3_INIT_GAMEPAD == SDL_INIT_GAMECONTROLLER, "SDL_INIT_GAMECONTROLLER");
static_assert(S3_INIT_SENSOR == SDL_INIT_SENSOR, "SDL_INIT_SENSOR");
static_assert((int)S3_THREAD_PRIORITY_TIME_CRITICAL == (int)SDL_THREAD_PRIORITY_TIME_CRITICAL, "SDL_ThreadPriority");

#define S3_INIT_SDL2_MASK (S3_INIT_AUDIO | S3_INIT_VIDEO | S3_INIT_EVENTS | S3_INIT_JOYSTICK | S3_INIT_HAPTIC | S3_INIT_GAMEPAD | S3_INIT_SENSOR)

static SDL_threadID s3_main_thread;
static SDL_threadID s3_events_thread;
static SDL_threadID s3_video_thread;

static void S3_NoteMainThread(void)
{
    if (s3_main_thread == 0) {
        s3_main_thread = SDL_ThreadID();
    }
}

bool S3_IsMainThread(void)
{
    const SDL_threadID current = SDL_ThreadID();

    if (s3_video_thread != 0) {
        return current == s3_video_thread;
    }

    if (s3_events_thread != 0) {
        return current == s3_events_thread;
    }

    if (s3_main_thread != 0) {
        return current == s3_main_thread;
    }

    return true;
}

bool S3_InitSubSystem(S3_InitFlags flags)
{
    Uint32 native = flags & S3_INIT_SDL2_MASK;

    S3_NoteMainThread();

    if ((flags & S3_INIT_CAMERA) != 0) {
        SDL_SetError("SDL_INIT_CAMERA is not supported by sdl3on2");
        return false;
    }

    if ((native & (S3_INIT_AUDIO | S3_INIT_VIDEO | S3_INIT_JOYSTICK | S3_INIT_GAMEPAD | S3_INIT_SENSOR)) != 0) {
        native |= SDL_INIT_EVENTS;
    }

#ifdef __SWITCH__
    // SDL2's Switch video driver would take the default NWindow for EGL, which Vulkan presents to.
    native &= ~(Uint32)SDL_INIT_VIDEO;
    // The layer reads controllers from libnx itself.
    native &= ~(Uint32)(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
#endif

    if (native != 0 && SDL_InitSubSystem(native) != 0) {
        return false;
    }

    if ((native & SDL_INIT_EVENTS) != 0) {
        s3_events_thread = SDL_ThreadID();
    }

    if ((flags & S3_INIT_VIDEO) != 0) {
#ifdef __SWITCH__
        if (!S3_SwitchInitVideo()) {
            SDL_QuitSubSystem(native);
            return false;
        }
#endif
        s3_video_thread = SDL_ThreadID();
    }

#ifdef __SWITCH__
    if ((flags & (S3_INIT_JOYSTICK | S3_INIT_GAMEPAD)) != 0) {
        S3_SwitchInitPads();
    }
#endif

    return true;
}

bool S3_Init(S3_InitFlags flags)
{
    return S3_InitSubSystem(flags);
}

void S3_QuitSubSystem(S3_InitFlags flags)
{
    Uint32 native = flags & S3_INIT_SDL2_MASK;

    if ((native & (S3_INIT_AUDIO | S3_INIT_VIDEO | S3_INIT_JOYSTICK | S3_INIT_GAMEPAD | S3_INIT_SENSOR)) != 0) {
        native |= SDL_INIT_EVENTS;
    }

#ifdef __SWITCH__
    native &= ~(Uint32)(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

    if ((flags & S3_INIT_VIDEO) != 0) {
        S3_SwitchQuitVideo(false);
    }

    if ((flags & (S3_INIT_JOYSTICK | S3_INIT_GAMEPAD)) != 0) {
        S3_SwitchQuitPads(false);
    }
#endif

    if (native != 0) {
        SDL_QuitSubSystem(native);
    }
}

S3_InitFlags S3_WasInit(S3_InitFlags flags)
{
    S3_InitFlags result = 0;

    // SDL2 reads 0 as "every subsystem", so a request for SDL2-less bits alone must not reach it.
    if (flags == 0) {
        result = (S3_InitFlags)SDL_WasInit(0) & S3_INIT_SDL2_MASK;
    } else if ((flags & S3_INIT_SDL2_MASK) != 0) {
        result = (S3_InitFlags)SDL_WasInit(flags & S3_INIT_SDL2_MASK);
    }

#ifdef __SWITCH__
    result &= ~(S3_InitFlags)(S3_INIT_VIDEO | S3_INIT_JOYSTICK | S3_INIT_GAMEPAD);

    if ((flags == 0 || (flags & S3_INIT_VIDEO) != 0) && S3_SwitchVideoInitialized()) {
        result |= S3_INIT_VIDEO;
    }

    if (S3_SwitchPadsInitialized()) {
        result |= (flags == 0 ? S3_INIT_JOYSTICK | S3_INIT_GAMEPAD : flags) & (S3_INIT_JOYSTICK | S3_INIT_GAMEPAD);
    }
#endif

    return result;
}

void S3_Quit(void)
{
#ifdef __SWITCH__
    S3_SwitchQuitVideo(true);
    S3_SwitchQuitPads(true);
#endif

    S3_QuitProperties();

    SDL_Quit();

    s3_events_thread = 0;
    s3_video_thread = 0;
}

bool S3_SetHint(const char* name, const char* value)
{
    return SDL_SetHint(name, value) == SDL_TRUE;
}

static S3_PropertiesID s3_app_metadata;

bool S3_SetAppMetadataProperty(const char* name, const char* value)
{
    S3_NoteMainThread();

    if (name == NULL || *name == '\0') {
        SDL_SetError("Parameter 'name' is invalid");
        return false;
    }

    if (s3_app_metadata == 0) {
        s3_app_metadata = S3_CreateProperties();
        if (s3_app_metadata == 0) {
            return false;
        }
    }

    return S3_SetStringProperty(s3_app_metadata, name, value);
}

bool S3_SetAppMetadata(const char* appname, const char* appversion, const char* appidentifier)
{
    S3_SetAppMetadataProperty(S3_PROP_APP_METADATA_NAME_STRING, appname);
    S3_SetAppMetadataProperty(S3_PROP_APP_METADATA_VERSION_STRING, appversion);
    S3_SetAppMetadataProperty(S3_PROP_APP_METADATA_IDENTIFIER_STRING, appidentifier);

    return true;
}

bool S3_SetCurrentThreadPriority(S3_ThreadPriority priority)
{
    return SDL_SetThreadPriority((SDL_ThreadPriority)priority) == 0;
}

S3_Mutex* S3_CreateMutex(void)
{
    return SDL_CreateMutex();
}

void S3_LockMutex(S3_Mutex* mutex)
{
    if (mutex == NULL) {
        return;
    }

    SDL_LockMutex(mutex);
}

void S3_UnlockMutex(S3_Mutex* mutex)
{
    if (mutex == NULL) {
        return;
    }

    SDL_UnlockMutex(mutex);
}

void S3_DestroyMutex(S3_Mutex* mutex)
{
    SDL_DestroyMutex(mutex);
}

Uint64 S3_GetTicks(void)
{
    return SDL_GetTicks64();
}

Uint64 S3_GetTicksNS(void)
{
    return SDL_GetTicks64() * UINT64_C(1000000);
}

Uint64 S3_GetPerformanceCounter(void)
{
    return SDL_GetPerformanceCounter();
}

Uint64 S3_GetPerformanceFrequency(void)
{
    return SDL_GetPerformanceFrequency();
}

int S3_GetSystemRAM(void)
{
    return SDL_GetSystemRAM();
}

bool S3_ShowSimpleMessageBox(S3_MessageBoxFlags flags, const char* title, const char* message, S3_Window* window)
{
#ifdef __SWITCH__
    // The console window is the layer's, not an SDL2 window.
    window = NULL;
#endif

    return SDL_ShowSimpleMessageBox((Uint32)flags, title, message, window) == 0;
}

struct S3_AudioStream {
    SDL_AudioStream* conversion;
    SDL_AudioDeviceID device;
    S3_AudioStreamCallback callback;
    void* userdata;
    Uint8 silence;
    S3_AudioSpec src;
    S3_AudioSpec dst;
    int sample_frames;
    char* device_name;
    SDL_mutex* mutex;
    // Bytes accepted in the input format and read back in the output format, under `mutex`.
    Uint64 input_put;
    Uint64 output_read;
};

static int S3_DefaultSampleFrames(int freq)
{
    if (freq <= 22050) {
        return 512;
    }

    if (freq <= 48000) {
        return 1024;
    }

    if (freq <= 96000) {
        return 2048;
    }

    return 4096;
}

static int S3_AudioFrameBytes(const S3_AudioSpec* spec)
{
    return (int)(SDL_AUDIO_BITSIZE((SDL_AudioFormat)spec->format) / 8) * spec->channels;
}

static bool S3_ValidAudioSpec(const S3_AudioSpec* spec)
{
    if (spec == NULL) {
        SDL_SetError("Parameter 'spec' is invalid");
        return false;
    }

    if (spec->channels < 1 || spec->channels > 8 || spec->freq <= 0 || SDL_AUDIO_BITSIZE((SDL_AudioFormat)spec->format) == 0) {
        SDL_SetError("Invalid audio spec");
        return false;
    }

    return true;
}

S3_AudioStream* S3_CreateAudioStream(const S3_AudioSpec* src_spec, const S3_AudioSpec* dst_spec)
{
    S3_AudioStream* stream;

    if (!S3_ValidAudioSpec(src_spec) || !S3_ValidAudioSpec(dst_spec)) {
        return NULL;
    }

    stream = (S3_AudioStream*)SDL_calloc(1, sizeof(*stream));
    if (stream == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    stream->mutex = SDL_CreateMutex();
    if (stream->mutex == NULL) {
        SDL_free(stream);
        return NULL;
    }

    stream->conversion = SDL_NewAudioStream((SDL_AudioFormat)src_spec->format,
        (Uint8)src_spec->channels,
        src_spec->freq,
        (SDL_AudioFormat)dst_spec->format,
        (Uint8)dst_spec->channels,
        dst_spec->freq);

    if (stream->conversion == NULL) {
        SDL_DestroyMutex(stream->mutex);
        SDL_free(stream);
        return NULL;
    }

    stream->src = *src_spec;
    stream->dst = *dst_spec;

    return stream;
}

static S3_AudioStream* s3_device_streams[8];

static void S3_TrackDeviceStream(S3_AudioStream* stream, bool add)
{
    size_t index;

    for (index = 0; index < SDL_arraysize(s3_device_streams); index++) {
        if (add && s3_device_streams[index] == NULL) {
            s3_device_streams[index] = stream;
            return;
        }

        if (!add && s3_device_streams[index] == stream) {
            s3_device_streams[index] = NULL;
            return;
        }
    }
}

static S3_AudioStream* S3_FindDeviceStream(S3_AudioDeviceID devid)
{
    size_t index;

    for (index = 0; index < SDL_arraysize(s3_device_streams); index++) {
        if (s3_device_streams[index] != NULL && (S3_AudioDeviceID)s3_device_streams[index]->device == devid) {
            return s3_device_streams[index];
        }
    }

    return NULL;
}

static void SDLCALL S3_AudioDeviceCallback(void* userdata, Uint8* buffer, int len)
{
    S3_AudioStream* stream = (S3_AudioStream*)userdata;
    int available;
    int got;

    SDL_LockMutex(stream->mutex);
    available = SDL_AudioStreamAvailable(stream->conversion);
    SDL_UnlockMutex(stream->mutex);

    // SDL3 asks the app for the shortfall, in bytes of the stream's input format.
    if (available < len) {
        stream->callback(stream->userdata, stream, len - available, len);
    }

    SDL_LockMutex(stream->mutex);
    got = SDL_AudioStreamGet(stream->conversion, buffer, len);
    if (got > 0) {
        stream->output_read += (Uint64)got;
    }
    SDL_UnlockMutex(stream->mutex);

    if (got < 0) {
        got = 0;
    }

    if (got < len) {
        SDL_memset(buffer + got, stream->silence, (size_t)(len - got));
    }
}

S3_AudioStream* S3_OpenAudioDeviceStream(S3_AudioDeviceID devid, const S3_AudioSpec* spec, S3_AudioStreamCallback callback, void* userdata)
{
    S3_AudioStream* stream;
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;
    char* default_name = NULL;

    if (devid != S3_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
        SDL_SetError("Only SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK is supported by sdl3on2");
        return NULL;
    }

    if (!S3_ValidAudioSpec(spec)) {
        return NULL;
    }

    stream = (S3_AudioStream*)SDL_calloc(1, sizeof(*stream));
    if (stream == NULL) {
        SDL_SetError("Out of memory");
        return NULL;
    }

    SDL_zero(desired);
    desired.freq = spec->freq;
    desired.format = (SDL_AudioFormat)spec->format;
    desired.channels = (Uint8)spec->channels;
    desired.samples = (Uint16)S3_DefaultSampleFrames(spec->freq);

    if (callback != NULL) {
        stream->mutex = SDL_CreateMutex();
        stream->conversion = SDL_NewAudioStream(desired.format, desired.channels, desired.freq,
            desired.format, desired.channels, desired.freq);
        if (stream->mutex == NULL || stream->conversion == NULL) {
            SDL_FreeAudioStream(stream->conversion);
            SDL_DestroyMutex(stream->mutex);
            SDL_free(stream);
            return NULL;
        }
        stream->callback = callback;
        stream->userdata = userdata;
        desired.callback = S3_AudioDeviceCallback;
        desired.userdata = stream;
    }

    // With no allowed changes SDL2 converts internally, so the stream stays in the caller's format.
    stream->device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (stream->device == 0) {
        SDL_FreeAudioStream(stream->conversion);
        SDL_DestroyMutex(stream->mutex);
        SDL_free(stream);
        return NULL;
    }

    stream->src = *spec;
    stream->dst.format = (S3_AudioFormat)obtained.format;
    stream->dst.channels = obtained.channels;
    stream->dst.freq = obtained.freq;
    stream->sample_frames = obtained.samples;
    stream->silence = obtained.silence;

    if (SDL_GetDefaultAudioInfo(&default_name, NULL, 0) == 0 && default_name != NULL) {
        stream->device_name = default_name;
    } else if (SDL_GetNumAudioDevices(0) > 0 && SDL_GetAudioDeviceName(0, 0) != NULL) {
        stream->device_name = SDL_strdup(SDL_GetAudioDeviceName(0, 0));
    }

    S3_TrackDeviceStream(stream, true);

    return stream;
}

S3_AudioDeviceID S3_GetAudioStreamDevice(S3_AudioStream* stream)
{
    if (stream == NULL) {
        SDL_SetError("Parameter 'stream' is invalid");
        return 0;
    }

    return (S3_AudioDeviceID)stream->device;
}

bool S3_ResumeAudioStreamDevice(S3_AudioStream* stream)
{
    if (stream == NULL || stream->device == 0) {
        SDL_SetError("Parameter 'stream' is invalid");
        return false;
    }

    SDL_PauseAudioDevice(stream->device, 0);

    return true;
}

bool S3_GetAudioDeviceFormat(S3_AudioDeviceID devid, S3_AudioSpec* spec, int* sample_frames)
{
    const S3_AudioStream* stream = S3_FindDeviceStream(devid);
    SDL_AudioSpec native;

    if (spec == NULL) {
        SDL_SetError("Parameter 'spec' is invalid");
        return false;
    }

    if (stream != NULL) {
        *spec = stream->dst;
        if (sample_frames != NULL) {
            *sample_frames = stream->sample_frames;
        }
        return true;
    }

    if (devid != S3_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
        SDL_SetError("Invalid audio device instance");
        return false;
    }

    if (SDL_GetDefaultAudioInfo(NULL, &native, 0) != 0) {
        return false;
    }

    spec->format = (S3_AudioFormat)native.format;
    spec->channels = native.channels;
    spec->freq = native.freq;

    if (sample_frames != NULL) {
        *sample_frames = native.samples != 0 ? native.samples : S3_DefaultSampleFrames(native.freq);
    }

    return true;
}

const char* S3_GetAudioDeviceName(S3_AudioDeviceID devid)
{
    const S3_AudioStream* stream = S3_FindDeviceStream(devid);

    if (stream != NULL && stream->device_name != NULL) {
        return stream->device_name;
    }

    if (stream != NULL || devid == S3_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
        return "System audio playback device";
    }

    SDL_SetError("Invalid audio device instance");

    return NULL;
}

bool S3_PutAudioStreamData(S3_AudioStream* stream, const void* buf, int len)
{
    int rc;

    if (stream == NULL) {
        SDL_SetError("Invalid audio stream");
        return false;
    }

    if (stream->device != 0 && stream->callback == NULL) {
        return SDL_QueueAudio(stream->device, buf, (Uint32)len) == 0;
    }

    SDL_LockMutex(stream->mutex);
    rc = SDL_AudioStreamPut(stream->conversion, buf, len);
    if (rc == 0 && len > 0) {
        stream->input_put += (Uint64)len;
    }
    SDL_UnlockMutex(stream->mutex);

    return rc == 0;
}

int S3_GetAudioStreamQueued(S3_AudioStream* stream)
{
    Uint64 put;
    Uint64 read;
    Uint64 used;
    Uint64 queued;
    int src_frame_bytes;
    int dst_frame_bytes;

    if (stream == NULL) {
        SDL_SetError("Invalid audio stream");
        return -1;
    }

    if (stream->device != 0 && stream->callback == NULL) {
        return (int)SDL_GetQueuedAudioSize(stream->device);
    }

    SDL_LockMutex(stream->mutex);
    put = stream->input_put;
    read = stream->output_read;
    SDL_UnlockMutex(stream->mutex);

    src_frame_bytes = S3_AudioFrameBytes(&stream->src);
    dst_frame_bytes = S3_AudioFrameBytes(&stream->dst);
    if (src_frame_bytes <= 0 || dst_frame_bytes <= 0 || stream->src.freq <= 0 || stream->dst.freq <= 0) {
        return put > (Uint64)SDL_MAX_SINT32 ? SDL_MAX_SINT32 : (int)put;
    }

    // SDL2 cannot report the input its resampler has used, so it is estimated from the output read.
    used = read * (Uint64)src_frame_bytes * (Uint64)stream->src.freq
        / ((Uint64)dst_frame_bytes * (Uint64)stream->dst.freq);
    queued = put > used ? put - used : 0;
    queued -= queued % (Uint64)src_frame_bytes;

    return queued > (Uint64)SDL_MAX_SINT32 ? SDL_MAX_SINT32 : (int)queued;
}

int S3_GetAudioStreamData(S3_AudioStream* stream, void* buf, int len)
{
    int got;

    // A device stream with no callback is SDL2's own queue, which has nothing to read back.
    if (stream == NULL || stream->conversion == NULL) {
        SDL_SetError("Invalid audio stream");
        return -1;
    }

    SDL_LockMutex(stream->mutex);
    got = SDL_AudioStreamGet(stream->conversion, buf, len);
    if (got > 0) {
        stream->output_read += (Uint64)got;
    }
    SDL_UnlockMutex(stream->mutex);

    return got;
}

int S3_GetAudioStreamInputRate(S3_AudioStream* stream)
{
    return stream != NULL ? stream->src.freq : 0;
}

void S3_DestroyAudioStream(S3_AudioStream* stream)
{
    if (stream == NULL) {
        return;
    }

    if (stream->device != 0) {
        S3_TrackDeviceStream(stream, false);
        SDL_CloseAudioDevice(stream->device);
        SDL_free(stream->device_name);
    }

    if (stream->conversion != NULL) {
        SDL_LockMutex(stream->mutex);
        SDL_FreeAudioStream(stream->conversion);
        SDL_UnlockMutex(stream->mutex);
        SDL_DestroyMutex(stream->mutex);
    }

    SDL_free(stream);
}
