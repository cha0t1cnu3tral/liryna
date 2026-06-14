#include "screen_registry.h"

#include <stdio.h>

#include "structure_browser.h"

enum
{
    STRUCTURE_BROWSER_BIOME_COLUMNS = 2,
    STRUCTURE_BROWSER_SUPPORT_COLUMNS = 3,
    STRUCTURE_BROWSER_TYPE_OPTION_COUNT = 2,
    STRUCTURE_BROWSER_CONTAINER_COUNT = 5
};

static const char *const g_structure_browser_type_options[STRUCTURE_BROWSER_TYPE_OPTION_COUNT] = {
    "Structure",
    "Village",
};

static StructureBrowserEntry g_structure_browser_entries[STRUCTURE_BROWSER_MAX_ENTRIES];
static char g_structure_browser_entry_labels[STRUCTURE_BROWSER_MAX_ENTRIES][192];
static int g_structure_browser_entry_count = 0;
static StructureBrowserSettings g_structure_browser_settings;
static bool g_structure_browser_has_selection = false;
static char g_structure_browser_status[160] = "Choose a saved structure.";

static UiWidget g_structure_browser_entry_widgets[STRUCTURE_BROWSER_MAX_ENTRIES];
static UiWidget g_structure_browser_empty_widgets[1];
static UiWidget g_structure_browser_type_widgets[1];
static UiWidget g_structure_browser_biome_widgets[BIOME_TYPE_COUNT];
static UiWidget g_structure_browser_support_widgets[TILE_ID_COUNT];
static UiWidget g_structure_browser_action_widgets[] = {
    UI_BUTTON("Edit selected structure", UI_ACTION_EDIT_STRUCTURE_ENTRY),
    UI_BUTTON("Save generation settings", UI_ACTION_SAVE_STRUCTURE_SETTINGS),
    UI_BUTTON("Back", UI_ACTION_BACK),
};
static UiWidget g_structure_browser_containers[STRUCTURE_BROWSER_CONTAINER_COUNT];
static UiWidget g_structure_browser_root = {
    .type = UI_WIDGET_CONTAINER,
    .label = "Structure browser",
    .enabled = true,
    .focusable = false,
    .action = UI_ACTION_NONE,
    .direction = UI_CONTAINER_VERTICAL,
    .children = g_structure_browser_containers,
    .child_count = UI_ARRAY_COUNT(g_structure_browser_containers),
};
static UiScreenDefinition g_structure_browser_screen = {
    UI_SCREEN_STRUCTURE_BROWSER,
    "Structure browser",
    &g_structure_browser_root,
};

static int ui_structure_browser_support_widget_count(void)
{
    int support_count = 0;
    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        if (structure_builder_is_support_tile((TileId)tile_id))
        {
            support_count++;
        }
    }
    return support_count;
}

static void ui_structure_browser_bind_settings(void)
{
    g_structure_browser_type_widgets[0] = (UiWidget){
        .type = UI_WIDGET_PICKER,
        .label = "Generation type",
        .enabled = g_structure_browser_has_selection,
        .focusable = true,
        .direction = UI_CONTAINER_VERTICAL,
        .value_binding = UI_VALUE_INT_POINTER,
        .picker_options = g_structure_browser_type_options,
        .picker_option_count = STRUCTURE_BROWSER_TYPE_OPTION_COUNT,
        .picker_index = g_structure_browser_has_selection
                            ? (int *)&g_structure_browser_settings.save_kind
                            : NULL,
    };

    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        const BiomeDefinition *biome = biome_get_definition((BiomeType)biome_index);
        g_structure_browser_biome_widgets[biome_index] = (UiWidget){
            .type = UI_WIDGET_TOGGLE,
            .label = biome != NULL ? biome->name : "Biome",
            .enabled = g_structure_browser_has_selection,
            .focusable = true,
            .direction = UI_CONTAINER_VERTICAL,
            .value_binding = UI_VALUE_BOOL_POINTER,
            .toggle_value = g_structure_browser_has_selection
                                ? &g_structure_browser_settings.allowed_biomes[biome_index]
                                : NULL,
        };
    }

    int support_index = 0;
    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        if (!structure_builder_is_support_tile((TileId)tile_id))
        {
            continue;
        }

        const TileDefinition *tile = tiles_get_definition((TileId)tile_id);
        g_structure_browser_support_widgets[support_index] = (UiWidget){
            .type = UI_WIDGET_TOGGLE,
            .label = tile != NULL ? tile->name : "Tile",
            .enabled = g_structure_browser_has_selection,
            .focusable = true,
            .direction = UI_CONTAINER_VERTICAL,
            .value_binding = UI_VALUE_BOOL_POINTER,
            .toggle_value = g_structure_browser_has_selection
                                ? &g_structure_browser_settings.allowed_support_tiles[tile_id]
                                : NULL,
        };
        support_index++;
    }

    g_structure_browser_action_widgets[0].enabled = g_structure_browser_has_selection;
    g_structure_browser_action_widgets[1].enabled = g_structure_browser_has_selection;
}

