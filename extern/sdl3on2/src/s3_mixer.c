#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "s3_internal.h"

#include <SDL3_mixer/s3_mixer_defs.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"

#define S3_DEFAULT_FREQ 22050
#define S3_DEFAULT_CHANNELS 2
#define S3_DEVICE_SAMPLES 1024

// Mixing granularity; bounds the per-mixer scratch buffers.
#define S3_MIX_CHUNK_FRAMES 512

#define S3_INITIAL_TRACKS 64

// One MP3 frame is 1152 PCM frames; decode a few at a time.
#define S3_MP3_DECODE_FRAMES 4608

typedef enum S3_AudioKind {
    S3_AUDIO_PCM, // fully decoded, already in device format
    S3_AUDIO_MP3 // compressed, decoded on demand while mixing
} S3_AudioKind;

struct MIX_Audio {
    S3_AudioKind kind;
    int refcount; // one app reference plus one per bound track

    Uint8* pcm;
    Sint64 pcm_frames;
    int pcm_channels;
    int pcm_freq;

    // Exactly one of these is set for S3_AUDIO_MP3.
    Uint8* mp3_data;
    size_t mp3_size;
    char* mp3_path;

    int mp3_channels;
    int mp3_freq;
    Sint64 mp3_frames; // -1 when the duration is not known

    MIX_Audio* next; // global registry, for MIX_Quit()
};

struct MIX_Track {
    MIX_Mixer* mixer;

    // At most one of these is non-NULL.
    MIX_Audio* audio;
    S3_AudioStream* appstream;

    // Mutated only with the audio device locked.
    bool playing;
    bool paused;
    int loops; // loops still to run; -1 is infinite
    float gain;
    bool spatialized;
    MIX_StereoGains gains;
    Sint64 fade_total; // device frames; 0 when not fading
    Sint64 fade_left;

    Sint64 pcm_pos;

    bool mp3_ready;
    bool mp3_eof;
    drmp3 mp3;
    SDL_RWops* mp3_rw;
    SDL_AudioStream* mp3_cvt;
    Sint16* mp3_buf;
    int mp3_buf_frames;
};

struct MIX_Mixer {
    SDL_AudioDeviceID devid;
    SDL_AudioSpec spec; // the SDL2 obtained spec
    float gain;

    MIX_Track** tracks;
    int num_tracks;
    int max_tracks;

    Sint32* accum; // S3_MIX_CHUNK_FRAMES * channels
    Sint16* scratch; // S3_MIX_CHUNK_FRAMES * channels

    MIX_Mixer* next; // global registry, for MIX_Quit()
};

static int s3_init_count;
static MIX_Mixer* s3_mixers;
static MIX_Audio* s3_audios;

static int s3_track_input_rate(MIX_Track* track);

static bool s3_fail(const char* what)
{
    SDL_SetError("%s", what);
    return false;
}

static void* s3_fail_ptr(const char* what)
{
    SDL_SetError("%s", what);
    return NULL;
}

static void s3_lock(MIX_Mixer* mixer)
{
    if (mixer != NULL && mixer->devid != 0) {
        SDL_LockAudioDevice(mixer->devid);
    }
}

static void s3_unlock(MIX_Mixer* mixer)
{
    if (mixer != NULL && mixer->devid != 0) {
        SDL_UnlockAudioDevice(mixer->devid);
    }
}

static void* s3_drmp3_malloc(size_t sz, void* userdata)
{
    (void)userdata;
    return SDL_malloc(sz);
}

static void* s3_drmp3_realloc(void* ptr, size_t sz, void* userdata)
{
    (void)userdata;
    return SDL_realloc(ptr, sz);
}

static void s3_drmp3_free(void* ptr, void* userdata)
{
    (void)userdata;
    SDL_free(ptr);
}

static const drmp3_allocation_callbacks s3_drmp3_allocs = {
    NULL,
    s3_drmp3_malloc,
    s3_drmp3_realloc,
    s3_drmp3_free,
};

static size_t s3_drmp3_read(void* userdata, void* out, size_t size)
{
    return SDL_RWread((SDL_RWops*)userdata, out, 1, size);
}

static drmp3_bool32 s3_drmp3_seek(void* userdata, int offset, drmp3_seek_origin origin)
{
    int whence = RW_SEEK_SET;

    if (origin == DRMP3_SEEK_CUR) {
        whence = RW_SEEK_CUR;
    } else if (origin == DRMP3_SEEK_END) {
        whence = RW_SEEK_END;
    }

    return SDL_RWseek((SDL_RWops*)userdata, offset, whence) < 0 ? DRMP3_FALSE : DRMP3_TRUE;
}

static drmp3_bool32 s3_drmp3_tell(void* userdata, drmp3_int64* cursor)
{
    Sint64 pos = SDL_RWtell((SDL_RWops*)userdata);

    if (pos < 0) {
        return DRMP3_FALSE;
    }

    *cursor = (drmp3_int64)pos;
    return DRMP3_TRUE;
}

static drmp3_bool32 s3_drmp3_init_rw(drmp3* mp3, SDL_RWops* rw)
{
    return drmp3_init(mp3, s3_drmp3_read, s3_drmp3_seek, s3_drmp3_tell, NULL, rw, &s3_drmp3_allocs);
}

