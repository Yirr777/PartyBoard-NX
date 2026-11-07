#include "port/settings.h"
#include "port/config.hpp"
#include "port/widescreen.h"
#include <dolphin/gx/GXAurora.h>
#ifdef __SWITCH__
#include "nx_platform.h"
#endif

namespace partyboard {

UserSettings g_userSettings = {
    .video = {
        .enableFullscreen {"video.enableFullscreen", false},
        .enableVsync {"video.enableVsync", true},
        // Stretching the 4:3 picture to a wide screen distorts it; on Switch the choice is 16:9
        // (port/widescreen.h) or the original 4:3 with side bars.
#ifdef __SWITCH__
        .lockAspectRatio {"video.lockAspectRatio", true},
        .widescreen {"video.widescreen", true},
#else
        .lockAspectRatio {"video.lockAspectRatio", false},
        .widescreen {"video.widescreen", false},
#endif
        .enableFpsOverlay {"game.enableFpsOverlay", false},
        .fpsOverlayCorner {"game.fpsOverlayCorner", 0},
    },

    .audio = {
        .masterVolume {"audio.masterVolume", 80},
        .mainMusicVolume {"audio.mainMusicVolume", 100},
        .subMusicVolume {"audio.subMusicVolume", 100},
        .soundEffectsVolume {"audio.soundEffectsVolume", 100},
        .fanfareVolume {"audio.fanfareVolume", 100},
        .enableReverb {"audio.enableReverb", true},
        .enableHrtf {"audio.enableHrtf", false},
        .menuSounds {"audio.menuSounds", true},
    },

    .game = {
        .language { "game.language", GameLanguage::English },

        // Quality of Life
        .enableQuickTransform {"game.enableQuickTransform", false},

        // Preferences
        .pauseOnFocusLost {"game.pauseOnFocusLost", false},
        .enableAchievementToasts {"game.enableAchievementToasts", true},
        .enableControllerToasts {"game.enableControllerToasts", true},
        .switchButtonPrompts {"game.switchButtonPrompts", true},
        .switchSystemLanguage {"game.switchSystemLanguage", true},

        // Graphics
        .internalResolutionScale {"game.internalResolutionScale", 0},
        .shadowResolutionMultiplier {"game.shadowResolutionMultiplier", 1},

        // Audio

        // Input
        .allowBackgroundInput {"game.allowBackgroundInput", true},

        // Cheats
        .infiniteHearts {"game.infiniteHearts", false},
        .unlockAllMinigames {"game.unlockAllMinigames", false},
        .unlockBowsersGnarlyParty {"game.unlockBowsersGnarlyParty", false},

        // Technical

        // Controls
        .enableTurboKeybind {"game.enableTurboKeybind", false},

        // Tools
        .speedrunMode {"game.speedrunMode", false},
        .recordingMode {"game.recordingMode", false}
    },

    .backend = {
        .isoPath {"backend.isoPath", ""},
        .isoVerification {"backend.isoVerification", DiscVerificationState::Unknown},
        .graphicsBackend {"backend.graphicsBackend", "auto"},
#ifdef __SWITCH__
        // Its own key on Switch: earlier builds saved the PC default (false) without showing it.
        .skipPreLaunchUI {"backend.switchSkipMainMenu", true},
#else
        .skipPreLaunchUI {"backend.skipPreLaunchUI", false},
#endif
        .skipBootSequence {"backend.skipBootSequence", false},
        .showPipelineCompilation {"backend.showPipelineCompilation", false},
        .wasPresetChosen {"backend.wasPresetChosen", false},
        .enableCrashReporting {"backend.enableCrashReporting", true},
        .checkForUpdates {"backend.checkForUpdates", true},
        .cardFileType {"backend.cardFileType", static_cast<int>(CARD_RAWIMAGE)},
        .enableAdvancedSettings {"backend.enableAdvancedSettings", false},
    }
};

UserSettings& getSettings() {
    return g_userSettings;
}

void applyAspect() {
    const bool wide = g_userSettings.video.widescreen.getValue();
    PartyBoard_SetWidescreen(wide);
    if (wide || !g_userSettings.video.lockAspectRatio.getValue()) {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
    } else {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);
    }
}

