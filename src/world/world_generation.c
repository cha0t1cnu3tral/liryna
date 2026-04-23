#include "world_generation.h"

#include <math.h>
#include <stddef.h>

#include "biome_tile_sets.h"

static int world_generation_index_from_xy(const World *world, int x, int y)
{
    return (y * world->width) + x;
}

static float world_generation_lerp(float a, float b, float t)
{
    return a + ((b - a) * t);
}

static float world_generation_smoothstep(float t)
{
    return t * t * (3.0f - (2.0f * t));
}

static float world_generation_temperature_to_celsius(float normalized_temperature)
{
    return -40.0f + (normalized_temperature * 85.0f);
}

static unsigned int world_generation_hash_u32(unsigned int value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static float world_generation_hash_to_unit_float(int x, int y, unsigned int seed)
{
    unsigned int n = seed;
    n ^= world_generation_hash_u32((unsigned int)x * 0x9e3779b1U);
    n ^= world_generation_hash_u32((unsigned int)y * 0x85ebca6bU);
    n = world_generation_hash_u32(n);
    return (float)(n & 0x00FFFFFFU) / 16777215.0f;
}

static float world_generation_value_noise(float x, float y, unsigned int seed)
{
    const int x0 = (int)floorf(x);
    const int y0 = (int)floorf(y);
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = world_generation_smoothstep(x - (float)x0);
    const float ty = world_generation_smoothstep(y - (float)y0);

    const float v00 = world_generation_hash_to_unit_float(x0, y0, seed);
    const float v10 = world_generation_hash_to_unit_float(x1, y0, seed);
    const float v01 = world_generation_hash_to_unit_float(x0, y1, seed);
    const float v11 = world_generation_hash_to_unit_float(x1, y1, seed);

    const float nx0 = world_generation_lerp(v00, v10, tx);
    const float nx1 = world_generation_lerp(v01, v11, tx);
    return world_generation_lerp(nx0, nx1, ty);
}

static float world_generation_fractal_noise(float x, float y, unsigned int seed, int octaves)
{
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float value = 0.0f;
    float total_amplitude = 0.0f;

    for (int i = 0; i < octaves; i++)
    {
        value += world_generation_value_noise(x * frequency,
                                              y * frequency,
                                              seed + (unsigned int)(i * 1013)) *
                 amplitude;
        total_amplitude += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    if (total_amplitude <= 0.0f)
    {
        return 0.0f;
    }

    return value / total_amplitude;
}

static BiomeType world_generation_choose_biome(float elevation, float moisture, float temperature_c)
{
    if (elevation < 0.26f)
    {
        return BIOME_OCEAN;
    }

    if (elevation < 0.31f)
    {
        return BIOME_COAST;
    }

    if (elevation < 0.35f && moisture > 0.62f)
    {
        return BIOME_LAKE;
    }

    if (temperature_c <= 5.0f)
    {
        if (elevation > 0.72f)
        {
            return BIOME_SNOWY_MOUNTAINS;
        }
        return BIOME_TUNDRA;
    }

    if (elevation > 0.78f)
    {
        return BIOME_MOUNTAINS;
    }

    if (elevation > 0.62f)
    {
        return BIOME_HILLS;
    }

    if (moisture > 0.72f)
    {
        if (temperature_c > 12.0f)
        {
            return BIOME_SWAMP;
        }
        return BIOME_FOREST;
    }

    if (moisture < 0.30f)
    {
        if (temperature_c > 24.0f)
        {
            return BIOME_DESERT;
        }
        return BIOME_DRY_PLAINS_STEPPE;
    }

    if (moisture > 0.50f)
    {
        return BIOME_FOREST;
    }

    return BIOME_PLAINS;
}

static bool world_generation_tile_is_frozen(TileId tile_id)
{
    return tile_id == TILE_SNOW || tile_id == TILE_ICE || tile_id == TILE_FROZENGROUND ||
           tile_id == TILE_PERMAFROST;
}

static BiomeType world_generation_adjust_biome_for_temperature(BiomeType biome_type,
                                                               float elevation,
                                                               float moisture,
                                                               float temperature_c)
{
    const BiomeDefinition *biome = biome_get_definition(biome_type);
    if (biome != NULL && temperature_c >= biome->min_temperature_c &&
        temperature_c <= biome->max_temperature_c)
    {
        return biome_type;
    }

    if (temperature_c <= 5.0f)
    {
        return elevation > 0.72f ? BIOME_SNOWY_MOUNTAINS : BIOME_TUNDRA;
    }

    if (moisture < 0.35f && temperature_c > 24.0f)
    {
        return BIOME_DESERT;
    }

    if (moisture > 0.72f && temperature_c > 12.0f)
    {
        return BIOME_SWAMP;
    }

    return BIOME_PLAINS;
}

static bool world_generation_is_water_biome(BiomeType biome_type)
{
    return biome_type == BIOME_OCEAN || biome_type == BIOME_LAKE || biome_type == BIOME_RIVER ||
           biome_type == BIOME_COAST;
}

static bool world_generation_is_cold_biome(BiomeType biome_type)
{
    return biome_type == BIOME_TUNDRA || biome_type == BIOME_SNOWY_MOUNTAINS;
}

static bool world_generation_tile_can_host_feature(TileId tile_id)
{
    const TileDefinition *tile = tiles_get_definition(tile_id);
    if (tile == NULL)
    {
        return false;
    }

    return tile->walkable && !tile->is_liquid && !tile->blocks_land_movement &&
           (tile->layer == TILE_LAYER_GROUND || tile->layer == TILE_LAYER_FLOOR);
}

static bool world_generation_feature_is_valid_for_climate(TileId feature_tile, float temperature_c)
{
    if ((feature_tile == TILE_TREEPALM || feature_tile == TILE_CACTUS ||
         feature_tile == TILE_DESERT_SHRUB) &&
        temperature_c < 14.0f)
    {
        return false;
    }

    if (feature_tile == TILE_TREEPINE && temperature_c > 26.0f)
    {
        return false;
    }

    return true;
}

static bool world_generation_base_is_sandy(TileId base_tile_id)
{
    return base_tile_id == TILE_SAND || base_tile_id == TILE_DESERTSAND;
}

static bool world_generation_base_is_wet(TileId base_tile_id)
{
    return base_tile_id == TILE_MUD || base_tile_id == TILE_WETSOIL ||
           base_tile_id == TILE_FLOODEDGROUND || base_tile_id == TILE_CLAY ||
           base_tile_id == TILE_MOSS || base_tile_id == TILE_FORESTFLOOR;
}

static bool world_generation_base_is_rocky(TileId base_tile_id)
{
    return base_tile_id == TILE_GRAVEL || base_tile_id == TILE_GRANITE ||
           base_tile_id == TILE_BASALT || base_tile_id == TILE_LIMESTONE ||
           base_tile_id == TILE_FROZENGROUND || base_tile_id == TILE_PERMAFROST ||
           base_tile_id == TILE_SNOW || base_tile_id == TILE_ICE;
}

static bool world_generation_feature_allowed_in_biome(BiomeType biome_type, TileId feature_tile)
{
    switch (biome_type)
    {
    case BIOME_FOREST:
        return feature_tile == TILE_TREEOAK || feature_tile == TILE_TREEPINE ||
               feature_tile == TILE_TREEBIRCH || feature_tile == TILE_TREESAPLING ||
               feature_tile == TILE_BUSH || feature_tile == TILE_BERRYBUSH ||
               feature_tile == TILE_MUSHROOMPATCH;
    case BIOME_PLAINS:
        return feature_tile == TILE_TREESAPLING || feature_tile == TILE_BUSH ||
               feature_tile == TILE_BERRYBUSH || feature_tile == TILE_FLOWERPATCH;
    case BIOME_SWAMP:
        return feature_tile == TILE_TREEDEAD || feature_tile == TILE_VINES ||
               feature_tile == TILE_VINEPATCH || feature_tile == TILE_MUSHROOMPATCH ||
               feature_tile == TILE_BUSH;
    case BIOME_DESERT:
        return feature_tile == TILE_CACTUS || feature_tile == TILE_DESERT_SHRUB ||
               feature_tile == TILE_TREEDEAD;
    case BIOME_DRY_PLAINS_STEPPE:
        return feature_tile == TILE_DESERT_SHRUB || feature_tile == TILE_TALLWEEDS ||
               feature_tile == TILE_BUSH;
    case BIOME_HILLS:
        return feature_tile == TILE_TREEOAK || feature_tile == TILE_BUSH ||
               feature_tile == TILE_ROCKCLUSTER || feature_tile == TILE_BOULDER;
    case BIOME_MOUNTAINS:
        return feature_tile == TILE_ROCKCLUSTER || feature_tile == TILE_BOULDER ||
               feature_tile == TILE_LARGEROCK || feature_tile == TILE_CAVEENTRANCE;
    case BIOME_TUNDRA:
        return feature_tile == TILE_TREEDEAD || feature_tile == TILE_ROCKCLUSTER ||
               feature_tile == TILE_BOULDER;
    case BIOME_SNOWY_MOUNTAINS:
        return feature_tile == TILE_BOULDER || feature_tile == TILE_LARGEROCK ||
               feature_tile == TILE_ROCKCLUSTER;
    case BIOME_COAST:
        return feature_tile == TILE_TREEPALM || feature_tile == TILE_BUSH ||
               feature_tile == TILE_ROCKCLUSTER;
    case BIOME_OCEAN:
    case BIOME_LAKE:
    case BIOME_RIVER:
    case BIOME_TYPE_COUNT:
    default:
        return false;
    }
}

static bool world_generation_feature_allowed_on_base(TileId feature_tile, TileId base_tile_id)
{
    if (!world_generation_tile_can_host_feature(base_tile_id))
    {
        return false;
    }

    switch (feature_tile)
    {
    case TILE_TREEOAK:
    case TILE_TREEBIRCH:
    case TILE_TREESAPLING:
    case TILE_BUSH:
    case TILE_BERRYBUSH:
    case TILE_FLOWERPATCH:
    case TILE_TALLWEEDS:
        return !world_generation_base_is_rocky(base_tile_id) &&
               !world_generation_base_is_sandy(base_tile_id);
    case TILE_TREEPINE:
        return !world_generation_base_is_sandy(base_tile_id);
    case TILE_TREEPALM:
        return world_generation_base_is_sandy(base_tile_id);
    case TILE_TREEDEAD:
        return base_tile_id != TILE_FORESTFLOOR;
    case TILE_VINES:
    case TILE_VINEPATCH:
    case TILE_MUSHROOMPATCH:
        return world_generation_base_is_wet(base_tile_id);
    case TILE_CACTUS:
    case TILE_DESERT_SHRUB:
        return world_generation_base_is_sandy(base_tile_id) || base_tile_id == TILE_GRAVEL;
    case TILE_ROCKCLUSTER:
    case TILE_BOULDER:
    case TILE_LARGEROCK:
    case TILE_CAVEENTRANCE:
        return world_generation_base_is_rocky(base_tile_id);
    default:
        return false;
    }
}

static float world_generation_feature_density(BiomeType biome_type)
{
    switch (biome_type)
    {
    case BIOME_FOREST:
        return 0.18f;
    case BIOME_SWAMP:
        return 0.16f;
    case BIOME_MOUNTAINS:
        return 0.14f;
    case BIOME_HILLS:
        return 0.13f;
    case BIOME_DESERT:
        return 0.10f;
    case BIOME_PLAINS:
    case BIOME_DRY_PLAINS_STEPPE:
    case BIOME_TUNDRA:
    case BIOME_SNOWY_MOUNTAINS:
    case BIOME_COAST:
        return 0.07f;
    case BIOME_OCEAN:
    case BIOME_LAKE:
    case BIOME_RIVER:
    case BIOME_TYPE_COUNT:
    default:
        return 0.0f;
    }
}

static TileId world_generation_choose_feature_tile(BiomeType biome_type,
                                                   TileId base_tile_id,
                                                   int x,
                                                   int y,
                                                   unsigned int seed,
                                                   float temperature_c,
                                                   float moisture,
                                                   float elevation)
{
    if (!world_generation_tile_can_host_feature(base_tile_id))
    {
        return TILE_ID_COUNT;
    }

    const TileId *feature_tiles = NULL;
    size_t feature_tile_count = 0;
    static const TileId forest_features[] = {
        TILE_TREEOAK, TILE_TREEPINE, TILE_TREEBIRCH, TILE_TREESAPLING, TILE_BUSH, TILE_BERRYBUSH,
        TILE_MUSHROOMPATCH};
    static const TileId plains_features[] = {
        TILE_TREESAPLING, TILE_BUSH, TILE_BERRYBUSH, TILE_FLOWERPATCH};
    static const TileId swamp_features[] = {
        TILE_TREEDEAD, TILE_VINES, TILE_VINEPATCH, TILE_MUSHROOMPATCH, TILE_BUSH};
    static const TileId desert_features[] = {TILE_CACTUS, TILE_DESERT_SHRUB, TILE_TREEDEAD};
    static const TileId dry_steppe_features[] = {TILE_DESERT_SHRUB, TILE_TALLWEEDS, TILE_BUSH};
    static const TileId hills_features[] = {TILE_TREEOAK, TILE_BUSH, TILE_ROCKCLUSTER, TILE_BOULDER};
    static const TileId mountain_features[] = {
        TILE_ROCKCLUSTER, TILE_BOULDER, TILE_LARGEROCK, TILE_CAVEENTRANCE};
    static const TileId tundra_features[] = {TILE_TREEDEAD, TILE_ROCKCLUSTER, TILE_BOULDER};
    static const TileId snowy_mountain_features[] = {TILE_BOULDER, TILE_LARGEROCK, TILE_ROCKCLUSTER};
    static const TileId coast_features[] = {TILE_TREEPALM, TILE_BUSH, TILE_ROCKCLUSTER};

    switch (biome_type)
    {
    case BIOME_FOREST:
        feature_tiles = forest_features;
        feature_tile_count = sizeof(forest_features) / sizeof(forest_features[0]);
        break;
    case BIOME_PLAINS:
        feature_tiles = plains_features;
        feature_tile_count = sizeof(plains_features) / sizeof(plains_features[0]);
        break;
    case BIOME_SWAMP:
        feature_tiles = swamp_features;
        feature_tile_count = sizeof(swamp_features) / sizeof(swamp_features[0]);
        break;
    case BIOME_DESERT:
        feature_tiles = desert_features;
        feature_tile_count = sizeof(desert_features) / sizeof(desert_features[0]);
        break;
    case BIOME_DRY_PLAINS_STEPPE:
        feature_tiles = dry_steppe_features;
        feature_tile_count = sizeof(dry_steppe_features) / sizeof(dry_steppe_features[0]);
        break;
    case BIOME_HILLS:
        feature_tiles = hills_features;
        feature_tile_count = sizeof(hills_features) / sizeof(hills_features[0]);
        break;
    case BIOME_MOUNTAINS:
        feature_tiles = mountain_features;
        feature_tile_count = sizeof(mountain_features) / sizeof(mountain_features[0]);
        break;
    case BIOME_TUNDRA:
        feature_tiles = tundra_features;
        feature_tile_count = sizeof(tundra_features) / sizeof(tundra_features[0]);
        break;
    case BIOME_SNOWY_MOUNTAINS:
        feature_tiles = snowy_mountain_features;
        feature_tile_count = sizeof(snowy_mountain_features) / sizeof(snowy_mountain_features[0]);
        break;
    case BIOME_COAST:
        feature_tiles = coast_features;
        feature_tile_count = sizeof(coast_features) / sizeof(coast_features[0]);
        break;
    case BIOME_OCEAN:
    case BIOME_LAKE:
    case BIOME_RIVER:
    case BIOME_TYPE_COUNT:
    default:
        return TILE_ID_COUNT;
    }

    if (feature_tiles == NULL || feature_tile_count == 0)
    {
        return TILE_ID_COUNT;
    }

    float spawn_chance = world_generation_feature_density(biome_type);
    if (world_generation_base_is_wet(base_tile_id))
    {
        spawn_chance += 0.04f;
    }
    if (elevation > 0.70f && world_generation_base_is_rocky(base_tile_id))
    {
        spawn_chance += 0.05f;
    }
    if (moisture < 0.25f && world_generation_base_is_sandy(base_tile_id))
    {
        spawn_chance += 0.03f;
    }
    if (spawn_chance > 0.42f)
    {
        spawn_chance = 0.42f;
    }

    const float cluster_noise =
        world_generation_fractal_noise((float)x * 0.028f, (float)y * 0.028f, seed + 2909U, 2);
    spawn_chance *= 0.55f + (cluster_noise * 0.90f);
    if (spawn_chance > 0.42f)
    {
        spawn_chance = 0.42f;
    }

    const float roll = world_generation_hash_to_unit_float(x, y, seed + 1907U);
    if (roll > spawn_chance)
    {
        return TILE_ID_COUNT;
    }

    const unsigned int hash = world_generation_hash_u32(seed ^ ((unsigned int)x * 2671U) ^
                                                        ((unsigned int)y * 1777U));
    for (size_t i = 0; i < feature_tile_count; i++)
    {
        const TileId candidate = feature_tiles[(hash + (unsigned int)i) % feature_tile_count];
        if (!world_generation_feature_allowed_in_biome(biome_type, candidate))
        {
            continue;
        }
        if (!world_generation_feature_is_valid_for_climate(candidate, temperature_c))
        {
            continue;
        }
        if (!world_generation_feature_allowed_on_base(candidate, base_tile_id))
        {
            continue;
        }
        if (candidate == TILE_CACTUS &&
            !(temperature_c > 16.0f && moisture < 0.30f && world_generation_base_is_sandy(base_tile_id)))
        {
            continue;
        }
        if (candidate == TILE_TREEPALM &&
            !(temperature_c > 18.0f && world_generation_base_is_sandy(base_tile_id)))
        {
            continue;
        }
        if (candidate == TILE_MUSHROOMPATCH && moisture < 0.55f)
        {
            continue;
        }
        if (candidate == TILE_CAVEENTRANCE &&
            !(elevation > 0.78f && world_generation_base_is_rocky(base_tile_id)))
        {
            continue;
        }
        if (candidate == TILE_TREEPINE && elevation < 0.45f)
        {
            continue;
        }

        {
            return candidate;
        }
    }

    return TILE_ID_COUNT;
}

static TileId world_generation_choose_tile_from_biome(const BiomeDefinition *biome,
                                                       int x,
                                                       int y,
                                                       unsigned int seed,
                                                       bool require_walkable,
                                                       float temperature_c)
{
    if (biome == NULL || biome->tiles == NULL || biome->tile_count == 0)
    {
        return TILE_GRASS;
    }

    const unsigned int hash = world_generation_hash_u32(seed ^ ((unsigned int)x * 92821U) ^
                                                        ((unsigned int)y * 68917U));

    if (!require_walkable)
    {
        const TileId candidate = biome->tiles[hash % biome->tile_count];
        if (temperature_c > 2.0f && world_generation_tile_is_frozen(candidate))
        {
            for (size_t i = 0; i < biome->tile_count; i++)
            {
                const TileId replacement = biome->tiles[(hash + (unsigned int)i) % biome->tile_count];
                if (!world_generation_tile_is_frozen(replacement))
                {
                    return replacement;
                }
            }
        }
        return candidate;
    }

    size_t walkable_count = 0;
    for (size_t i = 0; i < biome->tile_count; i++)
    {
        const TileDefinition *tile = tiles_get_definition(biome->tiles[i]);
        if (tile != NULL && tile->walkable && !tile->blocks_land_movement)
        {
            walkable_count++;
        }
    }

    if (walkable_count == 0)
    {
        return biome->tiles[hash % biome->tile_count];
    }

    const size_t choice = (size_t)(hash % walkable_count);
    size_t seen = 0;
    for (size_t i = 0; i < biome->tile_count; i++)
    {
        const TileDefinition *tile = tiles_get_definition(biome->tiles[i]);
        if (tile != NULL && tile->walkable && !tile->blocks_land_movement)
        {
            const bool frozen_tile = world_generation_tile_is_frozen(biome->tiles[i]);
            if (seen == choice && !(temperature_c > 2.0f && frozen_tile))
            {
                return biome->tiles[i];
            }
            seen++;
        }
    }

    for (size_t i = 0; i < biome->tile_count; i++)
    {
        const TileDefinition *tile = tiles_get_definition(biome->tiles[i]);
        if (tile == NULL || !tile->walkable || tile->blocks_land_movement)
        {
            continue;
        }

        if (temperature_c > 2.0f && world_generation_tile_is_frozen(biome->tiles[i]))
        {
            continue;
        }

        return biome->tiles[i];
    }

    return TILE_GRASS;
}

static bool world_generation_biome_is_spawn_preferred(BiomeType biome_type)
{
    return biome_type == BIOME_PLAINS || biome_type == BIOME_FOREST || biome_type == BIOME_HILLS ||
           biome_type == BIOME_DRY_PLAINS_STEPPE;
}

static bool world_generation_has_nearby_harsh_biome(const World *world,
                                                    int center_x,
                                                    int center_y,
                                                    int radius)
{
    if (world == NULL || world->biomes == NULL || radius <= 0)
    {
        return false;
    }

    const int min_x = center_x - radius;
    const int max_x = center_x + radius;
    const int min_y = center_y - radius;
    const int max_y = center_y + radius;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            if (x < 0 || y < 0 || x >= world->width || y >= world->height)
            {
                continue;
            }

            const int index = world_generation_index_from_xy(world, x, y);
            const BiomeType biome = world->biomes[index];
            if (world_generation_is_water_biome(biome) || world_generation_is_cold_biome(biome))
            {
                return true;
            }
        }
    }

    return false;
}