// SDL2's SDL_AudioStreamPut bypasses the staging buffer for a write at least its size, and
// SDL_AudioStreamFlush then drops the resampler tail. A final one-frame put of silence goes
// through the staging buffer, so the flush emits the tail.
static bool s3_stream_finish(SDL_AudioStream* cvt, SDL_AudioFormat src_format, int src_channels)
{
    // Large enough for one frame of any SDL2 format at up to 8 channels.
    Uint8 silence[8 * 8];
    const int frame_bytes = (SDL_AUDIO_BITSIZE(src_format) / 8) * src_channels;

    if (frame_bytes <= 0 || frame_bytes > (int)sizeof(silence)) {
        return SDL_AudioStreamFlush(cvt) >= 0;
    }

    SDL_memset(silence, SDL_AUDIO_ISUNSIGNED(src_format) ? 0x80 : 0x00, sizeof(silence));

    if (SDL_AudioStreamPut(cvt, silence, frame_bytes) < 0) {
        return false;
    }

    return SDL_AudioStreamFlush(cvt) >= 0;
}

static bool s3_stream_extract(SDL_AudioStream* cvt, int frame_bytes, Uint8** out_pcm, Sint64* out_frames)
{
    int available;
    Uint8* buffer;
    int got;

    available = SDL_AudioStreamAvailable(cvt);
    if (available < 0) {
        return false;
    }

    available -= available % frame_bytes;

    buffer = (Uint8*)SDL_malloc(available > 0 ? (size_t)available : 1);
    if (buffer == NULL) {
        return s3_fail("Out of memory");
    }

    got = available > 0 ? SDL_AudioStreamGet(cvt, buffer, available) : 0;
    if (got < 0) {
        SDL_free(buffer);
        return false;
    }

    *out_pcm = buffer;
    *out_frames = got / frame_bytes;
    return true;
}

static MIX_Audio* s3_audio_new(S3_AudioKind kind)
{
    MIX_Audio* audio = (MIX_Audio*)SDL_calloc(1, sizeof(*audio));

    if (audio == NULL) {
        return (MIX_Audio*)s3_fail_ptr("Out of memory");
    }

    audio->kind = kind;
    audio->refcount = 1;
    audio->mp3_frames = -1;

    audio->next = s3_audios;
    s3_audios = audio;

    return audio;
}

static void s3_audio_free(MIX_Audio* audio)
{
    MIX_Audio** link;

    for (link = &s3_audios; *link != NULL; link = &(*link)->next) {
        if (*link == audio) {
            *link = audio->next;
            break;
        }
    }

    SDL_free(audio->pcm);
    SDL_free(audio->mp3_data);
    SDL_free(audio->mp3_path);
    SDL_free(audio);
}

static void s3_audio_unref(MIX_Audio* audio)
{
    if (audio != NULL && --audio->refcount <= 0) {
        s3_audio_free(audio);
    }
}

static void s3_mixer_target(MIX_Mixer* mixer, SDL_AudioFormat* format, int* channels, int* freq)
{
    if (mixer != NULL) {
        *format = mixer->spec.format;
        *channels = mixer->spec.channels;
        *freq = mixer->spec.freq;
    } else {
        *format = AUDIO_S16SYS;
        *channels = S3_DEFAULT_CHANNELS;
        *freq = S3_DEFAULT_FREQ;
    }
}

static MIX_Audio* s3_load_wav(MIX_Mixer* mixer, const Uint8* data, size_t size)
{
    SDL_AudioSpec wav_spec;
    Uint8* wav_buffer = NULL;
    Uint32 wav_length = 0;
    SDL_RWops* rw;
    SDL_AudioStream* cvt;
    SDL_AudioFormat dst_format;
    int dst_channels;
    int dst_freq;
    MIX_Audio* audio;

    if (size > (size_t)INT_MAX) {
        return (MIX_Audio*)s3_fail_ptr("WAV is too large");
    }

    rw = SDL_RWFromConstMem(data, (int)size);
    if (rw == NULL) {
        return NULL;
    }

    if (SDL_LoadWAV_RW(rw, 1, &wav_spec, &wav_buffer, &wav_length) == NULL) {
        return NULL;
    }

    s3_mixer_target(mixer, &dst_format, &dst_channels, &dst_freq);

    cvt = SDL_NewAudioStream(wav_spec.format, wav_spec.channels, wav_spec.freq,
        dst_format, (Uint8)dst_channels, dst_freq);
    if (cvt == NULL) {
        SDL_FreeWAV(wav_buffer);
        return NULL;
    }

    audio = s3_audio_new(S3_AUDIO_PCM);
    if (audio == NULL) {
        SDL_FreeAudioStream(cvt);
        SDL_FreeWAV(wav_buffer);
        return NULL;
    }

    if (SDL_AudioStreamPut(cvt, wav_buffer, (int)wav_length) < 0
        || !s3_stream_finish(cvt, wav_spec.format, wav_spec.channels)
        || !s3_stream_extract(cvt, dst_channels * (int)sizeof(Sint16), &audio->pcm, &audio->pcm_frames)) {
        s3_audio_free(audio);
        SDL_FreeAudioStream(cvt);
        SDL_FreeWAV(wav_buffer);
        return NULL;
    }

    audio->pcm_channels = dst_channels;
    audio->pcm_freq = dst_freq;

    SDL_FreeAudioStream(cvt);
    SDL_FreeWAV(wav_buffer);

    return audio;
}