GameLanguage effectiveLanguage() {
#ifdef __SWITCH__
    if (g_userSettings.game.switchSystemLanguage.getValue()) {
        return static_cast<GameLanguage>(mp4_switch_system_language());
    }
#endif
    return g_userSettings.game.language.getValue();
}

void registerSettings() {
    // Video
    Register(g_userSettings.video.enableFullscreen);
    Register(g_userSettings.video.enableVsync);
    Register(g_userSettings.video.lockAspectRatio);
    Register(g_userSettings.video.widescreen);
    Register(g_userSettings.video.enableFpsOverlay);
    Register(g_userSettings.video.fpsOverlayCorner);

    // Audio
    Register(g_userSettings.audio.masterVolume);
    Register(g_userSettings.audio.mainMusicVolume);
    Register(g_userSettings.audio.subMusicVolume);
    Register(g_userSettings.audio.soundEffectsVolume);
    Register(g_userSettings.audio.fanfareVolume);
    Register(g_userSettings.audio.enableReverb);
    Register(g_userSettings.audio.enableHrtf);
    Register(g_userSettings.audio.menuSounds);

    // Game
    Register(g_userSettings.game.language);
    Register(g_userSettings.game.enableQuickTransform);
    Register(g_userSettings.game.pauseOnFocusLost);
    Register(g_userSettings.game.internalResolutionScale);
    Register(g_userSettings.game.shadowResolutionMultiplier);
    Register(g_userSettings.game.enableAchievementToasts);
    Register(g_userSettings.game.enableControllerToasts);
    Register(g_userSettings.game.switchButtonPrompts);
    Register(g_userSettings.game.switchSystemLanguage);
    Register(g_userSettings.game.enableTurboKeybind);
    Register(g_userSettings.game.speedrunMode);
    Register(g_userSettings.game.recordingMode);
    Register(g_userSettings.game.infiniteHearts);
    Register(g_userSettings.game.unlockAllMinigames);
    Register(g_userSettings.game.unlockBowsersGnarlyParty);
    Register(g_userSettings.game.allowBackgroundInput);

    Register(g_userSettings.backend.isoPath);
    Register(g_userSettings.backend.isoVerification);
    Register(g_userSettings.backend.graphicsBackend);
    Register(g_userSettings.backend.skipPreLaunchUI);
    Register(g_userSettings.backend.skipBootSequence);
    Register(g_userSettings.backend.showPipelineCompilation);
    Register(g_userSettings.backend.wasPresetChosen);
    Register(g_userSettings.backend.enableCrashReporting);
    Register(g_userSettings.backend.checkForUpdates);
    Register(g_userSettings.backend.cardFileType);
    Register(g_userSettings.backend.enableAdvancedSettings);
}

}

extern "C" {

CARDFileType partyboard_settings_card_file_type(void)
{
    return (CARDFileType)partyboard::getSettings().backend.cardFileType.getValue();
}

bool partyboard_settings_enableTurboKeybind(void)
{
    return partyboard::getSettings().game.enableTurboKeybind;
}

bool partyboard_settings_skipBootSequence(void)
{
    return partyboard::getSettings().backend.skipBootSequence;
}

bool partyboard_settings_unlock_all_minigames(void)
{
    return partyboard::getSettings().game.unlockAllMinigames.getValue();
}

s32 partyboard_settings_shadow_resolution_multiplier(void)
{
    return partyboard::getSettings().game.shadowResolutionMultiplier.getValue();
}

bool partyboard_settings_switch_button_prompts(void)
{
    return partyboard::getSettings().game.switchButtonPrompts.getValue();
}

bool partyboard_settings_unlock_bowsers_gnarly_party(void)
{
    return partyboard::getSettings().game.unlockBowsersGnarlyParty.getValue();
}
}