bool world_generate_procedural(World *world, unsigned int seed)
{
    if (world == NULL || world->tiles == NULL || world->biomes == NULL ||
        world->temperatures_c == NULL || world->width <= 0 || world->height <= 0)
    {
        return false;
    }

    const float inv_width = 1.0f / (float)world->width;
    const float inv_height = 1.0f / (float)world->height;

    for (int y = 0; y < world->height; y++)
    {
        for (int x = 0; x < world->width; x++)
        {
            const float nx = ((float)x * inv_width) - 0.5f;
            const float ny = ((float)y * inv_height) - 0.5f;
            const float distance_from_center = sqrtf((nx * nx) + (ny * ny));

            const float elevation_macro = world_generation_fractal_noise(
                (float)x * 0.0055f, (float)y * 0.0055f, seed + 11U, 2);
            const float elevation_micro = world_generation_fractal_noise(
                (float)x * 0.0130f, (float)y * 0.0130f, seed + 17U, 4);
            const float elevation_noise =
                (elevation_macro * 0.58f) + (elevation_micro * 0.42f);
            const float moisture_macro = world_generation_fractal_noise(
                (float)x * 0.0052f, (float)y * 0.0052f, seed + 41U, 2);
            const float moisture_micro = world_generation_fractal_noise(
                (float)x * 0.0110f, (float)y * 0.0110f, seed + 47U, 3);
            const float moisture = (moisture_macro * 0.64f) + (moisture_micro * 0.36f);

            float temperature = world_generation_fractal_noise(
                (float)x * 0.0095f, (float)y * 0.0095f, seed + 83U, 3);
            temperature = (temperature * 0.62f) +
                          (world_generation_fractal_noise(
                               (float)x * 0.0045f, (float)y * 0.0045f, seed + 89U, 2) *
                           0.38f);
            temperature -= fabsf(ny) * 0.42f;
            if (temperature < 0.0f)
            {
                temperature = 0.0f;
            }
            if (temperature > 1.0f)
            {
                temperature = 1.0f;
            }

            float elevation = elevation_noise - (distance_from_center * 0.62f);
            if (elevation < 0.0f)
            {
                elevation = 0.0f;
            }
            if (elevation > 1.0f)
            {
                elevation = 1.0f;
            }

            const float temperature_c = world_generation_temperature_to_celsius(temperature);
            BiomeType biome_type = world_generation_choose_biome(elevation, moisture, temperature_c);
            biome_type = world_generation_adjust_biome_for_temperature(
                biome_type, elevation, moisture, temperature_c);
            const BiomeDefinition *biome = biome_get_definition(biome_type);

            const bool is_water_biome = world_generation_is_water_biome(biome_type);
            const TileId base_tile_id = world_generation_choose_tile_from_biome(
                biome, x, y, seed + 131U, !is_water_biome, temperature_c);
            const TileId feature_tile_id = world_generation_choose_feature_tile(
                biome_type, base_tile_id, x, y, seed + 719U, temperature_c, moisture, elevation);
            const TileId tile_id = feature_tile_id != TILE_ID_COUNT ? feature_tile_id : base_tile_id;

            const int index = world_generation_index_from_xy(world, x, y);
            world->tiles[index] = tile_id;
            world->biomes[index] = biome_type;
            world->temperatures_c[index] = temperature_c;
        }
    }

    return true;
}

