#include "screen_registry.h"

#include <stddef.h>

const UiScreenDefinition *ui_get_screen_definition(UiScreen screen)
{
    switch (screen)
    {
    case UI_SCREEN_MENU:
        return ui_main_menu_screen();
    case UI_SCREEN_NEW_WORLD:
        return ui_new_world_screen();
    case UI_SCREEN_CREATIVE_INVENTORY:
        return ui_creative_inventory_screen();
    case UI_SCREEN_SAVED_WORLDS:
        return ui_saved_worlds_screen();
    case UI_SCREEN_SETTINGS:
        return ui_settings_screen();
    case UI_SCREEN_HELP:
        return ui_help_screen();
    case UI_SCREEN_TEST:
        return ui_test_screen();
    case UI_SCREEN_WORLD:
    default:
        return NULL;
    }
}