static MIX_Audio* s3_predecode_mp3(MIX_Mixer* mixer, SDL_RWops* rw)
{
    drmp3 mp3;
    SDL_AudioStream* cvt;
    SDL_AudioFormat dst_format;
    int dst_channels;
    int dst_freq;
    MIX_Audio* audio;
    Sint16 block[1152 * 2];
    drmp3_uint64 decoded;
    bool ok = true;

    if (!s3_drmp3_init_rw(&mp3, rw)) {
        return (MIX_Audio*)s3_fail_ptr("Unrecognized audio format");
    }

    s3_mixer_target(mixer, &dst_format, &dst_channels, &dst_freq);

    cvt = SDL_NewAudioStream(AUDIO_S16SYS, (Uint8)mp3.channels, (int)mp3.sampleRate,
        dst_format, (Uint8)dst_channels, dst_freq);
    if (cvt == NULL) {
        drmp3_uninit(&mp3);
        return NULL;
    }

    for (;;) {
        decoded = drmp3_read_pcm_frames_s16(&mp3, 1152, block);
        if (decoded == 0) {
            break;
        }

        if (SDL_AudioStreamPut(cvt, block, (int)(decoded * mp3.channels * sizeof(Sint16))) < 0) {
            ok = false;
            break;
        }
    }

    if (ok) {
        ok = s3_stream_finish(cvt, AUDIO_S16SYS, (int)mp3.channels);
    }

    audio = ok ? s3_audio_new(S3_AUDIO_PCM) : NULL;
    if (audio != NULL) {
        if (s3_stream_extract(cvt, dst_channels * (int)sizeof(Sint16), &audio->pcm, &audio->pcm_frames)) {
            audio->pcm_channels = dst_channels;
            audio->pcm_freq = dst_freq;
        } else {
            s3_audio_free(audio);
            audio = NULL;
        }
    }

    SDL_FreeAudioStream(cvt);
    drmp3_uninit(&mp3);

    return audio;
}

static bool s3_probe_mp3(SDL_RWops* rw, MIX_Audio* audio)
{
    drmp3 mp3;

    if (SDL_RWseek(rw, 0, RW_SEEK_SET) < 0) {
        return false;
    }

    if (!s3_drmp3_init_rw(&mp3, rw)) {
        return s3_fail("Unrecognized audio format");
    }

    audio->mp3_channels = (int)mp3.channels;
    audio->mp3_freq = (int)mp3.sampleRate;
    audio->mp3_frames = mp3.totalPCMFrameCount == DRMP3_UINT64_MAX
        ? -1
        : (Sint64)mp3.totalPCMFrameCount;

    drmp3_uninit(&mp3);
    return true;
}

// LoadAudio_IO on the SDL2 stream itself, which MIX_LoadAudio opens without a layer wrapper.
static MIX_Audio* s3_load_audio_rw(MIX_Mixer* mixer, SDL_RWops* rw, bool predecode, bool closerw)
{
    Uint8* data;
    size_t size = 0;
    MIX_Audio* audio = NULL;
    bool is_wav;

    if (rw == NULL) {
        return (MIX_Audio*)s3_fail_ptr("SDL_IOStream is NULL");
    }

    data = (Uint8*)SDL_LoadFile_RW(rw, &size, closerw ? 1 : 0);
    if (data == NULL) {
        return NULL;
    }

    is_wav = size >= 12 && SDL_memcmp(data, "RIFF", 4) == 0 && SDL_memcmp(data + 8, "WAVE", 4) == 0;

    if (is_wav) {
        audio = s3_load_wav(mixer, data, size);
    } else if (size > (size_t)INT_MAX) {
        audio = (MIX_Audio*)s3_fail_ptr("Audio file is too large");
    } else if (predecode) {
        SDL_RWops* mem = SDL_RWFromConstMem(data, (int)size);
        if (mem != NULL) {
            audio = s3_predecode_mp3(mixer, mem);
            SDL_RWclose(mem);
        }
    } else {
        SDL_RWops* mem = SDL_RWFromConstMem(data, (int)size);
        if (mem != NULL) {
            audio = s3_audio_new(S3_AUDIO_MP3);
            if (audio != NULL && !s3_probe_mp3(mem, audio)) {
                s3_audio_free(audio);
                audio = NULL;
            }
            SDL_RWclose(mem);
        }

        if (audio != NULL) {
            audio->mp3_data = data;
            audio->mp3_size = size;
            data = NULL;
        }
    }

    SDL_free(data);

    return audio;
}

MIX_Audio* S3_MIX_LoadAudio_IO(MIX_Mixer* mixer, S3_IOStream* io, bool predecode, bool closeio)
{
    MIX_Audio* audio;

    if (io == NULL) {
        return (MIX_Audio*)s3_fail_ptr("SDL_IOStream is NULL");
    }

    audio = s3_load_audio_rw(mixer, S3_UnwrapIO(io), predecode, false);
    // Upstream closes the stream whether or not the load succeeded.
    if (closeio) {
        S3_CloseIO(io);
    }

    return audio;
}

