#include "screen_registry.h"

#include "structure_builder.h"

enum
{
    STRUCTURE_SAVE_BIOME_COLUMNS = 2,
    STRUCTURE_SAVE_SUPPORT_COLUMNS = 3,
    STRUCTURE_SAVE_TYPE_OPTION_COUNT = 2
};

static StructureBuilderConfig *g_structure_builder_config = NULL;
static const char *const g_structure_type_options[STRUCTURE_SAVE_TYPE_OPTION_COUNT] = {
    "Structure",
    "Village",
};
static UiWidget g_structure_name_widgets[1];
static UiWidget g_structure_type_widgets[1];
static UiWidget g_structure_biome_widgets[BIOME_TYPE_COUNT];
static UiWidget g_structure_support_widgets[TILE_ID_COUNT];
static UiWidget g_structure_action_widgets[] = {
    UI_BUTTON("Save structure", UI_ACTION_SAVE_STRUCTURE),
    UI_BUTTON("Back", UI_ACTION_BACK),
};
static UiWidget g_structure_containers[5];
static UiWidget g_structure_root = {
    .type = UI_WIDGET_CONTAINER,
    .label = "Structure save",
    .enabled = true,
    .focusable = false,
    .action = UI_ACTION_NONE,
    .direction = UI_CONTAINER_VERTICAL,
    .children = g_structure_containers,
    .child_count = UI_ARRAY_COUNT(g_structure_containers),
};
static UiScreenDefinition g_structure_screen = {
    UI_SCREEN_STRUCTURE_SAVE,
    "Structure save",
    &g_structure_root,
};
static bool g_structure_screen_ready = false;

static void ui_build_structure_save_screen(void)
{
    if (g_structure_screen_ready)
    {
        return;
    }

    g_structure_name_widgets[0] = (UiWidget){
        .type = UI_WIDGET_EDIT_BOX,
        .label = "Build name",
        .enabled = true,
        .focusable = true,
        .direction = UI_CONTAINER_VERTICAL,
        .value_binding = UI_VALUE_STRING_BUFFER,
        .edit_value = g_structure_builder_config != NULL ? g_structure_builder_config->name : NULL,
        .edit_capacity =
            g_structure_builder_config != NULL ? sizeof(g_structure_builder_config->name) : 0,
    };
    g_structure_type_widgets[0] = (UiWidget){
        .type = UI_WIDGET_PICKER,
        .label = "Build type",
        .enabled = true,
        .focusable = true,
        .direction = UI_CONTAINER_VERTICAL,
        .value_binding = UI_VALUE_INT_POINTER,
        .picker_options = g_structure_type_options,
        .picker_option_count = STRUCTURE_SAVE_TYPE_OPTION_COUNT,
        .picker_index = g_structure_builder_config != NULL ? (int *)&g_structure_builder_config->save_kind
                                                           : NULL,
    };

    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        const BiomeDefinition *biome = biome_get_definition((BiomeType)biome_index);
        g_structure_biome_widgets[biome_index] = (UiWidget){
            .type = UI_WIDGET_TOGGLE,
            .label = biome != NULL ? biome->name : "Biome",
            .enabled = true,
            .focusable = true,
            .direction = UI_CONTAINER_VERTICAL,
            .value_binding = UI_VALUE_BOOL_POINTER,
            .toggle_value = g_structure_builder_config != NULL
                                ? &g_structure_builder_config->allowed_biomes[biome_index]
                                : NULL,
        };
    }

    int support_count = 0;
    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        if (!structure_builder_is_support_tile((TileId)tile_id))
        {
            continue;
        }

        const TileDefinition *tile = tiles_get_definition((TileId)tile_id);
        g_structure_support_widgets[support_count] = (UiWidget){
            .type = UI_WIDGET_TOGGLE,
            .label = tile != NULL ? tile->name : "Tile",
            .enabled = true,
            .focusable = true,
            .direction = UI_CONTAINER_VERTICAL,
            .value_binding = UI_VALUE_BOOL_POINTER,
            .toggle_value = g_structure_builder_config != NULL
                                ? &g_structure_builder_config->allowed_support_tiles[tile_id]
                                : NULL,
        };
        support_count++;
    }

    g_structure_containers[0] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Name",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_name_widgets,
        .child_count = UI_ARRAY_COUNT(g_structure_name_widgets),
    };
    g_structure_containers[1] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Type",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_type_widgets,
        .child_count = UI_ARRAY_COUNT(g_structure_type_widgets),
    };
    g_structure_containers[2] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Allowed biomes",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_GRID,
        .children = g_structure_biome_widgets,
        .child_count = BIOME_TYPE_COUNT,
        .grid_columns = STRUCTURE_SAVE_BIOME_COLUMNS,
    };
    g_structure_containers[3] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Allowed support tiles",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_GRID,
        .children = g_structure_support_widgets,
        .child_count = support_count,
        .grid_columns = STRUCTURE_SAVE_SUPPORT_COLUMNS,
    };
    g_structure_containers[4] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Actions",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = g_structure_action_widgets,
        .child_count = UI_ARRAY_COUNT(g_structure_action_widgets),
    };

    g_structure_screen_ready = true;
}

void ui_structure_save_bind_config(StructureBuilderConfig *config)
{
    g_structure_builder_config = config;
    g_structure_name_widgets[0] = (UiWidget){
        .type = UI_WIDGET_EDIT_BOX,
        .label = "Build name",
        .enabled = true,
        .focusable = true,
        .direction = UI_CONTAINER_VERTICAL,
        .value_binding = UI_VALUE_STRING_BUFFER,
        .edit_value = config != NULL ? config->name : NULL,
        .edit_capacity = config != NULL ? sizeof(config->name) : 0,
    };
    g_structure_type_widgets[0] = (UiWidget){
        .type = UI_WIDGET_PICKER,
        .label = "Build type",
        .enabled = true,
        .focusable = true,
        .direction = UI_CONTAINER_VERTICAL,
        .value_binding = UI_VALUE_INT_POINTER,
        .picker_options = g_structure_type_options,
        .picker_option_count = STRUCTURE_SAVE_TYPE_OPTION_COUNT,
        .picker_index = config != NULL ? (int *)&config->save_kind : NULL,
    };

    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        g_structure_biome_widgets[biome_index].toggle_value =
            config != NULL ? &config->allowed_biomes[biome_index] : NULL;
    }

    int support_count = 0;
    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        if (!structure_builder_is_support_tile((TileId)tile_id))
        {
            continue;
        }

        g_structure_support_widgets[support_count].toggle_value =
            config != NULL ? &config->allowed_support_tiles[tile_id] : NULL;
        support_count++;
    }
}

const UiScreenDefinition *ui_structure_save_screen(void)
{
    ui_build_structure_save_screen();
    return &g_structure_screen;
}
