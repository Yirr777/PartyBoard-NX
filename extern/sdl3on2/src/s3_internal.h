// In the implementation SDL_ names are SDL2's and S3_ names are the layer's.

#ifndef SDL3ON2_S3_INTERNAL_H_
#define SDL3ON2_S3_INTERNAL_H_

#define S3_IMPLEMENTATION 1

// A package whose include directory is SDL2's own, not its parent, has only <SDL.h>.
#if defined(__has_include)
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#else
#include <SDL2/SDL.h>
#endif

#include <SDL3/s3_defs.h>

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface* S3_UnwrapSurface(S3_Surface* surface);
S3_Surface* S3_WrapSurface(SDL_Surface* sdl2_surface);
void S3_RefreshSurface(S3_Surface* surface);

SDL_BlendMode S3_ToSDL2BlendMode(S3_BlendMode mode);
S3_BlendMode S3_FromSDL2BlendMode(SDL_BlendMode mode);

SDL_RWops* S3_UnwrapIO(S3_IOStream* stream);

S3_PropertiesID S3_AcquireObjectProperties(const void* object, bool* created);
void S3_ReleaseObjectProperties(const void* object);
void S3_QuitProperties(void);

int S3_GetAudioStreamInputRate(S3_AudioStream* stream);

S3_JoystickID S3_FromSDL2JoystickID(SDL_JoystickID id);
int S3_JoystickDeviceIndex(S3_JoystickID instance_id);

bool S3_TranslateEvent(const SDL_Event* native, S3_Event* event, char* text, size_t text_size);
bool S3_PushWindowEvent(Uint8 sdl2_window_event, Uint32 window_id, Sint32 data1, Sint32 data2);

#ifndef __SWITCH__
void S3_PublishNativeWindowProperties(SDL_Window* window, S3_PropertiesID props);
#endif

#ifdef __SWITCH__
bool S3_SwitchInitVideo(void);
void S3_SwitchQuitVideo(bool all);
bool S3_SwitchVideoInitialized(void);
void S3_SwitchPumpApplet(void);
bool S3_SwitchInitPads(void);
void S3_SwitchQuitPads(bool all);
bool S3_SwitchPadsInitialized(void);
void S3_SwitchPumpPads(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
