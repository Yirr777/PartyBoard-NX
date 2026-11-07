# sdl3on2

A subset of the SDL3 API, and of SDL3_mixer, implemented on SDL2, for platforms where only SDL2
is available, such as the Nintendo Switch through devkitPro's `switch-sdl2`.

The include directory shadows `<SDL3/...>` and `<SDL3_mixer/...>`. Each mapped SDL3 name is a
macro for an `S3_` function that translates to SDL2. A name that is not mapped is a compile
error. Only the library's own sources see the real SDL2 headers.

## What is mapped

Init and quit, hints, app metadata, events, windows and renderers, surfaces and pixel formats,
gamepads, joysticks and haptics, keyboard and mouse, I/O streams, properties, filesystem paths,
audio streams, and a mixer with tracks and WAV and MP3 input. Within each area, only the functions
declared in `include/SDL3/s3_defs.h` and `include/SDL3_mixer/s3_mixer_defs.h` exist.

## Known differences from SDL3

- Only the default playback device opens. A device stream opened without a callback queues
  through SDL2's `SDL_QueueAudio`. SDL2 drains that queue a whole device buffer at a time and
  fills any shortfall with silence, so an application needs to keep more than one device buffer
  queued.
- `SDL_GetAudioStreamQueued` is exact while nothing has been read. SDL2 does not report how much
  input its resampler has consumed, so after reads the input behind the output is estimated from
  the rates and frame sizes.
- An audio stream's format cannot change once it is created; `SDL_SetAudioStreamFormat` is not
  mapped.
- Setting a string property to NULL clears its value; it does not delete the property.
- SDL3 event types with no SDL2 source, such as pen and camera events, are not declared, and
  `SDL_INIT_CAMERA` fails. `SDL_EVENT_FINGER_CANCELED` and the drop events are declared so code
  can name them, but none is ever delivered.
- `SDL_SetWindowRelativeMouseMode` sets SDL2's relative mode, which applies to every window.
- On Switch, gamepads come from libnx instead of SDL2's joystick driver. Each of up to eight
  controllers is a gamepad only while it is connected, and its player index is the console's player
  number. SDL3's sideways mapping applies to a single Joy-Con. A controller that changes style,
  such as a Joy-Con pair split in two, is removed and added again. Power levels are unknown, and
  virtual joysticks, LEDs and sensors are not supported. `SDL_SendGamepadEffect` takes a libnx
  `HidVibrationValue`, which plays on each of the controller's actuators until the next rumble call.
- Window properties carry the native handles from SDL2's `SDL_GetWindowWMInfo` for Win32, X11,
  Wayland, Android and UWP, refreshed by each `SDL_GetWindowProperties` call. Other video drivers,
  including Cocoa, publish none.
- On UWP, which SDL3 does not support, `SDL_PLATFORM_WINRT` is defined in place of
  `SDL_PLATFORM_WIN32`, and `SDL_PROP_WINDOW_WINRT_WINDOW_POINTER` holds the window's
  `CoreWindow` as an `IInspectable*`. `SDL_HINT_WINRT_HANDLE_BACK_BUTTON` defaults to 1, so SDL2
  marks a back request, such as B on an Xbox controller, as handled.
- In the mixer:
  - Mixing is Sint16 at the device format, not float.
  - `MIX_CreateMixerDevice` opens the default playback device whatever `devid` names. The device
    runs at S16 in native byte order: a requested rate and channel count are honoured, a
    requested format is not.
  - Only WAV and MP3 decode, recognised by content rather than file name. WAV is decoded to the
    device format at load whatever `predecode` says; MP3 honours it.
  - `MIX_LoadAudio(path, false)` on an MP3 streams from the file instead of holding the
    compressed bytes in memory.
  - `MIX_GetTrackRemaining` counts device frames for decoded input and MP3 frames for streaming
    MP3. An MP3's duration comes only from its Xing or VBRI header, so an untagged MP3 reports
    `MIX_DURATION_UNKNOWN`, and the count includes the encoder delay and padding upstream
    subtracts. A stream-fed track reports -1.
  - A track whose input runs dry always stops.
  - Groups, tags, 3D positioning, track callbacks, frequency ratios, output channel maps,
    metadata properties, `MIX_Generate`, `MIX_CreateMixer`, and every `MIX_PROP_PLAY_` property
    but `MIX_PROP_PLAY_LOOPS_NUMBER` are not declared.
  - `S3_MIX_SetMixerDevicePaused` pauses the device and is not in SDL3_mixer; code that also
    builds against the real library checks `SDL3ON2_MIXER` first.

These areas are exercised by the programs that use the library. There is no test suite.

## Using it

```cmake
include(FetchContent)
FetchContent_Declare(sdl3on2
    GIT_REPOSITORY https://github.com/new-coke/sdl3on2.git
    GIT_TAG <commit>)
FetchContent_MakeAvailable(sdl3on2)
target_link_libraries(game PRIVATE sdl3on2)
```

SDL2 must already be found as `SDL2::SDL2-static` or `SDL2::SDL2`, or in `SDL2_LIBRARIES`. On
Switch, `DEVKITPRO` must be set. The entry point comes from libnx, and SDL2main is not linked.

On UWP, `CMAKE_SYSTEM_NAME` must be `WindowsStore`, or `WINAPI_FAMILY=WINAPI_FAMILY_APP` defined
for every target. SDL2's WinRT sources are C++/CX and need MSVC; the layer and the application do
not. The entry point is the layer's `WinMain`, so the executable uses the Windows subsystem.

## Licence

zlib, as SDL itself. `src/stb_image.h` and `src/dr_mp3.h` are third-party and keep their own
licences.
