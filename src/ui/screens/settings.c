#include "screen_registry.h"

static const UiWidget k_volume_controls[] = {
    UI_SETTING_SLIDER("Master volume", SETTING_MASTER_VOLUME),
    UI_SETTING_SLIDER("Music volume", SETTING_MUSIC_VOLUME),
    UI_SETTING_SLIDER("Ambience volume", SETTING_AMBIENCE_VOLUME),
    UI_SETTING_SLIDER("Footsteps volume", SETTING_FOOTSTEPS_VOLUME),
};

static const UiWidget k_navigation_widgets[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_settings_containers[] = {
    UI_VERTICAL_CONTAINER("Volume controls", k_volume_controls),
    UI_VERTICAL_CONTAINER("Navigation", k_navigation_widgets),
};

static const UiWidget k_settings_root =
    UI_VERTICAL_CONTAINER("Settings", k_settings_containers);

static const UiScreenDefinition k_settings_screen = {
    UI_SCREEN_SETTINGS,
    "Settings",
    &k_settings_root,
};

const UiScreenDefinition *ui_settings_screen(void)
{
    return &k_settings_screen;
}