MIX_Audio* S3_MIX_LoadAudio(MIX_Mixer* mixer, const char* path, bool predecode)
{
    SDL_RWops* rw;
    Uint8 header[12];
    bool is_wav;
    MIX_Audio* audio;

    if (path == NULL) {
        return (MIX_Audio*)s3_fail_ptr("Path is NULL");
    }

    rw = SDL_RWFromFile(path, "rb");
    if (rw == NULL) {
        return NULL;
    }

    is_wav = SDL_RWread(rw, header, 1, sizeof(header)) == sizeof(header)
        && SDL_memcmp(header, "RIFF", 4) == 0
        && SDL_memcmp(header + 8, "WAVE", 4) == 0;

    if (predecode || is_wav) {
        if (SDL_RWseek(rw, 0, RW_SEEK_SET) < 0) {
            SDL_RWclose(rw);
            return NULL;
        }
        return s3_load_audio_rw(mixer, rw, predecode, true);
    }

    audio = s3_audio_new(S3_AUDIO_MP3);
    if (audio == NULL) {
        SDL_RWclose(rw);
        return NULL;
    }

    if (!s3_probe_mp3(rw, audio)) {
        s3_audio_free(audio);
        SDL_RWclose(rw);
        return NULL;
    }

    SDL_RWclose(rw);

    audio->mp3_path = SDL_strdup(path);
    if (audio->mp3_path == NULL) {
        s3_audio_free(audio);
        return (MIX_Audio*)s3_fail_ptr("Out of memory");
    }

    return audio;
}

void S3_MIX_DestroyAudio(MIX_Audio* audio)
{
    s3_audio_unref(audio);
}

Sint64 S3_MIX_GetAudioDuration(MIX_Audio* audio)
{
    if (audio == NULL) {
        return -1;
    }

    return audio->kind == S3_AUDIO_PCM ? audio->pcm_frames : audio->mp3_frames;
}

static void s3_track_close_decoder(MIX_Track* track)
{
    if (track->mp3_ready) {
        drmp3_uninit(&track->mp3);
        track->mp3_ready = false;
    }

    if (track->mp3_rw != NULL) {
        SDL_RWclose(track->mp3_rw);
        track->mp3_rw = NULL;
    }

    if (track->mp3_cvt != NULL) {
        SDL_FreeAudioStream(track->mp3_cvt);
        track->mp3_cvt = NULL;
    }

    SDL_free(track->mp3_buf);
    track->mp3_buf = NULL;
    track->mp3_buf_frames = 0;
    track->mp3_eof = false;
}

static bool s3_track_open_decoder(MIX_Track* track)
{
    MIX_Audio* audio = track->audio;
    MIX_Mixer* mixer = track->mixer;

    if (audio == NULL || audio->kind != S3_AUDIO_MP3) {
        return true;
    }

    if (audio->mp3_path != NULL) {
        track->mp3_rw = SDL_RWFromFile(audio->mp3_path, "rb");
    } else if (audio->mp3_data != NULL && audio->mp3_size <= (size_t)INT_MAX) {
        track->mp3_rw = SDL_RWFromConstMem(audio->mp3_data, (int)audio->mp3_size);
    }

    if (track->mp3_rw == NULL) {
        s3_track_close_decoder(track);
        return s3_fail("Could not open MP3 source");
    }

    if (!s3_drmp3_init_rw(&track->mp3, track->mp3_rw)) {
        s3_track_close_decoder(track);
        return s3_fail("Unrecognized audio format");
    }
    track->mp3_ready = true;

    track->mp3_cvt = SDL_NewAudioStream(AUDIO_S16SYS, (Uint8)track->mp3.channels, (int)track->mp3.sampleRate,
        mixer->spec.format, mixer->spec.channels, mixer->spec.freq);
    if (track->mp3_cvt == NULL) {
        s3_track_close_decoder(track);
        return false;
    }

    track->mp3_buf_frames = S3_MP3_DECODE_FRAMES;
    track->mp3_buf = (Sint16*)SDL_malloc((size_t)track->mp3_buf_frames * track->mp3.channels * sizeof(Sint16));
    if (track->mp3_buf == NULL) {
        s3_track_close_decoder(track);
        return s3_fail("Out of memory");
    }

    return true;
}

// Call with the device locked.
static void s3_track_rewind(MIX_Track* track)
{
    track->pcm_pos = 0;

    if (track->mp3_ready) {
        drmp3_seek_to_pcm_frame(&track->mp3, 0);
        track->mp3_eof = false;
        if (track->mp3_cvt != NULL) {
            SDL_AudioStreamClear(track->mp3_cvt);
        }
    }
}

static void s3_track_stop(MIX_Track* track)
{
    track->playing = false;
    track->paused = false;
    track->fade_total = 0;
    track->fade_left = 0;
}

static int s3_pull_pcm(MIX_Track* track, Sint16* buffer, int frames)
{
    MIX_Audio* audio = track->audio;
    // Audio can come from another mixer; this path cannot convert channel layouts.
    const int channels = audio->pcm_channels;
    int done = 0;

    if (channels <= 0 || channels != track->mixer->spec.channels) {
        s3_track_stop(track);
        return 0;
    }

    while (done < frames) {
        Sint64 available = audio->pcm_frames - track->pcm_pos;
        int take;

        if (available <= 0) {
            if (track->loops == 0 || audio->pcm_frames <= 0) {
                s3_track_stop(track);
                break;
            }

            if (track->loops > 0) {
                track->loops--;
            }

            track->pcm_pos = 0;
            continue;
        }

        take = frames - done;
        if ((Sint64)take > available) {
            take = (int)available;
        }

        SDL_memcpy(buffer + (size_t)done * channels,
            audio->pcm + (size_t)track->pcm_pos * channels * sizeof(Sint16),
            (size_t)take * channels * sizeof(Sint16));

        track->pcm_pos += take;
        done += take;
    }

    return done;
}

