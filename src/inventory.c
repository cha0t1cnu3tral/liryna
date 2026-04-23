#include "inventory.h"

#include "world/tiles.h"

void inventory_init(Inventory *inventory, GameMode mode)
{
    if (inventory == NULL)
    {
        return;
    }

    inventory->mode = mode;
    inventory->active_hotbar_slot = 0;
    inventory->selected_tile = TILE_GRASS;

    for (int i = 0; i < INVENTORY_SURVIVAL_SLOT_COUNT; i++)
    {
        inventory->slots[i].occupied = false;
        inventory->slots[i].tile_id = TILE_ID_COUNT;
        inventory->slots[i].count = 0;
    }

    for (int i = 0; i < INVENTORY_HOTBAR_SLOT_COUNT; i++)
    {
        inventory->hotbar_tiles[i] = TILE_ID_COUNT;
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

bool inventory_assign_hotbar_slot(Inventory *inventory, int slot_index, int tile_id)
{
    if (inventory == NULL ||
        slot_index < 0 || slot_index >= INVENTORY_HOTBAR_SLOT_COUNT ||
        tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return false;
    }

    inventory->hotbar_tiles[slot_index] = tile_id;
    return true;
}

int inventory_hotbar_tile(const Inventory *inventory, int slot_index)
{
    if (inventory == NULL ||
        slot_index < 0 || slot_index >= INVENTORY_HOTBAR_SLOT_COUNT)
    {
        return TILE_ID_COUNT;
    }

    return inventory->hotbar_tiles[slot_index];
}

bool inventory_select_hotbar_slot(Inventory *inventory, int slot_index)
{
    if (inventory == NULL ||
        slot_index < 0 || slot_index >= INVENTORY_HOTBAR_SLOT_COUNT)
    {
        return false;
    }

    inventory->active_hotbar_slot = slot_index;
    const int tile_id = inventory_hotbar_tile(inventory, slot_index);
    if (tile_id >= 0 && tile_id < TILE_ID_COUNT)
    {
        inventory->selected_tile = tile_id;
    }

    return true;
}
