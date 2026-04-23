#include "tile_categories.h"
#include "world/tiles.h"

const char *tile_category_name(TileCategory category)
{
    switch (category)
    {
    case TILE_CATEGORY_TREES:
        return "Trees";
    case TILE_CATEGORY_PLANTS:
        return "Plants";
    case TILE_CATEGORY_ROCKS:
        return "Rocks";
    case TILE_CATEGORY_FURNITURE:
        return "Furniture";
    case TILE_CATEGORY_WATER:
        return "Water";
    case TILE_CATEGORY_STRUCTURES:
        return "Structures";
    case TILE_CATEGORY_TERRAIN:
        return "Terrain";
    case TILE_CATEGORY_MISC:
    default:
        return "Misc";
    }
}

TileCategory tile_category_for_definition(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return TILE_CATEGORY_MISC;
    }

    switch (tile->id)
    {
    case TILE_TREEOAK:
    case TILE_TREEPINE:
    case TILE_TREEBIRCH:
    case TILE_TREEPALM:
    case TILE_TREEDEAD:
    case TILE_TREEBURNT:
    case TILE_TREESAPLING:
        return TILE_CATEGORY_TREES;
    case TILE_BUSH:
    case TILE_BERRYBUSH:
    case TILE_FLOWERPATCH:
    case TILE_TALLWEEDS:
    case TILE_VINEPATCH:
    case TILE_VINES:
    case TILE_MUSHROOMPATCH:
    case TILE_GIANTMUSHROOM:
    case TILE_CACTUS:
    case TILE_DESERT_SHRUB:
        return TILE_CATEGORY_PLANTS;
    case TILE_BOULDER:
    case TILE_ROCKCLUSTER:
    case TILE_LARGEROCK:
    case TILE_CAVEENTRANCE:
        return TILE_CATEGORY_ROCKS;
    case TILE_TABLE:
    case TILE_TABLEROUND:
    case TILE_TABLELONG:
    case TILE_CHAIRWOOD:
    case TILE_CHAIRMETAL:
    case TILE_SOFA:
    case TILE_BED:
    case TILE_BUNKBED:
    case TILE_CABINET:
    case TILE_SHELF:
    case TILE_BOOKSHELF:
    case TILE_CHEST:
    case TILE_LOCKER:
    case TILE_DESK:
    case TILE_OFFICEDESK:
    case TILE_LAMP:
    case TILE_CEILINGLIGHT:
    case TILE_FLOORLAMP:
    case TILE_STOVE:
    case TILE_OVEN:
    case TILE_SINK:
    case TILE_BATHTUB:
    case TILE_TOILET:
    case TILE_SHOWER:
    case TILE_FRIDGE:
    case TILE_WASHINGMACHINE:
    case TILE_DRYER:
    case TILE_SMALLAXE:
    case TILE_PICKAXE:
    case TILE_READER:
    case TILE_RADIO:
        return TILE_CATEGORY_FURNITURE;
    case TILE_WOODFOUNDATION:
    case TILE_STONEFOUNDATION:
    case TILE_CONCRETEFOUNDATION:
    case TILE_BRICKFOUNDATION:
    case TILE_STEELFOUNDATION:
    case TILE_REINFORCEDFOUNDATION:
    case TILE_PIER:
    case TILE_CARPET:
    case TILE_LOGWALL:
    case TILE_PLANKWALL:
    case TILE_STONEWALL:
    case TILE_BRICKWALL:
    case TILE_CONCRETEWALL:
    case TILE_REINFORCEDWALL:
    case TILE_STEELWALL:
    case TILE_GLASSWALL:
    case TILE_METALWALL:
    case TILE_FENCEWOOD:
    case TILE_FENCEMETAL:
    case TILE_FENCESTONE:
    case TILE_FENCECHAIN:
    case TILE_FENCEBARBED:
    case TILE_BARRICADEWOOD:
    case TILE_BARRICADESANDBAG:
    case TILE_RUINEDWALL:
    case TILE_CRACKEDWALL:
    case TILE_SHIPPIECE:
        return TILE_CATEGORY_STRUCTURES;
    default:
        break;
    }

    if (tile->is_liquid)
    {
        return TILE_CATEGORY_WATER;
    }

    switch (tile->id)
    {
    case TILE_GRASS:
    case TILE_DRYGRASS:
    case TILE_TALLGRASS:
    case TILE_DIRT:
    case TILE_PACKEDDIRT:
    case TILE_MUD:
    case TILE_SAND:
    case TILE_DESERTSAND:
    case TILE_GRAVEL:
    case TILE_CLAY:
    case TILE_MOSS:
    case TILE_FORESTFLOOR:
    case TILE_SNOW:
    case TILE_ICE:
    case TILE_FROZENGROUND:
    case TILE_PERMAFROST:
    case TILE_BASALT:
    case TILE_GRANITE:
    case TILE_LIMESTONE:
    case TILE_WETSOIL:
    case TILE_FLOODEDGROUND:
    case TILE_OILSPILL:
        return TILE_CATEGORY_TERRAIN;
    default:
        break;
    }

    return TILE_CATEGORY_MISC;
}
