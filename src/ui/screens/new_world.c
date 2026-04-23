#include "screen_registry.h"

static const UiWidget k_new_world_mode_buttons[] = {
    UI_BUTTON("Survival", UI_ACTION_START_WORLD_SURVIVAL),
    UI_BUTTON("Creative", UI_ACTION_START_WORLD_CREATIVE),
};

static const UiWidget k_new_world_navigation_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_new_world_containers[] = {
    UI_VERTICAL_CONTAINER("Mode", k_new_world_mode_buttons),
    UI_VERTICAL_CONTAINER("Navigation", k_new_world_navigation_buttons),
};

static const UiWidget k_new_world_root =
    UI_VERTICAL_CONTAINER("New world", k_new_world_containers);

static const UiScreenDefinition k_new_world_screen = {
    UI_SCREEN_NEW_WORLD,
    "New world",
    &k_new_world_root,
};

const UiScreenDefinition *ui_new_world_screen(void)
{
    return &k_new_world_screen;
}
