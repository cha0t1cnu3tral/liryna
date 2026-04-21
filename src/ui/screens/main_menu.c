#include "screen_registry.h"

static const UiWidget k_main_menu_buttons[] = {
    UI_BUTTON("New world", UI_ACTION_NEW_WORLD),
    UI_BUTTON("Saved worlds", UI_ACTION_OPEN_SAVED_WORLDS),
    UI_BUTTON("Settings", UI_ACTION_OPEN_SETTINGS),
    UI_BUTTON("Help", UI_ACTION_OPEN_HELP),
    UI_BUTTON("Exit", UI_ACTION_EXIT),
};

static const UiWidget k_main_menu_root =
    UI_VERTICAL_CONTAINER("Main menu", k_main_menu_buttons);

static const UiScreenDefinition k_main_menu_screen = {
    UI_SCREEN_MENU,
    "Main menu",
    &k_main_menu_root,
};

const UiScreenDefinition *ui_main_menu_screen(void)
{
    return &k_main_menu_screen;
}
