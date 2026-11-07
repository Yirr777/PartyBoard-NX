// Party Board on Horizon: process setup and teardown in libnx's hooks around main().
// Structure follows new-coke/strikers' src/platform/switch/nx_init.c (CC0 1.0).

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <switch.h>

#include "nx_platform.h"

int port_sqlite_vfs_register(void);

static int s_nxlink = -1;
static FILE* s_logFile;

// libnx fills these from the loader's argument string; argv[0] is the .nro's own sdmc: path.
extern int __system_argc;
extern char** __system_argv;

static char s_homeDir[512] = MP4_SWITCH_HOME;

const char* mp4_switch_home(void)
{
    return s_homeDir;
}

static void resolve_home(void)
{
    const char* exe = (__system_argc > 0 && __system_argv != NULL) ? __system_argv[0] : NULL;
    if (exe == NULL || strncmp(exe, "sdmc:/", 6) != 0 || strrchr(exe, '/') == NULL)
        return;
    const size_t n = (size_t)(strrchr(exe, '/') - exe);
    if (n == 0 || n >= sizeof s_homeDir)
        return;
    memcpy(s_homeDir, exe, n);
    s_homeDir[n] = '\0';
}

static void open_log_file(void)
{
    char path[600];
    snprintf(path, sizeof path, "%s/partyboard-log.txt", s_homeDir);
    // Only when the user opted in by creating the file (or with nxlink), since SD writes are slow.
    struct stat st;
    if (stat(path, &st) != 0)
        return;
    s_logFile = freopen(path, "w", stdout);
    if (s_logFile != NULL)
    {
        dup2(fileno(stdout), fileno(stderr));
        setvbuf(stdout, NULL, _IOLBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
}

void userAppInit(void)
{
    if (R_SUCCEEDED(socketInitializeDefault()))
    {
        s_nxlink = nxlinkStdio();
        if (s_nxlink < 0)
            socketExit();
    }

    resolve_home();
    mkdir(s_homeDir, 0777);
    chdir(s_homeDir);
    if (s_nxlink < 0)
        open_log_file();
    else
    {
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    fprintf(stderr, "[nx] Party Board home %s\n", s_homeDir);

    if (R_SUCCEEDED(romfsInit()))
        setenv("PARTYBOARD_RESOURCES_DIR", "romfs:/", 0);

    // Launched from the album the process gets a few hundred MiB, less than the game needs.
    AppletType type = appletGetAppletType();
    if (type != AppletType_Application && type != AppletType_SystemApplication)
    {
        fprintf(stderr, "[nx] applet mode (type %d) has too little memory; exiting\n", (int)type);
        ErrorApplicationConfig error;
        if (R_SUCCEEDED(errorApplicationCreate(&error,
                "Party Board needs the full memory of title mode.",
                "Party Board cannot run from the album, which limits homebrew to a few hundred "
                "MiB.\n\nStart it from the HOME menu instead: hold R while launching any installed "
                "game, then pick Party Board in the homebrew menu.")))
            errorApplicationShow(&error);
        exit(1);
    }

    // Video capture (hold the Capture button). Works when the title's control.nacp allows it: the
    // forwarder's does; in title mode it is the host game's setting. It uses 96 MiB of the heap.
    if (R_SUCCEEDED(appletInitializeGamePlayRecording()))
        appletSetGamePlayRecordingState(true);
    else
        fprintf(stderr, "[nx] video capture not available for this title\n");

    // Exit is not locked: HOME > Close ends the process at once, since the game loop has no
    // shutdown path to run first.
    // Shader compilation dominates the first launch; run the CPU at full speed while it does.
    appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);

    // NVK only exposes GPUs it knows as conformant without this; the Tegra X1 is not one.
    setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 0);
    // The OpenGL ES fallback (devkitPro's Mesa nvc0) is what emulators run; without load
    // propagation its shaders avoid instruction forms they do not translate (IMAD with a
    // constant-buffer operand). NVK compiles with NAK and ignores this.
    setenv("NV50_PROG_OPTIMIZE", "1", 0);

    if (port_sqlite_vfs_register() != 0)
        fprintf(stderr, "[nx] SQLite file layer registration failed; caches will not persist\n");
}

int mp4_switch_system_language(void)
{
    static int cached = -1;
    if (cached >= 0)
        return cached;
    cached = 0;
    u64 code = 0;
    SetLanguage lang;
    if (R_SUCCEEDED(setInitialize())) {
        if (R_SUCCEEDED(setGetSystemLanguage(&code)) && R_SUCCEEDED(setMakeLanguage(code, &lang))) {
            switch (lang) {
                case SetLanguage_DE: cached = 1; break;
                case SetLanguage_FR:
                case SetLanguage_FRCA: cached = 2; break;
                case SetLanguage_ES:
                case SetLanguage_ES419: cached = 3; break;
                case SetLanguage_IT: cached = 4; break;
                default: cached = 0; break;
            }
        }
        setExit();
    }
    printf("[nx] system language -> %d\n", cached);
    return cached;
}

int mp4_switch_controller_count(void)
{
    int count = 0;
    for (int i = 0; i < 8; i++)
    {
        if (hidGetNpadStyleSet((HidNpadIdType)(HidNpadIdType_No1 + i)) != 0)
            count++;
    }
    if (hidGetNpadStyleSet(HidNpadIdType_Handheld) != 0)
        count++;
    return count;
}

int mp4_switch_show_controllers(void)
{
    // The system's controller screen, for up to the game's four players. It suspends the game the
    // way the HOME menu does, and the pads layer picks up whatever it changes.
    HidLaControllerSupportArg arg;
    hidLaCreateControllerSupportArg(&arg);
    arg.hdr.player_count_min = 1;
    arg.hdr.player_count_max = 4;
    arg.hdr.enable_take_over_connection = 1;
    arg.hdr.enable_left_justify = 1;
    arg.hdr.enable_permit_joy_dual = 1;
    const Result rc = hidLaShowControllerSupport(NULL, &arg);
    // Leaving the screen with B ends it without a result, which libnx reports as LibAppletBadExit.
    if (rc == MAKERESULT(Module_Libnx, LibnxError_LibAppletBadExit))
        fprintf(stderr, "[nx] controller screen closed without changes\n");
    else if (R_FAILED(rc))
        fprintf(stderr, "[nx] controller screen failed: 0x%x\n", (unsigned)rc);
    return R_SUCCEEDED(rc);
}

void mp4_switch_boot_done(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
}

// Tint's recursive resolver and Dawn's error paths need more than newlib's default thread stack.
#define MP4_SWITCH_THREAD_STACK (16u * 1024u * 1024u)

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg)
{
    pthread_attr_t sized;
    size_t size = 0;
    if (attr != NULL)
    {
        sized = *attr;
        pthread_attr_getstacksize(&sized, &size);
    }
    else
    {
        pthread_attr_init(&sized);
    }
    if (size < MP4_SWITCH_THREAD_STACK)
        pthread_attr_setstacksize(&sized, MP4_SWITCH_THREAD_STACK);
    return __real_pthread_create(thread, &sized, start, arg);
}

