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

            const float elevation_noise = world_generation_fractal_noise(
                (float)x * 0.055f, (float)y * 0.055f, seed + 17U, 4);
            const float moisture = world_generation_fractal_noise(
                (float)x * 0.048f, (float)y * 0.048f, seed + 47U, 3);

            float temperature = world_generation_fractal_noise(
                (float)x * 0.04f, (float)y * 0.04f, seed + 83U, 3);
            temperature -= fabsf(ny) * 0.35f;
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

            const bool is_water_biome = biome_type == BIOME_OCEAN || biome_type == BIOME_LAKE ||
                                        biome_type == BIOME_RIVER || biome_type == BIOME_COAST;
            const TileId tile_id = world_generation_choose_tile_from_biome(
                biome, x, y, seed + 131U, !is_water_biome, temperature_c);

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
    if (world == NULL || world->tiles == NULL || out_x == NULL || out_y == NULL)
    {
        return false;
    }

    const int center_x = world->width / 2;
    const int center_y = world->height / 2;
    const int max_radius = world->width > world->height ? world->width : world->height;

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

                const TileDefinition *tile =
                    tiles_get_definition(world->tiles[world_generation_index_from_xy(world, x, y)]);
                if (tile != NULL && tile->walkable && !tile->blocks_land_movement)
                {
                    *out_x = x;
                    *out_y = y;
                    return true;
                }
            }
        }
    }

    return false;
}
