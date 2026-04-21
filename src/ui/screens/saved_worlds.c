#include "screen_registry.h"

static const UiWidget k_saved_worlds_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};

static const UiWidget k_saved_worlds_root =
    UI_VERTICAL_CONTAINER("Saved worlds", k_saved_worlds_buttons);

static const UiScreenDefinition k_saved_worlds_screen = {
    UI_SCREEN_SAVED_WORLDS,
    "Saved worlds",
    &k_saved_worlds_root,
};

const UiScreenDefinition *ui_saved_worlds_screen(void)
{
    return &k_saved_worlds_screen;
}
