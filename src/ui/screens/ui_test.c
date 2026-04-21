#include "screen_registry.h"

static bool k_test_toggle_enabled = false;
static int k_test_volume = 65;
static int k_test_speed = 3;
static char k_test_name[32] = "Tester";
static int k_test_mode_index = 0;
static int k_test_theme_index = 1;

static const char *const k_test_mode_options[] = {
    "explore",
    "combat",
    "build",
};

static const char *const k_test_theme_options[] = {
    "classic",
    "compact",
    "verbose",
};

static const UiWidget k_basic_widgets[] = {
    UI_BUTTON("Example button", UI_ACTION_NONE),
    UI_TOGGLE("Example toggle", &k_test_toggle_enabled),
    UI_EDIT_BOX("Example edit box", k_test_name, sizeof(k_test_name)),
};

static const UiWidget k_value_widgets[] = {
    UI_SLIDER("Volume", &k_test_volume, 0, 100, 5),
    UI_SLIDER("Speed", &k_test_speed, 1, 10, 1),
    UI_PICKER("Mode", k_test_mode_options, &k_test_mode_index),
    UI_PICKER("Theme", k_test_theme_options, &k_test_theme_index),
};

static const UiWidget k_navigation_widgets[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_test_containers[] = {
    UI_VERTICAL_CONTAINER("Basic widgets", k_basic_widgets),
    UI_VERTICAL_CONTAINER("Value widgets", k_value_widgets),
    UI_VERTICAL_CONTAINER("Navigation", k_navigation_widgets),
};

static const UiWidget k_test_root =
    UI_VERTICAL_CONTAINER("UI test", k_test_containers);

static const UiScreenDefinition k_test_screen = {
    UI_SCREEN_TEST,
    "UI test",
    &k_test_root,
};

const UiScreenDefinition *ui_test_screen(void)
{
    return &k_test_screen;
}
