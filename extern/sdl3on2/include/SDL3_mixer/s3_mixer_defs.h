// The SDL3_mixer subset, under the S3_MIX_ prefix so it can sit beside SDL2's headers.

#ifndef SDL3ON2_S3_MIXER_DEFS_H_
#define SDL3ON2_S3_MIXER_DEFS_H_

#include <SDL3/s3_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MIX_Mixer MIX_Mixer;
typedef struct MIX_Audio MIX_Audio;
typedef struct MIX_Track MIX_Track;

typedef struct MIX_StereoGains {
    float left;
    float right;
} MIX_StereoGains;

// The one MIX_PlayTrack() property read.
#define MIX_PROP_PLAY_LOOPS_NUMBER "SDL_mixer.play.loops"

#define MIX_DURATION_UNKNOWN -1
#define MIX_DURATION_INFINITE -2

extern int S3_MIX_Version(void);

extern bool S3_MIX_Init(void);

extern void S3_MIX_Quit(void);

// Opens the default playback device whatever `devid` names, at S16 whatever `spec` asks.
extern MIX_Mixer* S3_MIX_CreateMixerDevice(S3_AudioDeviceID devid, const S3_AudioSpec* spec);

extern void S3_MIX_DestroyMixer(MIX_Mixer* mixer);

// Not in SDL3_mixer; guard uses with SDL3ON2_MIXER.
extern void S3_MIX_SetMixerDevicePaused(MIX_Mixer* mixer, bool paused);

extern bool S3_MIX_GetMixerFormat(MIX_Mixer* mixer, S3_AudioSpec* spec);

extern void S3_MIX_LockMixer(MIX_Mixer* mixer);

extern void S3_MIX_UnlockMixer(MIX_Mixer* mixer);

// WAV and MP3 only, recognised by content.
extern MIX_Audio* S3_MIX_LoadAudio(MIX_Mixer* mixer, const char* path, bool predecode);

extern MIX_Audio* S3_MIX_LoadAudio_IO(MIX_Mixer* mixer, S3_IOStream* io, bool predecode, bool closeio);

extern void S3_MIX_DestroyAudio(MIX_Audio* audio);

extern Sint64 S3_MIX_GetAudioDuration(MIX_Audio* audio);

extern MIX_Track* S3_MIX_CreateTrack(MIX_Mixer* mixer);

extern void S3_MIX_DestroyTrack(MIX_Track* track);

extern MIX_Mixer* S3_MIX_GetTrackMixer(MIX_Track* track);

extern bool S3_MIX_SetTrackAudio(MIX_Track* track, MIX_Audio* audio);

extern bool S3_MIX_SetTrackAudioStream(MIX_Track* track, S3_AudioStream* stream);

extern MIX_Audio* S3_MIX_GetTrackAudio(MIX_Track* track);

extern S3_AudioStream* S3_MIX_GetTrackAudioStream(MIX_Track* track);

// Only MIX_PROP_PLAY_LOOPS_NUMBER is read.
extern bool S3_MIX_PlayTrack(MIX_Track* track, S3_PropertiesID options);

// `fade_out_frames` counts frames of the track's input format; 0 stops at once.
extern bool S3_MIX_StopTrack(MIX_Track* track, Sint64 fade_out_frames);

extern bool S3_MIX_PauseTrack(MIX_Track* track);

extern bool S3_MIX_ResumeTrack(MIX_Track* track);

extern bool S3_MIX_TrackPlaying(MIX_Track* track);

extern bool S3_MIX_TrackPaused(MIX_Track* track);

extern bool S3_MIX_SetMixerGain(MIX_Mixer* mixer, float gain);

extern float S3_MIX_GetMixerGain(MIX_Mixer* mixer);

extern bool S3_MIX_SetTrackGain(MIX_Track* track, float gain);

extern float S3_MIX_GetTrackGain(MIX_Track* track);

// NULL `gains` turns panning off.
extern bool S3_MIX_SetTrackStereo(MIX_Track* track, const MIX_StereoGains* gains);

extern Sint64 S3_MIX_GetTrackRemaining(MIX_Track* track);

// A loop count of -1 loops forever.
extern int S3_MIX_GetTrackLoops(MIX_Track* track);
extern bool S3_MIX_SetTrackLoops(MIX_Track* track, int num_loops);

// Frames of the track's input format.
extern Sint64 S3_MIX_TrackMSToFrames(MIX_Track* track, Sint64 ms);
extern Sint64 S3_MIX_TrackFramesToMS(MIX_Track* track, Sint64 frames);

extern Sint64 S3_MIX_MSToFrames(int sample_rate, Sint64 ms);

extern Sint64 S3_MIX_FramesToMS(int sample_rate, Sint64 frames);

#ifdef __cplusplus
}
#endif

#endif