static int s3_pull_mp3(MIX_Track* track, Sint16* buffer, int frames)
{
    const int frame_bytes = track->mixer->spec.channels * (int)sizeof(Sint16);
    const int want = frames * frame_bytes;
    int empty_decodes = 0;
    int got;

    if (!track->mp3_ready || track->mp3_cvt == NULL) {
        s3_track_stop(track);
        return 0;
    }

    while (!track->mp3_eof && SDL_AudioStreamAvailable(track->mp3_cvt) < want) {
        drmp3_uint64 decoded = drmp3_read_pcm_frames_s16(&track->mp3,
            (drmp3_uint64)track->mp3_buf_frames, track->mp3_buf);

        if (decoded == 0) {
            // Guard against a source that yields nothing after a rewind.
            if (track->loops != 0 && empty_decodes < 1 && drmp3_seek_to_pcm_frame(&track->mp3, 0)) {
                if (track->loops > 0) {
                    track->loops--;
                }
                empty_decodes++;
                continue;
            }

            track->mp3_eof = true;
            s3_stream_finish(track->mp3_cvt, AUDIO_S16SYS, (int)track->mp3.channels);
            break;
        }

        empty_decodes = 0;
        SDL_AudioStreamPut(track->mp3_cvt, track->mp3_buf,
            (int)(decoded * track->mp3.channels * sizeof(Sint16)));
    }

    got = SDL_AudioStreamGet(track->mp3_cvt, buffer, want);
    if (got <= 0) {
        if (track->mp3_eof) {
            s3_track_stop(track);
        }
        return 0;
    }

    return got / frame_bytes;
}

static int s3_pull_appstream(MIX_Track* track, Sint16* buffer, int frames)
{
    const int frame_bytes = track->mixer->spec.channels * (int)sizeof(Sint16);
    int got = S3_GetAudioStreamData(track->appstream, buffer, frames * frame_bytes);

    if (got <= 0) {
        // MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN defaults to true.
        s3_track_stop(track);
        return 0;
    }

    return got / frame_bytes;
}

static int s3_track_pull(MIX_Track* track, Sint16* buffer, int frames)
{
    if (track->appstream != NULL) {
        return s3_pull_appstream(track, buffer, frames);
    }

    if (track->audio == NULL) {
        s3_track_stop(track);
        return 0;
    }

    if (track->audio->kind == S3_AUDIO_PCM) {
        return s3_pull_pcm(track, buffer, frames);
    }

    return s3_pull_mp3(track, buffer, frames);
}

static void s3_track_accumulate(MIX_Mixer* mixer, MIX_Track* track, Sint32* accum, int frames)
{
    const int channels = mixer->spec.channels;
    const Sint16* src = mixer->scratch;
    const float base = track->gain * mixer->gain;
    const float left = track->spatialized ? base * track->gains.left : base;
    const float right = track->spatialized ? base * track->gains.right : base;
    int frame;
    int channel;

    for (frame = 0; frame < frames; frame++) {
        float fade = 1.0f;

        if (track->fade_total > 0) {
            if (track->fade_left > 0) {
                fade = (float)track->fade_left / (float)track->fade_total;
                track->fade_left--;
            } else {
                fade = 0.0f;
            }
        }

        for (channel = 0; channel < channels; channel++) {
            const int index = frame * channels + channel;
            // Panning assumes stereo; other channel layouts are not spatialized correctly.
            const float gain = channel == 1 ? right : left;
            accum[index] += (Sint32)((float)src[index] * gain * fade);
        }
    }

    if (track->fade_total > 0 && track->fade_left <= 0) {
        s3_track_stop(track);
    }
}

static void s3_mix_chunk(MIX_Mixer* mixer, Sint16* out, int frames)
{
    const int channels = mixer->spec.channels;
    const int samples = frames * channels;
    Sint32* accum = mixer->accum;
    bool mixed = false;
    int index;
    int sample;

    for (index = 0; index < mixer->num_tracks; index++) {
        MIX_Track* track = mixer->tracks[index];
        int got;

        if (track == NULL || !track->playing || track->paused) {
            continue;
        }

        got = s3_track_pull(track, mixer->scratch, frames);
        if (got <= 0) {
            continue;
        }

        if (!mixed) {
            SDL_memset(accum, 0, (size_t)samples * sizeof(Sint32));
            mixed = true;
        }

        s3_track_accumulate(mixer, track, accum, got);
    }

    if (!mixed) {
        return; // the output buffer is already silent
    }

    for (sample = 0; sample < samples; sample++) {
        Sint32 value = accum[sample];

        if (value > 32767) {
            value = 32767;
        } else if (value < -32768) {
            value = -32768;
        }

        out[sample] = (Sint16)value;
    }
}

static void SDLCALL s3_audio_callback(void* userdata, Uint8* out, int len)
{
    MIX_Mixer* mixer = (MIX_Mixer*)userdata;
    const int frame_bytes = mixer->spec.channels * (int)sizeof(Sint16);
    Sint16* cursor = (Sint16*)out;
    int frames_left = len / frame_bytes;

    SDL_memset(out, 0, (size_t)len);

    while (frames_left > 0) {
        const int chunk = frames_left > S3_MIX_CHUNK_FRAMES ? S3_MIX_CHUNK_FRAMES : frames_left;

        s3_mix_chunk(mixer, cursor, chunk);

        cursor += (size_t)chunk * mixer->spec.channels;
        frames_left -= chunk;
    }
}

