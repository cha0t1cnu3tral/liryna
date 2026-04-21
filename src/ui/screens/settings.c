#include "screen_registry.h"

static bool k_speech_hints_enabled = true;
static int k_speech_rate = 50;
static char k_player_name[32] = "Player";
static int k_difficulty_index = 1;

static const char *const k_difficulty_options[] = {
    "easy",
    "normal",
    "hard",
};

static const UiWidget k_settings_buttons[] = {
    UI_TOGGLE("Speech hints", &k_speech_hints_enabled),
    UI_SLIDER("Speech rate", &k_speech_rate, 0, 100, 5),
    UI_EDIT_BOX("Player name", k_player_name, sizeof(k_player_name)),
    UI_PICKER("Difficulty", k_difficulty_options, &k_difficulty_index),
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
