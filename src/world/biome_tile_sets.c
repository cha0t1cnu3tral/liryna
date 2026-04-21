#include "biome_tile_sets.h"

static const TileId g_biome_ocean_tiles[] = {
    TILE_OCEANWATER,
    TILE_DEEPWATER,
    TILE_SHALLOWWATER,
    TILE_WATERFOAM,
    TILE_WATERCURRENT,
    TILE_WATEREDGE};

static const TileId g_biome_lake_tiles[] = {
    TILE_LAKEWATER,
    TILE_PONDWATER,
    TILE_SHALLOWWATER,
    TILE_WATEREDGE,
    TILE_WETSOIL,
    TILE_CLAY};

static const TileId g_biome_plains_tiles[] = {
    TILE_GRASS,
    TILE_TALLGRASS,
    TILE_DIRT,
    TILE_PACKEDDIRT,
    TILE_FLOWERPATCH,
    TILE_BUSH};

static const TileId g_biome_forest_tiles[] = {
    TILE_FORESTFLOOR,
    TILE_MOSS,
    TILE_GRASS,
    TILE_TREEOAK,
    TILE_TREEPINE,
    TILE_TREEBIRCH,
    TILE_TREESAPLING,
    TILE_BUSH,
    TILE_BERRYBUSH,
    TILE_MUSHROOMPATCH};

static const TileId g_biome_swamp_tiles[] = {
    TILE_SWAMPWATER,
    TILE_MARSHWATER,
    TILE_SHALLOWWATER,
    TILE_MUD,
    TILE_WETSOIL,
    TILE_FLOODEDGROUND,
    TILE_TREEDEAD,
    TILE_VINES,
    TILE_VINEPATCH};

static const TileId g_biome_desert_tiles[] = {
    TILE_DESERTSAND,
    TILE_SAND,
    TILE_DRYGRASS,
    TILE_CACTUS,
    TILE_DESERT_SHRUB,
    TILE_GRAVEL};

static const TileId g_biome_dry_plains_steppe_tiles[] = {
    TILE_DRYGRASS,
    TILE_PACKEDDIRT,
    TILE_DIRT,
    TILE_TALLWEEDS,
    TILE_DESERT_SHRUB,
    TILE_GRAVEL};

static const TileId g_biome_hills_tiles[] = {
    TILE_GRASS,
    TILE_DIRT,
    TILE_GRAVEL,
    TILE_LIMESTONE,
    TILE_ROCKCLUSTER,
    TILE_BOULDER};

static const TileId g_biome_mountains_tiles[] = {
    TILE_GRANITE,
    TILE_BASALT,
    TILE_LIMESTONE,
    TILE_GRAVEL,
    TILE_ROCKCLUSTER,
    TILE_LARGEROCK,
    TILE_BOULDER,
    TILE_CAVEENTRANCE};

static const TileId g_biome_tundra_tiles[] = {
    TILE_FROZENGROUND,
    TILE_PERMAFROST,
    TILE_SNOW,
    TILE_ICE,
    TILE_TREEDEAD,
    TILE_GRAVEL};

static const TileId g_biome_snowy_mountains_tiles[] = {
    TILE_SNOW,
    TILE_ICE,
    TILE_PERMAFROST,
    TILE_GRANITE,
    TILE_BASALT,
    TILE_LARGEROCK,
    TILE_BOULDER};

static const TileId g_biome_coast_tiles[] = {
    TILE_SAND,
    TILE_SHALLOWWATER,
    TILE_WATEREDGE,
    TILE_GRAVEL,
    TILE_CLAY,
    TILE_WETSOIL};

static const TileId g_biome_river_tiles[] = {
    TILE_RIVERWATER,
    TILE_STREAMWATER,
    TILE_WATERCURRENT,
    TILE_WATEREDGE,
    TILE_SHALLOWWATER,
    TILE_MUD,
    TILE_WETSOIL};