// libnx's abort exits silently; a fault makes Atmosphere write a crash report with the PC.
__attribute__((noreturn)) void abort(void)
{
    fflush(NULL);
    volatile int* fault = NULL;
    *fault = 0;
    for (;;)
    {
    }
}

void userAppExit(void)
{
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    fflush(NULL);
    romfsExit();
    if (s_nxlink >= 0)
    {
        close(s_nxlink);
        socketExit();
    }
}

// MusyX's mixer (hw_pc_nx.c). libnx's default core is the creator's, here the game thread's, so
// the mixer is pinned to core 2; core 3 belongs to the system. Slightly higher priority than the
// game, as a late cycle is an audible gap and a late frame is not.
static Thread s_audioThread;

int mp4_switch_audio_thread_start(void (*entry)(void*))
{
    if (R_FAILED(threadCreate(&s_audioThread, entry, NULL, NULL, 0x100000, 0x2B, 2)))
        return 0;
    if (R_FAILED(threadStart(&s_audioThread)))
    {
        threadClose(&s_audioThread);
        return 0;
    }
    return 1;
}

void mp4_switch_audio_thread_join(void)
{
    threadWaitForExit(&s_audioThread);
    threadClose(&s_audioThread);
}

void mp4_switch_sleep_ms(unsigned ms)
{
    svcSleepThread((s64)ms * 1000000);
}

// Frame statistics for hardware reports, written only when logging is on (see open_log_file).
extern char* fake_heap_end;
extern unsigned mp4_switch_audio_underruns;
extern size_t HuMemUsedMallocSizeGet(int heap);
extern size_t HuMemHeapSizeGet(int heap);

#define MP4_HEAP_DATA 2 // HEAP_DATA in include/game/memory.h
#define MP4_SWITCH_STATS_FRAMES 300u

static u64 s_frameStart, s_workSum, s_workMax, s_windowStart;
static unsigned s_statFrames;

void mp4_switch_frame_work_done(void)
{
    const u64 work = armGetSystemTick() - s_frameStart;
    s_workSum += work;
    if (work > s_workMax)
        s_workMax = work;
}

void mp4_switch_frame_done(void)
{
    const u64 now = armGetSystemTick();
    s_frameStart = now;
    if (s_nxlink < 0 && s_logFile == NULL)
        return;
    if (s_windowStart == 0)
    {
        s_windowStart = now;
        s_workSum = s_workMax = 0;
        s_statFrames = 0;
        return;
    }
    if (++s_statFrames < MP4_SWITCH_STATS_FRAMES)
        return;

    const double windowMs = (double)armTicksToNs(now - s_windowStart) / 1e6;
    const struct mallinfo heap = mallinfo();
    const unsigned long long heapFree =
        (unsigned long long)heap.fordblks + (unsigned long long)(fake_heap_end - (char*)sbrk(0));
    // The game's own HEAP_DATA holds its models; PartyBoard-X found a leak there on long boards.
    const size_t dataFree = HuMemHeapSizeGet(MP4_HEAP_DATA) - HuMemUsedMallocSizeGet(MP4_HEAP_DATA);
    fprintf(stderr,
            "[nx] perf %s: %.1f fps, work avg %.1f ms max %.1f ms, heap used %llu MiB free %llu MiB, "
            "HEAP_DATA free %zu KiB, audio underruns %u\n",
            appletGetOperationMode() == AppletOperationMode_Console ? "docked" : "handheld",
            s_statFrames * 1000.0 / windowMs,
            (double)armTicksToNs(s_workSum) / 1e6 / s_statFrames,
            (double)armTicksToNs(s_workMax) / 1e6,
            (unsigned long long)heap.uordblks >> 20, heapFree >> 20, dataFree >> 10,
            mp4_switch_audio_underruns);
    s_windowStart = now;
    s_workSum = s_workMax = 0;
    s_statFrames = 0;
}
