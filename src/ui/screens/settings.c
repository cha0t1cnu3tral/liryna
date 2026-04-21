#include "screen_registry.h"

static const UiWidget k_settings_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_settings_root =
    UI_VERTICAL_CONTAINER("Settings", k_settings_buttons);

static const UiScreenDefinition k_settings_screen = {
    UI_SCREEN_SETTINGS,
    "Settings",
    &k_settings_root,
};

const UiScreenDefinition *ui_settings_screen(void)
{
    return &k_settings_screen;
}