static const BiomeDefinition g_biome_definitions[BIOME_TYPE_COUNT] = {
    {BIOME_OCEAN, "Ocean", "Deep water and coastlines.", -2.0f, 30.0f,
     g_biome_ocean_tiles, sizeof(g_biome_ocean_tiles) / sizeof(g_biome_ocean_tiles[0])},
    {BIOME_LAKE, "Lake", "Inland standing water.", 0.0f, 25.0f, g_biome_lake_tiles,
     sizeof(g_biome_lake_tiles) / sizeof(g_biome_lake_tiles[0])},
    {BIOME_PLAINS, "Plains", "Open grassland terrain.", -20.0f, 30.0f, g_biome_plains_tiles,
     sizeof(g_biome_plains_tiles) / sizeof(g_biome_plains_tiles[0])},
    {BIOME_FOREST, "Forest", "Tree-dense natural land.", -30.0f, 30.0f, g_biome_forest_tiles,
     sizeof(g_biome_forest_tiles) / sizeof(g_biome_forest_tiles[0])},
    {BIOME_SWAMP, "Swamp", "Wet ground and standing water.", 5.0f, 35.0f, g_biome_swamp_tiles,
     sizeof(g_biome_swamp_tiles) / sizeof(g_biome_swamp_tiles[0])},
    {BIOME_DESERT, "Desert", "Dry sand with sparse vegetation.", -4.0f, 38.0f,
     g_biome_desert_tiles, sizeof(g_biome_desert_tiles) / sizeof(g_biome_desert_tiles[0])},
    {BIOME_DRY_PLAINS_STEPPE, "Dry Plains / Steppe", "Dry grassland transition terrain.",
     -10.0f, 35.0f, g_biome_dry_plains_steppe_tiles,
     sizeof(g_biome_dry_plains_steppe_tiles) / sizeof(g_biome_dry_plains_steppe_tiles[0])},
    {BIOME_HILLS, "Hills", "Rolling elevated terrain.", -10.0f, 25.0f, g_biome_hills_tiles,
     sizeof(g_biome_hills_tiles) / sizeof(g_biome_hills_tiles[0])},
    {BIOME_MOUNTAINS, "Mountains", "Steep high-elevation terrain.", -20.0f, 15.0f,
     g_biome_mountains_tiles, sizeof(g_biome_mountains_tiles) / sizeof(g_biome_mountains_tiles[0])},
    {BIOME_TUNDRA, "Tundra", "Cold flat terrain.", -40.0f, 18.0f, g_biome_tundra_tiles,
     sizeof(g_biome_tundra_tiles) / sizeof(g_biome_tundra_tiles[0])},
    {BIOME_SNOWY_MOUNTAINS, "Snowy Mountains", "Cold high-elevation terrain.", -40.0f, 5.0f,
     g_biome_snowy_mountains_tiles,
     sizeof(g_biome_snowy_mountains_tiles) / sizeof(g_biome_snowy_mountains_tiles[0])},
    {BIOME_COAST, "Coast", "Shoreline where land meets ocean.", -2.0f, 30.0f, g_biome_coast_tiles,
     sizeof(g_biome_coast_tiles) / sizeof(g_biome_coast_tiles[0])},
    {BIOME_RIVER, "River", "Flowing water biome.", 0.0f, 25.0f, g_biome_river_tiles,
     sizeof(g_biome_river_tiles) / sizeof(g_biome_river_tiles[0])}};

size_t biome_count(void)
{
    return BIOME_TYPE_COUNT;
}

const BiomeDefinition *biome_get_definition(BiomeType type)
{
    if (type < 0 || type >= BIOME_TYPE_COUNT)
    {
        return NULL;
    }

    return &g_biome_definitions[type];
}

bool biome_contains_tile(BiomeType type, TileId tile_id)
{
    const BiomeDefinition *biome = biome_get_definition(type);
    if (biome == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < biome->tile_count; i++)
    {
        if (biome->tiles[i] == tile_id)
        {
            return true;
        }
    }

    return false;
}
