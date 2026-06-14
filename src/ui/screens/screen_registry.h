#ifndef LIRYNA_UI_SCREEN_REGISTRY_H
#define LIRYNA_UI_SCREEN_REGISTRY_H

#include "../ui_internal.h"

struct Inventory;
struct StructureBuilderConfig;
struct World;

const UiScreenDefinition *ui_get_screen_definition(UiScreen screen);

const UiScreenDefinition *ui_main_menu_screen(void);
const UiScreenDefinition *ui_new_world_screen(void);
const UiScreenDefinition *ui_creative_inventory_screen(void);
const UiScreenDefinition *ui_survival_inventory_screen(void);
const UiScreenDefinition *ui_structure_save_screen(void);
const UiScreenDefinition *ui_structure_browser_screen(void);
const UiScreenDefinition *ui_saved_worlds_screen(void);
const UiScreenDefinition *ui_settings_screen(void);
const UiScreenDefinition *ui_help_screen(void);
const UiScreenDefinition *ui_test_screen(void);

void ui_survival_inventory_set_inventory(const struct Inventory *inventory);
void ui_structure_save_bind_config(struct StructureBuilderConfig *config);
bool ui_structure_browser_select_entry(int entry_index, char *out_error, size_t out_error_size);
bool ui_structure_browser_save_selected(char *out_path, size_t out_path_size,
                                        char *out_error, size_t out_error_size);
bool ui_structure_browser_load_selected_into_world(struct World *world,
                                                   struct StructureBuilderConfig *config,
                                                   char *out_error,
                                                   size_t out_error_size);

#endif
