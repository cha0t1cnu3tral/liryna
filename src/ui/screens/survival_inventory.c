#include "screen_registry.h"

#include "inventory.h"
#include "tile_categories.h"
#include "../../world/tiles.h"

#include <stdio.h>

enum
{
    SURVIVAL_INVENTORY_COLUMNS = 4,
    SURVIVAL_CATEGORY_CAPACITY = INVENTORY_SURVIVAL_SLOT_COUNT
};

static const Inventory *k_bound_inventory = NULL;

static UiWidget k_category_tile_buttons[TILE_CATEGORY_COUNT][SURVIVAL_CATEGORY_CAPACITY];
static char k_category_tile_values[TILE_CATEGORY_COUNT][SURVIVAL_CATEGORY_CAPACITY][32];
static UiWidget k_empty_category_buttons[TILE_CATEGORY_COUNT];
static UiWidget k_category_containers[TILE_CATEGORY_COUNT + 1];
static UiWidget k_navigation_buttons[] = {
    UI_BUTTON("Back", UI_ACTION_BACK),
};
static UiWidget k_survival_root = {
    .type = UI_WIDGET_CONTAINER,
    .label = "Servival Mind Spelling Hear Inventory",
    .enabled = true,
    .focusable = false,
    .action = UI_ACTION_NONE,
    .direction = UI_CONTAINER_VERTICAL,
    .children = k_category_containers,
    .child_count = TILE_CATEGORY_COUNT + 1,
};
static UiScreenDefinition k_survival_screen = {
    UI_SCREEN_SURVIVAL_INVENTORY,
    "Servival Mind Spelling Hear Inventory",
    &k_survival_root,
};

void ui_survival_inventory_set_inventory(const struct Inventory *inventory)
{
    k_bound_inventory = inventory;
}

static void ui_build_survival_inventory_screen(void)
{
    int category_counts[TILE_CATEGORY_COUNT] = {0};

    for (int category_index = 0; category_index < TILE_CATEGORY_COUNT; category_index++)
    {
        k_empty_category_buttons[category_index] = (UiWidget){
            .type = UI_WIDGET_BUTTON,
            .label = "Empty",
            .enabled = true,
            .focusable = true,
            .action = UI_ACTION_NONE,
            .direction = UI_CONTAINER_VERTICAL,
        };
    }

    if (k_bound_inventory != NULL && k_bound_inventory->mode == GAME_MODE_SURVIVAL)
    {
        for (int slot_index = 0; slot_index < INVENTORY_SURVIVAL_SLOT_COUNT; slot_index++)
        {
            const InventorySlot *slot = &k_bound_inventory->slots[slot_index];
            if (!slot->occupied || slot->tile_id < 0 || slot->tile_id >= TILE_ID_COUNT)
            {
                continue;
            }

            const TileDefinition *tile = tiles_get_definition((TileId)slot->tile_id);
            if (tile == NULL)
            {
                continue;
            }

            const TileCategory category = tile_category_for_definition(tile);
            const int category_index = (int)category;
            int tile_index = category_counts[category_index];
            if (tile_index < 0 || tile_index >= SURVIVAL_CATEGORY_CAPACITY)
            {
                continue;
            }

            snprintf(k_category_tile_values[category_index][tile_index],
                     sizeof(k_category_tile_values[category_index][tile_index]),
                     "x%d", slot->count);
            k_category_tile_buttons[category_index][tile_index] = (UiWidget){
                .type = UI_WIDGET_BUTTON,
                .label = tile->name,
                .value = k_category_tile_values[category_index][tile_index],
                .enabled = true,
                .focusable = true,
                .action = UI_ACTION_SELECT_SURVIVAL_TILE,
                .direction = UI_CONTAINER_VERTICAL,
                .user_data = slot_index,
            };
            category_counts[category_index]++;
        }
    }

    for (int category_index = 0; category_index < TILE_CATEGORY_COUNT; category_index++)
    {
        const UiWidget *children = k_category_tile_buttons[category_index];
        int child_count = category_counts[category_index];
        if (child_count <= 0)
        {
            children = &k_empty_category_buttons[category_index];
            child_count = 1;
        }

        k_category_containers[category_index] = (UiWidget){
            .type = UI_WIDGET_CONTAINER,
            .label = tile_category_name((TileCategory)category_index),
            .enabled = true,
            .focusable = false,
            .action = UI_ACTION_NONE,
            .direction = UI_CONTAINER_GRID,
            .children = children,
            .child_count = child_count,
            .grid_columns = SURVIVAL_INVENTORY_COLUMNS,
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
}

const UiScreenDefinition *ui_survival_inventory_screen(void)
{
    ui_build_survival_inventory_screen();
    return &k_survival_screen;
}
