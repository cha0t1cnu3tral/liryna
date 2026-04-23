#ifndef LIRYNA_UI_SCREEN_REGISTRY_H
#define LIRYNA_UI_SCREEN_REGISTRY_H

#include "../ui_internal.h"

struct Inventory;

const UiScreenDefinition *ui_get_screen_definition(UiScreen screen);

const UiScreenDefinition *ui_main_menu_screen(void);
const UiScreenDefinition *ui_new_world_screen(void);
const UiScreenDefinition *ui_creative_inventory_screen(void);
const UiScreenDefinition *ui_survival_inventory_screen(void);
const UiScreenDefinition *ui_saved_worlds_screen(void);
const UiScreenDefinition *ui_settings_screen(void);
const UiScreenDefinition *ui_help_screen(void);
const UiScreenDefinition *ui_test_screen(void);

void ui_survival_inventory_set_inventory(const struct Inventory *inventory);

#endif
