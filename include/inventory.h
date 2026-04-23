#ifndef LIRYNA_INVENTORY_H
#define LIRYNA_INVENTORY_H

#include <stdbool.h>

typedef enum GameMode
{
    GAME_MODE_SURVIVAL = 0,
    GAME_MODE_CREATIVE
} GameMode;

typedef struct InventorySlot
{
    bool occupied;
    int tile_id;
    int count;
} InventorySlot;

enum
{
    INVENTORY_SURVIVAL_SLOT_COUNT = 36,
    INVENTORY_HOTBAR_SLOT_COUNT = 9
};

typedef struct Inventory
{
    GameMode mode;
    InventorySlot slots[INVENTORY_SURVIVAL_SLOT_COUNT];
    int hotbar_tiles[INVENTORY_HOTBAR_SLOT_COUNT];
    int active_hotbar_slot;
    int selected_tile;
} Inventory;

void inventory_init(Inventory *inventory, GameMode mode);
bool inventory_add_survival(Inventory *inventory, int tile_id, int count);
int inventory_tile_count(const Inventory *inventory, int tile_id);
bool inventory_is_unlimited_tile(const Inventory *inventory, int tile_id);
bool inventory_assign_hotbar_slot(Inventory *inventory, int slot_index, int tile_id);
int inventory_hotbar_tile(const Inventory *inventory, int slot_index);
bool inventory_select_hotbar_slot(Inventory *inventory, int slot_index);

#endif