int S3_MIX_Version(void)
{
    return 3 * 1000000 + 2 * 1000 + 2;
}

bool S3_MIX_Init(void)
{
    if (s3_init_count == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
            return false;
        }
    }

    s3_init_count++;
    return true;
}

void S3_MIX_Quit(void)
{
    if (s3_init_count == 0) {
        return;
    }

    if (--s3_init_count > 0) {
        return;
    }

    // Upstream frees every mixer, its tracks, and every MIX_Audio whatever references remain.
    while (s3_mixers != NULL) {
        S3_MIX_DestroyMixer(s3_mixers);
    }

    while (s3_audios != NULL) {
        s3_audio_free(s3_audios);
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

MIX_Mixer* S3_MIX_CreateMixerDevice(S3_AudioDeviceID devid, const S3_AudioSpec* spec)
{
    SDL_AudioSpec desired;
    MIX_Mixer* mixer;

    (void)devid; // only the default playback device is supported

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        return NULL;
    }

    mixer = (MIX_Mixer*)SDL_calloc(1, sizeof(*mixer));
    if (mixer == NULL) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return (MIX_Mixer*)s3_fail_ptr("Out of memory");
    }

    SDL_zero(desired);
    desired.freq = spec != NULL && spec->freq > 0 ? spec->freq : S3_DEFAULT_FREQ;
    desired.channels = spec != NULL && spec->channels > 0 ? (Uint8)spec->channels : S3_DEFAULT_CHANNELS;
    desired.format = AUDIO_S16SYS;
    desired.samples = S3_DEVICE_SAMPLES;
    desired.callback = s3_audio_callback;
    desired.userdata = mixer;

    // Format and channel count must not drift: the mixer works in S16.
    mixer->devid = SDL_OpenAudioDevice(NULL, 0, &desired, &mixer->spec, SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (mixer->devid == 0) {
        SDL_free(mixer);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return NULL;
    }

    mixer->gain = 1.0f;
    mixer->max_tracks = S3_INITIAL_TRACKS;
    mixer->tracks = (MIX_Track**)SDL_calloc((size_t)mixer->max_tracks, sizeof(MIX_Track*));
    mixer->accum = (Sint32*)SDL_calloc((size_t)S3_MIX_CHUNK_FRAMES * mixer->spec.channels, sizeof(Sint32));
    mixer->scratch = (Sint16*)SDL_calloc((size_t)S3_MIX_CHUNK_FRAMES * mixer->spec.channels, sizeof(Sint16));

    if (mixer->tracks == NULL || mixer->accum == NULL || mixer->scratch == NULL) {
        SDL_CloseAudioDevice(mixer->devid);
        SDL_free(mixer->tracks);
        SDL_free(mixer->accum);
        SDL_free(mixer->scratch);
        SDL_free(mixer);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return (MIX_Mixer*)s3_fail_ptr("Out of memory");
    }

    mixer->next = s3_mixers;
    s3_mixers = mixer;

    SDL_PauseAudioDevice(mixer->devid, 0);

    return mixer;
}

// Pausing the device preserves every track's playback position.
void S3_MIX_SetMixerDevicePaused(MIX_Mixer* mixer, bool paused)
{
    if (mixer != NULL) {
        SDL_PauseAudioDevice(mixer->devid, paused ? 1 : 0);
    }
}

void S3_MIX_DestroyMixer(MIX_Mixer* mixer)
{
    MIX_Mixer** link;
    int index;

    if (mixer == NULL) {
        return;
    }

    // Stops the callback before anything is torn down.
    SDL_CloseAudioDevice(mixer->devid);
    mixer->devid = 0;

    for (index = 0; index < mixer->num_tracks; index++) {
        MIX_Track* track = mixer->tracks[index];

        s3_track_close_decoder(track);
        s3_audio_unref(track->audio);
        SDL_free(track);
    }

    for (link = &s3_mixers; *link != NULL; link = &(*link)->next) {
        if (*link == mixer) {
            *link = mixer->next;
            break;
        }
    }

    SDL_free(mixer->tracks);
    SDL_free(mixer->accum);
    SDL_free(mixer->scratch);
    SDL_free(mixer);

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool S3_MIX_GetMixerFormat(MIX_Mixer* mixer, S3_AudioSpec* spec)
{
    if (mixer == NULL) {
        if (spec != NULL) {
            SDL_zerop(spec);
        }

        return s3_fail("Mixer is NULL");
    }

    if (spec == NULL) {
        return s3_fail("Spec is NULL");
    }

    // SDL2 and SDL3 use the same SDL_AudioFormat bit encoding.
    spec->format = (S3_AudioFormat)mixer->spec.format;
    spec->channels = mixer->spec.channels;
    spec->freq = mixer->spec.freq;

    return true;
}

void S3_MIX_LockMixer(MIX_Mixer* mixer)
{
    s3_lock(mixer);
}

void S3_MIX_UnlockMixer(MIX_Mixer* mixer)
{
    s3_unlock(mixer);
}

bool S3_MIX_SetMixerGain(MIX_Mixer* mixer, float gain)
{
    if (mixer == NULL) {
        return s3_fail("Mixer is NULL");
    }

    // Upstream rejects a negative mixer gain; MIX_SetTrackGain clamps instead.
    if (gain < 0.0f) {
        return s3_fail("Parameter 'gain' is invalid");
    }

    s3_lock(mixer);
    mixer->gain = gain;
    s3_unlock(mixer);

    return true;
}

float S3_MIX_GetMixerGain(MIX_Mixer* mixer)
{
    float gain;

    if (mixer == NULL) {
        return 1.0f;
    }

    s3_lock(mixer);
    gain = mixer->gain;
    s3_unlock(mixer);

    return gain;
}

MIX_Track* S3_MIX_CreateTrack(MIX_Mixer* mixer)
{
    MIX_Track* track;

    if (mixer == NULL) {
        return (MIX_Track*)s3_fail_ptr("Mixer is NULL");
    }

    track = (MIX_Track*)SDL_calloc(1, sizeof(*track));
    if (track == NULL) {
        return (MIX_Track*)s3_fail_ptr("Out of memory");
    }

    track->mixer = mixer;
    track->gain = 1.0f;
    track->gains.left = 1.0f;
    track->gains.right = 1.0f;

    s3_lock(mixer);

    if (mixer->num_tracks == mixer->max_tracks) {
        const int wanted = mixer->max_tracks * 2;
        MIX_Track** grown = (MIX_Track**)SDL_realloc(mixer->tracks, (size_t)wanted * sizeof(MIX_Track*));

        if (grown == NULL) {
            s3_unlock(mixer);
            SDL_free(track);
            return (MIX_Track*)s3_fail_ptr("Out of memory");
        }

        mixer->tracks = grown;
        mixer->max_tracks = wanted;
    }

    mixer->tracks[mixer->num_tracks++] = track;

    s3_unlock(mixer);

    return track;
}

void S3_MIX_DestroyTrack(MIX_Track* track)
{
    MIX_Mixer* mixer;
    int index;

    if (track == NULL) {
        return;
    }

    mixer = track->mixer;

    s3_lock(mixer);

    for (index = 0; index < mixer->num_tracks; index++) {
        if (mixer->tracks[index] == track) {
            mixer->tracks[index] = mixer->tracks[mixer->num_tracks - 1];
            mixer->num_tracks--;
            break;
        }
    }

    s3_track_stop(track);

    s3_unlock(mixer);

    s3_track_close_decoder(track);
    s3_audio_unref(track->audio);
    SDL_free(track);
}

MIX_Mixer* S3_MIX_GetTrackMixer(MIX_Track* track)
{
    return track != NULL ? track->mixer : NULL;
}

bool S3_MIX_SetTrackAudio(MIX_Track* track, MIX_Audio* audio)
{
    MIX_Audio* previous;
    bool ok = true;

    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);

    // Replacing the input preserves the playing and paused state.
    s3_track_close_decoder(track);

    previous = track->audio;
    track->audio = audio;
    track->appstream = NULL;
    track->pcm_pos = 0;

    if (audio != NULL) {
        audio->refcount++;
        ok = s3_track_open_decoder(track);
        if (!ok) {
            audio->refcount--;
            track->audio = NULL;
        }
    }

    s3_unlock(track->mixer);

    s3_audio_unref(previous);

    return ok;
}

bool S3_MIX_SetTrackAudioStream(MIX_Track* track, S3_AudioStream* stream)
{
    MIX_Audio* previous;

    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);

    // Preserve the playing and paused state, as in MIX_SetTrackAudio.
    s3_track_close_decoder(track);

    previous = track->audio;
    track->audio = NULL;
    track->appstream = stream;
    track->pcm_pos = 0;

    s3_unlock(track->mixer);

    s3_audio_unref(previous);

    return true;
}

