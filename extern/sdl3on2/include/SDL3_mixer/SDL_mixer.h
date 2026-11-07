// An SDL3_mixer name not mapped here is a compile error.

#ifndef SDL3ON2_SDL_MIXER_H_
#define SDL3ON2_SDL_MIXER_H_

#include <SDL3/SDL.h>
#include <SDL3_mixer/s3_mixer_defs.h>

// The SDL3_mixer release whose API the mapped subset follows.
#define SDL_MIXER_MAJOR_VERSION 3
#define SDL_MIXER_MINOR_VERSION 2
#define SDL_MIXER_MICRO_VERSION 2

#define SDL_MIXER_VERSION                  \
    ((SDL_MIXER_MAJOR_VERSION) * 1000000   \
        + (SDL_MIXER_MINOR_VERSION) * 1000 \
        + (SDL_MIXER_MICRO_VERSION))

#define SDL_MIXER_VERSION_ATLEAST(X, Y, Z)                               \
    ((SDL_MIXER_MAJOR_VERSION >= X)                                      \
        && (SDL_MIXER_MAJOR_VERSION > X || SDL_MIXER_MINOR_VERSION >= Y) \
        && (SDL_MIXER_MAJOR_VERSION > X || SDL_MIXER_MINOR_VERSION > Y || SDL_MIXER_MICRO_VERSION >= Z))

// Code that also builds against the real library tests this before S3_MIX_SetMixerDevicePaused.
#define SDL3ON2_MIXER 1

#define MIX_Version S3_MIX_Version
#define MIX_Init S3_MIX_Init
#define MIX_Quit S3_MIX_Quit
#define MIX_CreateMixerDevice S3_MIX_CreateMixerDevice
#define MIX_DestroyMixer S3_MIX_DestroyMixer
#define MIX_GetMixerFormat S3_MIX_GetMixerFormat
#define MIX_LockMixer S3_MIX_LockMixer
#define MIX_UnlockMixer S3_MIX_UnlockMixer
#define MIX_LoadAudio S3_MIX_LoadAudio
#define MIX_LoadAudio_IO S3_MIX_LoadAudio_IO
#define MIX_DestroyAudio S3_MIX_DestroyAudio
#define MIX_GetAudioDuration S3_MIX_GetAudioDuration
#define MIX_CreateTrack S3_MIX_CreateTrack
#define MIX_DestroyTrack S3_MIX_DestroyTrack
#define MIX_GetTrackMixer S3_MIX_GetTrackMixer
#define MIX_SetTrackAudio S3_MIX_SetTrackAudio
#define MIX_SetTrackAudioStream S3_MIX_SetTrackAudioStream
#define MIX_GetTrackAudio S3_MIX_GetTrackAudio
#define MIX_GetTrackAudioStream S3_MIX_GetTrackAudioStream
#define MIX_PlayTrack S3_MIX_PlayTrack
#define MIX_StopTrack S3_MIX_StopTrack
#define MIX_PauseTrack S3_MIX_PauseTrack
#define MIX_ResumeTrack S3_MIX_ResumeTrack
#define MIX_TrackPlaying S3_MIX_TrackPlaying
#define MIX_TrackPaused S3_MIX_TrackPaused
#define MIX_SetTrackGain S3_MIX_SetTrackGain
#define MIX_GetTrackGain S3_MIX_GetTrackGain
#define MIX_SetMixerGain S3_MIX_SetMixerGain
#define MIX_GetMixerGain S3_MIX_GetMixerGain
#define MIX_SetTrackStereo S3_MIX_SetTrackStereo
#define MIX_GetTrackRemaining S3_MIX_GetTrackRemaining
#define MIX_GetTrackLoops S3_MIX_GetTrackLoops
#define MIX_SetTrackLoops S3_MIX_SetTrackLoops
#define MIX_TrackMSToFrames S3_MIX_TrackMSToFrames
#define MIX_TrackFramesToMS S3_MIX_TrackFramesToMS
#define MIX_MSToFrames S3_MIX_MSToFrames
#define MIX_FramesToMS S3_MIX_FramesToMS

#endif
