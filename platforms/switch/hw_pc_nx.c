// MusyX's audio device on Horizon: the mixer on its own thread, feeding a queued sdl3on2 stream.
// Follows MusyX's hw_pc_sdl.c, which needs SDL3 calls sdl3on2 does not have. The differences:
// - The mixer runs on a core of its own, since a thread started with libnx's default core shares
//   the game thread's (see mp4_switch_audio_thread_start in nx_init.c).
// - It mixes at the GameCube's 32 kHz rather than the device's 48 kHz, which costs less CPU; SDL2
//   converts the rate.
// - Pausing stops the mixer and lets the device play silence, since sdl3on2 has no device pause.

#include <stdatomic.h>

#include <SDL3/SDL.h>

#include "hw_pc_internal.h"
#include "musyx/hardware.h"
#include "musyx/pc.h"

#define NX_MIX_RATE 32000
#define NX_MIX_CHANNELS 2
// Cycles of 5 ms kept queued: enough to ride out a busy moment, short enough not to lag behind.
#define NX_QUEUED_CYCLES 8

int mp4_switch_audio_thread_start(void (*entry)(void*));
void mp4_switch_audio_thread_join(void);
void mp4_switch_sleep_ms(unsigned ms);

// Times the device ran out of samples while playing; reported in the [nx] perf line.
atomic_uint mp4_switch_audio_underruns;

static SDL_Mutex* s_mutex;
static SDL_AudioStream* s_stream;
static bool s_threadActive;
static atomic_bool s_running;
static atomic_bool s_paused;

static void mix_thread(void* unused)
{
    (void)unused;
    static s16 buffer[SAL_PC_MAX_FRAMES * SAL_PC_MAX_CHANNELS];
    const SND_PC_RENDER_INFO info = sndPCGetRenderInfo();
    const size_t frames = (info.mixRate + 199) / 200;
    const int bytes = (int)(frames * info.channels * sizeof(s16));
    bool started = false;

    while (atomic_load(&s_running)) {
        const int queued = SDL_GetAudioStreamQueued(s_stream);
        if (queued < 0) {
            break;
        }
        if (atomic_load(&s_paused) || queued >= bytes * NX_QUEUED_CYCLES) {
            mp4_switch_sleep_ms(1);
            continue;
        }
        if (queued == 0 && started) {
            atomic_fetch_add(&mp4_switch_audio_underruns, 1);
        }

        bool ok = true;
        hwIRQEnterCritical();
        if (!atomic_load(&s_paused) && atomic_load(&s_running)) {
            ok = sndPCRender(buffer, frames) && SDL_PutAudioStreamData(s_stream, buffer, bytes);
            started = true;
        }
        hwIRQLeaveCritical();
        if (!ok) {
            break;
        }
    }
    atomic_store(&s_running, false);
}

static bool open_stream(void)
{
    const SDL_AudioSpec spec = {SDL_AUDIO_S16, NX_MIX_CHANNELS, NX_MIX_RATE};
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return false;
    }
    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (s_stream == NULL) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    return true;
}

bool sndPCOpenAudio(const SND_PC_CONFIG* preferred, SND_PC_CONFIG* obtained)
{
    (void)preferred;
    SND_PC_CONFIG selected = {NX_MIX_RATE, NX_MIX_CHANNELS};
    if (sndIsInstalled() || s_stream != NULL || s_threadActive) {
        return false;
    }
    if (!open_stream()) {
        return false;
    }
    if (!sndPCConfigure(&selected)) {
        sndPCStopAudio();
        return false;
    }
    if (obtained != NULL) {
        *obtained = selected;
    }
    return true;
}

bool sndPCStartAudio(void)
{
    if (!sndIsInstalled() || s_threadActive) {
        return false;
    }
    if (s_stream == NULL && !open_stream()) {
        return false;
    }
    atomic_store(&s_paused, false);
    atomic_store(&s_running, true);
    s_threadActive = mp4_switch_audio_thread_start(mix_thread) != 0;
    if (!s_threadActive || !SDL_ResumeAudioStreamDevice(s_stream)) {
        sndPCStopAudio();
        return false;
    }
    return true;
}

void sndPCStopAudio(void)
{
    atomic_store(&s_running, false);
    if (s_threadActive) {
        mp4_switch_audio_thread_join();
        s_threadActive = false;
    }
    if (s_stream != NULL) {
        SDL_DestroyAudioStream(s_stream);
        s_stream = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void sndPCPauseAudio(bool value)
{
    atomic_store(&s_paused, value);
    // Barrier with a cycle already being rendered when the pause was requested.
    hwIRQEnterCritical();
    hwIRQLeaveCritical();
}

void hwInitIrq(void)
{
    s_mutex = SDL_CreateMutex();
    hwDisableIrq();
}

void hwExitIrq(void)
{
    SDL_DestroyMutex(s_mutex);
    s_mutex = NULL;
}

void hwEnableIrq(void)
{
    if (!salPCExternalLeave() && s_mutex != NULL) {
        SDL_UnlockMutex(s_mutex);
    }
}

void hwDisableIrq(void)
{
    if (!salPCExternalEnter() && s_mutex != NULL) {
        SDL_LockMutex(s_mutex);
    }
}

void hwIRQEnterCritical(void) { hwDisableIrq(); }
void hwIRQLeaveCritical(void) { hwEnableIrq(); }