MIX_Audio* S3_MIX_GetTrackAudio(MIX_Track* track)
{
    MIX_Audio* audio;

    if (track == NULL) {
        return NULL;
    }

    s3_lock(track->mixer);
    audio = track->audio;
    s3_unlock(track->mixer);

    return audio;
}

S3_AudioStream* S3_MIX_GetTrackAudioStream(MIX_Track* track)
{
    S3_AudioStream* stream;

    if (track == NULL) {
        return NULL;
    }

    s3_lock(track->mixer);
    stream = track->appstream;
    s3_unlock(track->mixer);

    return stream;
}

bool S3_MIX_PlayTrack(MIX_Track* track, S3_PropertiesID options)
{
    Sint64 loops = 0;

    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    if (track->audio == NULL && track->appstream == NULL) {
        return s3_fail("Track has no input");
    }

    if (options != 0) {
        loops = S3_GetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
    }

    // Upstream reads the count as an int, so one above INT_MAX wraps negative and loops forever.
    if (loops < 0 || loops > (Sint64)INT_MAX) {
        loops = -1;
    }

    s3_lock(track->mixer);

    track->loops = (int)loops;
    track->fade_total = 0;
    track->fade_left = 0;
    track->paused = false;

    // A stream-fed track keeps its queued data, so playing it again restarts one that ran dry.
    if (track->appstream == NULL) {
        s3_track_rewind(track);
    }

    track->playing = true;

    s3_unlock(track->mixer);

    return true;
}

