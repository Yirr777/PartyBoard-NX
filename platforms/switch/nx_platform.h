#ifndef MP4_SWITCH_NX_PLATFORM_H
#define MP4_SWITCH_NX_PLATFORM_H

#define MP4_SWITCH_HOME "sdmc:/switch/partyboard"

#ifdef __cplusplus
extern "C" {
#endif

// The directory holding partyboard.nro (normally sdmc:/switch/partyboard); also the working
// directory, so res/, the disc image, saves and caches sit beside the .nro.
const char* mp4_switch_home(void);

// Called once the game starts, to drop the CPU boost used for the first shader compilation.
void mp4_switch_boot_done(void);

// Controllers the console reports as connected, the attached Joy-Con included.
int mp4_switch_controller_count(void);

// Shows the system's controller screen; nonzero if it ran.
int mp4_switch_show_controllers(void);

// The console's language as a GameCube OS language (0 English, 1 German, 2 French, 3 Spanish,
// 4 Italian). Languages the game does not have give English.
int mp4_switch_system_language(void);

#ifdef __cplusplus
}
#endif

#endif
