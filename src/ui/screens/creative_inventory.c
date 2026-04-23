#include "screen_registry.h"

#include "tile_categories.h"
#include "../../world/tiles.h"

enum
{
    CREATIVE_INVENTORY_COLUMNS = 4
};

static UiWidget k_category_tile_buttons[TILE_CATEGORY_COUNT][TILE_ID_COUNT];
static UiWidget k_category_containers[TILE_CATEGORY_COUNT + 1];
static UiWidget k_navigation_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};
static UiWidget k_creative_root = {
    .type = UI_WIDGET_CONTAINER,
    .label = "Inventory",
    .enabled = true,
    .focusable = false,
    .action = UI_ACTION_NONE,
    .direction = UI_CONTAINER_VERTICAL,
    .children = k_category_containers,
    .child_count = TILE_CATEGORY_COUNT + 1,
};
static UiScreenDefinition k_creative_screen = {
    UI_SCREEN_CREATIVE_INVENTORY,
    "Inventory",
    &k_creative_root,
};
static bool k_creative_screen_ready = false;

static void ui_build_creative_inventory_screen(void)
{
    if (k_creative_screen_ready)
    {
        return;
    }

    int category_counts[TILE_CATEGORY_COUNT] = {0};
    const int total_tiles = (int)tiles_count();
    for (int tile_id = 0; tile_id < total_tiles; tile_id++)
    {
        const TileDefinition *tile = tiles_get_definition((TileId)tile_id);
        if (tile == NULL)
        {
            continue;
        }

        const TileCategory category = tile_category_for_definition(tile);
        const int category_index = (int)category;
        int tile_index = category_counts[category_index];
        if (tile_index < 0 || tile_index >= TILE_ID_COUNT)
        {
            continue;
        }

        k_category_tile_buttons[category_index][tile_index] = (UiWidget){
            .type = UI_WIDGET_BUTTON,
            .label = tile->name,
            .enabled = true,
            .focusable = true,
            .action = UI_ACTION_SELECT_CREATIVE_TILE,
            .direction = UI_CONTAINER_VERTICAL,
        };
        category_counts[category_index]++;
    }

    for (int category_index = 0; category_index < TILE_CATEGORY_COUNT; category_index++)
    {
        k_category_containers[category_index] = (UiWidget){
            .type = UI_WIDGET_CONTAINER,
            .label = tile_category_name((TileCategory)category_index),
            .enabled = true,
            .focusable = false,
            .action = UI_ACTION_NONE,
            .direction = UI_CONTAINER_GRID,
            .children = k_category_tile_buttons[category_index],
            .child_count = category_counts[category_index],
            .grid_columns = CREATIVE_INVENTORY_COLUMNS,
        };
    }

    k_category_containers[TILE_CATEGORY_COUNT] = (UiWidget){
        .type = UI_WIDGET_CONTAINER,
        .label = "Navigation",
        .enabled = true,
        .focusable = false,
        .action = UI_ACTION_NONE,
        .direction = UI_CONTAINER_VERTICAL,
        .children = k_navigation_buttons,
        .child_count = UI_ARRAY_COUNT(k_navigation_buttons),
    };

    k_creative_screen_ready = true;
}

const UiScreenDefinition *ui_creative_inventory_screen(void)
{
    ui_build_creative_inventory_screen();
    return &k_creative_screen;
}
