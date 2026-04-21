#include "screen_registry.h"

static const UiWidget k_help_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_help_root =
    UI_VERTICAL_CONTAINER("Help", k_help_buttons);

static const UiScreenDefinition k_help_screen = {
    UI_SCREEN_HELP,
    "Help",
    &k_help_root,
};

const UiScreenDefinition *ui_help_screen(void)
{
    return &k_help_screen;
}