bool world_find_spawn_tile(const World *world, int *out_x, int *out_y)
{
    if (world == NULL || world->tiles == NULL || world->biomes == NULL || out_x == NULL ||
        out_y == NULL)
    {
        return false;
    }

    const int center_x = world->width / 2;
    const int center_y = world->height / 2;
    const int max_radius = world->width > world->height ? world->width : world->height;

    for (int pass = 0; pass < 4; pass++)
    {
        for (int radius = 0; radius <= max_radius; radius++)
        {
            const int min_x = center_x - radius;
            const int max_x = center_x + radius;
            const int min_y = center_y - radius;
            const int max_y = center_y + radius;

            for (int y = min_y; y <= max_y; y++)
            {
                for (int x = min_x; x <= max_x; x++)
                {
                    if (x < 0 || y < 0 || x >= world->width || y >= world->height)
                    {
                        continue;
                    }

                    if (x != min_x && x != max_x && y != min_y && y != max_y)
                    {
                        continue;
                    }

                    const int index = world_generation_index_from_xy(world, x, y);
                    const BiomeType biome = world->biomes[index];
                    if (pass == 0 &&
                        (!world_generation_biome_is_spawn_preferred(biome) ||
                         world_generation_has_nearby_harsh_biome(world, x, y, 10)))
                    {
                        continue;
                    }
                    if (pass == 1 &&
                        (world_generation_is_cold_biome(biome) ||
                         world_generation_is_water_biome(biome) ||
                         world_generation_has_nearby_harsh_biome(world, x, y, 6)))
                    {
                        continue;
                    }
                    if (pass == 2 &&
                        (world_generation_is_water_biome(biome) ||
                         world_generation_has_nearby_harsh_biome(world, x, y, 3)))
                    {
                        continue;
                    }

                    const TileDefinition *tile = tiles_get_definition(world->tiles[index]);
                    if (tile != NULL && tile->walkable && !tile->blocks_land_movement)
                    {
                        *out_x = x;
                        *out_y = y;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}
