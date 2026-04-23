#ifndef LIRYNA_TILE_CATEGORIES_H
#define LIRYNA_TILE_CATEGORIES_H

typedef struct TileDefinition TileDefinition;

typedef enum TileCategory
{
    TILE_CATEGORY_TREES = 0,
    TILE_CATEGORY_PLANTS,
    TILE_CATEGORY_ROCKS,
    TILE_CATEGORY_FURNITURE,
    TILE_CATEGORY_WATER,
    TILE_CATEGORY_STRUCTURES,
    TILE_CATEGORY_TERRAIN,
    TILE_CATEGORY_MISC,
    TILE_CATEGORY_COUNT
} TileCategory;

TileCategory tile_category_for_definition(const TileDefinition *tile);
const char *tile_category_name(TileCategory category);

#endif
