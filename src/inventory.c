#include "inventory.h"

#include "world/tiles.h"

void inventory_init(Inventory *inventory, GameMode mode)
{
    if (inventory == NULL)
    {
        return;
    }

    inventory->mode = mode;
    inventory->selected_tile = TILE_GRASS;

    for (int i = 0; i < INVENTORY_SURVIVAL_SLOT_COUNT; i++)
    {
        inventory->slots[i].occupied = false;
        inventory->slots[i].tile_id = TILE_ID_COUNT;
        inventory->slots[i].count = 0;
    }
}

bool inventory_add_survival(Inventory *inventory, int tile_id, int count)
{
    if (inventory == NULL || inventory->mode != GAME_MODE_SURVIVAL ||
        tile_id < 0 || tile_id >= TILE_ID_COUNT || count <= 0)
    {
        return false;
    }

    for (int i = 0; i < INVENTORY_SURVIVAL_SLOT_COUNT; i++)
    {
        if (inventory->slots[i].occupied && inventory->slots[i].tile_id == tile_id)
        {
            inventory->slots[i].count += count;
            return true;
        }
    }

    for (int i = 0; i < INVENTORY_SURVIVAL_SLOT_COUNT; i++)
    {
        if (!inventory->slots[i].occupied)
        {
            inventory->slots[i].occupied = true;
            inventory->slots[i].tile_id = tile_id;
            inventory->slots[i].count = count;
            return true;
        }
    }

    return false;
}

int inventory_tile_count(const Inventory *inventory, int tile_id)
{
    if (inventory == NULL || tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return 0;
    }

    if (inventory->mode == GAME_MODE_CREATIVE)
    {
        return -1;
    }

    for (int i = 0; i < INVENTORY_SURVIVAL_SLOT_COUNT; i++)
    {
        if (inventory->slots[i].occupied && inventory->slots[i].tile_id == tile_id)
        {
            return inventory->slots[i].count;
        }
    }

    return 0;
}

bool inventory_is_unlimited_tile(const Inventory *inventory, int tile_id)
{
    if (inventory == NULL || tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return false;
    }

    return inventory->mode == GAME_MODE_CREATIVE;
}
