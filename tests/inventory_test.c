#include "inventory.h"
#include "world/tiles.h"

#include <assert.h>

int main(void)
{
    Inventory inventory;
    inventory_init(&inventory, GAME_MODE_SURVIVAL);

    assert(inventory_add_survival(&inventory, TILE_DOG_FOOD, 2));
    assert(inventory_tile_count(&inventory, TILE_DOG_FOOD) == 2);

    assert(inventory_remove_survival(&inventory, TILE_DOG_FOOD, 1));
    assert(inventory_tile_count(&inventory, TILE_DOG_FOOD) == 1);
    assert(!inventory_remove_survival(&inventory, TILE_DOG_FOOD, 2));
    assert(inventory_tile_count(&inventory, TILE_DOG_FOOD) == 1);

    assert(inventory_remove_survival(&inventory, TILE_DOG_FOOD, 1));
    assert(inventory_tile_count(&inventory, TILE_DOG_FOOD) == 0);
    assert(!inventory_remove_survival(&inventory, TILE_DOG_FOOD, 1));

    inventory_init(&inventory, GAME_MODE_CREATIVE);
    assert(!inventory_remove_survival(&inventory, TILE_DOG_FOOD, 1));

    return 0;
}