bool S3_MIX_StopTrack(MIX_Track* track, Sint64 fade_out_frames)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);

    if (!track->playing || fade_out_frames <= 0) {
        s3_track_stop(track);
    } else {
        // The API counts input frames; the fade counter advances in device frames.
        const int input_rate = s3_track_input_rate(track);
        const int device_rate = track->mixer != NULL ? track->mixer->spec.freq : 0;
        Sint64 device_frames = fade_out_frames;

        if (input_rate > 0 && device_rate > 0 && input_rate != device_rate) {
            device_frames = (fade_out_frames * device_rate) / input_rate;
            if (device_frames <= 0) {
                device_frames = 1;
            }
        }

        track->fade_total = device_frames;
        track->fade_left = device_frames;
    }

    s3_unlock(track->mixer);

    return true;
}

bool S3_MIX_PauseTrack(MIX_Track* track)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);
    if (track->playing) {
        track->paused = true;
    }
    s3_unlock(track->mixer);

    return true;
}

bool S3_MIX_ResumeTrack(MIX_Track* track)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);
    track->paused = false;
    s3_unlock(track->mixer);

    return true;
}

bool S3_MIX_TrackPlaying(MIX_Track* track)
{
    bool playing;

    if (track == NULL) {
        return false;
    }

    s3_lock(track->mixer);
    playing = track->playing && !track->paused;
    s3_unlock(track->mixer);

    return playing;
}

bool S3_MIX_TrackPaused(MIX_Track* track)
{
    bool paused;

    if (track == NULL) {
        return false;
    }

    s3_lock(track->mixer);
    paused = track->playing && track->paused;
    s3_unlock(track->mixer);

    return paused;
}

bool S3_MIX_SetTrackGain(MIX_Track* track, float gain)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    if (gain < 0.0f) {
        gain = 0.0f;
    }

    s3_lock(track->mixer);
    track->gain = gain;
    s3_unlock(track->mixer);

    return true;
}

float S3_MIX_GetTrackGain(MIX_Track* track)
{
    float gain;

    if (track == NULL) {
        return 1.0f;
    }

    s3_lock(track->mixer);
    gain = track->gain;
    s3_unlock(track->mixer);

    return gain;
}

bool S3_MIX_SetTrackStereo(MIX_Track* track, const MIX_StereoGains* gains)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);

    if (gains != NULL) {
        track->spatialized = true;
        track->gains.left = gains->left < 0.0f ? 0.0f : gains->left;
        track->gains.right = gains->right < 0.0f ? 0.0f : gains->right;
    } else {
        track->spatialized = false;
        track->gains.left = 1.0f;
        track->gains.right = 1.0f;
    }

    s3_unlock(track->mixer);

    return true;
}

Sint64 S3_MIX_GetTrackRemaining(MIX_Track* track)
{
    Sint64 remaining;

    if (track == NULL) {
        return -1;
    }

    s3_lock(track->mixer);

    if (!track->playing) {
        remaining = 0;
    } else if (track->appstream != NULL) {
        remaining = -1; // duration of an app-fed stream is unknowable
    } else if (track->audio == NULL) {
        remaining = -1;
    } else if (track->audio->kind == S3_AUDIO_PCM) {
        remaining = track->audio->pcm_frames - track->pcm_pos;
        if (remaining < 0) {
            remaining = 0;
        }
    } else if (track->mp3_ready && track->audio->mp3_frames >= 0) {
        remaining = track->audio->mp3_frames - (Sint64)track->mp3.currentPCMFrame;
        if (remaining < 0) {
            remaining = 0;
        }
    } else {
        remaining = -1;
    }

    s3_unlock(track->mixer);

    return remaining;
}

int S3_MIX_GetTrackLoops(MIX_Track* track)
{
    int loops;

    if (track == NULL) {
        return 0;
    }

    s3_lock(track->mixer);
    loops = track->playing ? track->loops : 0;
    s3_unlock(track->mixer);

    return loops;
}

bool S3_MIX_SetTrackLoops(MIX_Track* track, int num_loops)
{
    if (track == NULL) {
        return s3_fail("Track is NULL");
    }

    s3_lock(track->mixer);
    track->loops = num_loops < 0 ? -1 : num_loops;
    s3_unlock(track->mixer);

    return true;
}

Sint64 S3_MIX_MSToFrames(int sample_rate, Sint64 ms)
{
    if (sample_rate <= 0 || ms < 0) {
        return -1;
    }

    return (ms * sample_rate) / 1000;
}

Sint64 S3_MIX_FramesToMS(int sample_rate, Sint64 frames)
{
    if (sample_rate <= 0 || frames < 0) {
        return -1;
    }

    return (frames * 1000) / sample_rate;
}

// The track's input rate, as upstream defines it.
static int s3_track_input_rate(MIX_Track* track)
{
    if (track == NULL) {
        return 0;
    }

    if (track->audio != NULL) {
        return track->audio->kind == S3_AUDIO_PCM ? track->audio->pcm_freq : track->audio->mp3_freq;
    }

    if (track->appstream != NULL) {
        // The rate the app pushes, not the device rate the stream converts to.
        const int rate = S3_GetAudioStreamInputRate(track->appstream);

        if (rate > 0) {
            return rate;
        }

        // A stream with no source rate recorded is assumed to run at the device's.
        return track->mixer != NULL ? track->mixer->spec.freq : 0;
    }

    return 0;
}

Sint64 S3_MIX_TrackMSToFrames(MIX_Track* track, Sint64 ms)
{
    return S3_MIX_MSToFrames(s3_track_input_rate(track), ms);
}

Sint64 S3_MIX_TrackFramesToMS(MIX_Track* track, Sint64 frames)
{
    return S3_MIX_FramesToMS(s3_track_input_rate(track), frames);
}