static void ui_structure_browser_refresh_entries(void)
{
    char error[160];
    g_structure_browser_entry_count = structure_browser_list(
        g_structure_browser_entries, STRUCTURE_BROWSER_MAX_ENTRIES, error, sizeof(error));

    for (int i = 0; i < g_structure_browser_entry_count; i++)
    {
        snprintf(g_structure_browser_entry_labels[i], sizeof(g_structure_browser_entry_labels[i]),
                 "%s: %s %dx%d",
                 structure_builder_save_kind_name(g_structure_browser_entries[i].save_kind),
                 g_structure_browser_entries[i].name,
                 g_structure_browser_entries[i].width,
                 g_structure_browser_entries[i].height);
        g_structure_browser_entry_widgets[i] = (UiWidget){
            .type = UI_WIDGET_BUTTON,
            .label = g_structure_browser_entry_labels[i],
            .enabled = true,
            .focusable = true,
            .action = UI_ACTION_SELECT_STRUCTURE_ENTRY,
            .direction = UI_CONTAINER_VERTICAL,
            .user_data = i,
        };
    }

    if (g_structure_browser_entry_count == 0)
    {
        g_structure_browser_empty_widgets[0] = (UiWidget){
            .type = UI_WIDGET_BUTTON,
            .label = error[0] != '\0' ? error : "No saved structures found",
            .enabled = false,
            .focusable = true,
            .action = UI_ACTION_NONE,
            .direction = UI_CONTAINER_VERTICAL,
        };
    }
}

static void ui_structure_browser_rebuild_screen(void)
{
    ui_structure_browser_refresh_entries();
    ui_structure_browser_bind_settings();

    g_structure_browser_containers[0] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Saved structures and villages",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_browser_entry_count > 0 ? g_structure_browser_entry_widgets
                                                        : g_structure_browser_empty_widgets,
        .child_count = g_structure_browser_entry_count > 0 ? g_structure_browser_entry_count : 1,
    };
    g_structure_browser_containers[1] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = g_structure_browser_has_selection ? g_structure_browser_settings.name
                                                   : g_structure_browser_status,
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_browser_type_widgets,
        .child_count = UI_ARRAY_COUNT(g_structure_browser_type_widgets),
    };
    g_structure_browser_containers[2] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Allowed biomes",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_GRID,
        .children = g_structure_browser_biome_widgets,
        .child_count = BIOME_TYPE_COUNT,
        .grid_columns = STRUCTURE_BROWSER_BIOME_COLUMNS,
    };
    g_structure_browser_containers[3] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Allowed support tiles",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_GRID,
        .children = g_structure_browser_support_widgets,
        .child_count = ui_structure_browser_support_widget_count(),
        .grid_columns = STRUCTURE_BROWSER_SUPPORT_COLUMNS,
    };
    g_structure_browser_containers[4] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Actions",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_browser_action_widgets,
        .child_count = UI_ARRAY_COUNT(g_structure_browser_action_widgets),
    };
}

bool ui_structure_browser_select_entry(int entry_index, char *out_error, size_t out_error_size)
{
    ui_structure_browser_refresh_entries();
    if (entry_index < 0 || entry_index >= g_structure_browser_entry_count)
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Structure selection is no longer available.");
        }
        return false;
    }

    if (!structure_browser_load_settings(g_structure_browser_entries[entry_index].path,
                                         &g_structure_browser_settings,
                                         out_error,
                                         out_error_size))
    {
        g_structure_browser_has_selection = false;
        ui_structure_browser_rebuild_screen();
        return false;
    }

    g_structure_browser_has_selection = true;
    snprintf(g_structure_browser_status, sizeof(g_structure_browser_status), "%s",
             g_structure_browser_settings.name);
    ui_structure_browser_rebuild_screen();
    return true;
}

bool ui_structure_browser_save_selected(char *out_path,
                                        size_t out_path_size,
                                        char *out_error,
                                        size_t out_error_size)
{
    if (!g_structure_browser_has_selection)
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Open a structure before saving settings.");
        }
        return false;
    }

    const bool saved = structure_browser_save_settings(&g_structure_browser_settings,
                                                       out_path,
                                                       out_path_size,
                                                       out_error,
                                                       out_error_size);
    ui_structure_browser_rebuild_screen();
    return saved;
}

bool ui_structure_browser_load_selected_into_world(World *world,
                                                   StructureBuilderConfig *config,
                                                   char *out_error,
                                                   size_t out_error_size)
{
    if (!g_structure_browser_has_selection)
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Open a structure before editing it.");
        }
        return false;
    }

    return structure_browser_load_into_world(&g_structure_browser_settings,
                                             world,
                                             config,
                                             out_error,
                                             out_error_size);
}

const UiScreenDefinition *ui_structure_browser_screen(void)
{
    ui_structure_browser_rebuild_screen();
    return &g_structure_browser_screen;
}
