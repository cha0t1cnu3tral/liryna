#ifndef BIOME_TILE_SETS_H
#define BIOME_TILE_SETS_H

#include <stdbool.h>
#include <stddef.h>

#include "tiles.h"

typedef enum BiomeType
{
    BIOME_OCEAN = 0,
    BIOME_LAKE,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_SWAMP,
    BIOME_DESERT,
    BIOME_DRY_PLAINS_STEPPE,
    BIOME_HILLS,
    BIOME_MOUNTAINS,
    BIOME_TUNDRA,
    BIOME_SNOWY_MOUNTAINS,
    BIOME_COAST,
    BIOME_RIVER,
    BIOME_TYPE_COUNT
} BiomeType;

typedef struct BiomeDefinition
{
    BiomeType type;
    const char *name;
    const char *description;
    const TileId *tiles;
    size_t tile_count;
} BiomeDefinition;

size_t biome_count(void);
const BiomeDefinition *biome_get_definition(BiomeType type);
bool biome_contains_tile(BiomeType type, TileId tile_id);

#endif
