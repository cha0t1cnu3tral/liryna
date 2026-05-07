#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "world_generation.h"

static int world_index_from_xy(const World *world, int x, int y)
{
    return (y * world->width) + x;
}

static TileId *world_layer_storage(World *world, TileLayer layer)
{
    if (world == NULL)
    {
        return NULL;
    }

    switch (layer)
    {
    case TILE_LAYER_GROUND:
        return world->ground_tiles;
    case TILE_LAYER_FLOOR:
        return world->floor_tiles;
    case TILE_LAYER_OBJECT:
        return world->object_tiles;
    case TILE_LAYER_STRUCTURE:
        return world->structure_tiles;
    case TILE_LAYER_UNKNOWN:
    case TILE_LAYER_COUNT:
    default:
        return NULL;
    }
}

static const TileId *world_layer_storage_const(const World *world, TileLayer layer)
{
    return world_layer_storage((World *)world, layer);
}

bool world_is_in_bounds(const World *world, int tile_x, int tile_y)
{
    return world != NULL && tile_x >= 0 && tile_y >= 0 &&
           tile_x < world->width && tile_y < world->height;
}

TileId world_get_tile_id_at_layer(const World *world, int tile_x, int tile_y, TileLayer layer)
{
    const TileId *tiles = world_layer_storage_const(world, layer);
    if (tiles == NULL || !world_is_in_bounds(world, tile_x, tile_y))
    {
        return TILE_ID_COUNT;
    }

    return tiles[world_index_from_xy(world, tile_x, tile_y)];
}

const TileDefinition *world_get_tile_at_layer(const World *world,
                                              int tile_x,
                                              int tile_y,
                                              TileLayer layer)
{
    return tiles_get_definition(world_get_tile_id_at_layer(world, tile_x, tile_y, layer));
}

const TileDefinition *world_get_top_tile_at(const World *world, int tile_x, int tile_y)
{
    static const TileLayer k_layers[] = {
        TILE_LAYER_STRUCTURE,
        TILE_LAYER_OBJECT,
        TILE_LAYER_FLOOR,
        TILE_LAYER_GROUND,
    };

    for (int i = 0; i < (int)(sizeof(k_layers) / sizeof(k_layers[0])); i++)
    {
        const TileDefinition *tile = world_get_tile_at_layer(world, tile_x, tile_y, k_layers[i]);
        if (tile != NULL)
        {
            return tile;
        }
    }

    return NULL;
}

const TileDefinition *world_get_supporting_tile_at(const World *world, int tile_x, int tile_y)
{
    static const TileLayer k_layers[] = {
        TILE_LAYER_STRUCTURE,
        TILE_LAYER_OBJECT,
        TILE_LAYER_FLOOR,
        TILE_LAYER_GROUND,
    };

    for (int i = 0; i < (int)(sizeof(k_layers) / sizeof(k_layers[0])); i++)
    {
        const TileDefinition *tile = world_get_tile_at_layer(world, tile_x, tile_y, k_layers[i]);
        if (tile == NULL)
        {
            continue;
        }

        if (tile->walkable && !tile->blocks_land_movement)
        {
            return tile;
        }
    }

    return NULL;
}

bool world_can_occupy_tile(const World *world,
                           int tile_x,
                           int tile_y,
                           bool *out_swimming,
                           const TileDefinition **out_support_tile,
                           const TileDefinition **out_top_tile)
{
    static const TileLayer k_layers[] = {
        TILE_LAYER_STRUCTURE,
        TILE_LAYER_OBJECT,
        TILE_LAYER_FLOOR,
        TILE_LAYER_GROUND,
    };

    bool can_walk = false;
    bool can_swim = false;
    const TileDefinition *support_tile = NULL;
    const TileDefinition *top_tile = NULL;

    for (int i = 0; i < (int)(sizeof(k_layers) / sizeof(k_layers[0])); i++)
    {
        const TileDefinition *tile = world_get_tile_at_layer(world, tile_x, tile_y, k_layers[i]);
        if (tile == NULL)
        {
            continue;
        }

        if (top_tile == NULL)
        {
            top_tile = tile;
        }

        if (tile->blocks_land_movement)
        {
            can_walk = false;
            can_swim = false;
            support_tile = NULL;
            break;
        }

        if (!can_walk && tile->walkable)
        {
            can_walk = true;
            support_tile = tile;
            can_swim = false;
            break;
        }

        if (!can_walk && !can_swim && tile->is_liquid && !tile->blocks_swimming)
        {
            can_swim = true;
            support_tile = tile;
        }
    }

    if (out_swimming != NULL)
    {
        *out_swimming = can_swim;
    }
    if (out_support_tile != NULL)
    {
        *out_support_tile = support_tile;
    }
    if (out_top_tile != NULL)
    {
        *out_top_tile = top_tile;
    }

    return can_walk || can_swim;
}

static bool world_can_stand_on(const World *world, float x, float y)
{
    if (world == NULL || world->tile_size <= 0)
    {
        return false;
    }

    const int tile_x = (int)(x / (float)world->tile_size);
    const int tile_y = (int)(y / (float)world->tile_size);
    return world_can_occupy_tile(world, tile_x, tile_y, NULL, NULL, NULL);
}

static const TileDefinition *world_get_supporting_tile_definition_at(const World *world,
                                                                     float x,
                                                                     float y)
{
    if (world == NULL || world->tile_size <= 0)
    {
        return NULL;
    }

    const int tile_x = (int)(x / (float)world->tile_size);
    const int tile_y = (int)(y / (float)world->tile_size);
    return world_get_supporting_tile_at(world, tile_x, tile_y);
}

bool world_set_tile_at_layer(World *world, int tile_x, int tile_y, TileLayer layer, TileId tile_id)
{
    TileId *tiles = world_layer_storage(world, layer);
    if (tiles == NULL || !world_is_in_bounds(world, tile_x, tile_y))
    {
        return false;
    }

    if (tile_id != TILE_ID_COUNT)
    {
        const TileDefinition *tile = tiles_get_definition(tile_id);
        if (tile == NULL || tile->layer != layer)
        {
            return false;
        }
    }

    tiles[world_index_from_xy(world, tile_x, tile_y)] = tile_id;
    return true;
}

bool world_set_tile(World *world, int tile_x, int tile_y, TileId tile_id)
{
    const TileDefinition *tile = tiles_get_definition(tile_id);
    if (tile == NULL || tile->layer == TILE_LAYER_UNKNOWN || tile->layer >= TILE_LAYER_COUNT)
    {
        return false;
    }

    return world_set_tile_at_layer(world, tile_x, tile_y, tile->layer, tile_id);
}

bool world_clear_tile_at_layer(World *world, int tile_x, int tile_y, TileLayer layer)
{
    return world_set_tile_at_layer(world, tile_x, tile_y, layer, TILE_ID_COUNT);
}

static SDL_Color world_tile_color(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return (SDL_Color){70, 70, 70, 255};
    }

    if (tile->is_liquid)
    {
        return (SDL_Color){40, 95, 185, 255};
    }

    switch (tile->layer)
    {
    case TILE_LAYER_GROUND:
        return (SDL_Color){70, 120, 70, 255};
    case TILE_LAYER_FLOOR:
        return (SDL_Color){120, 105, 80, 255};
    case TILE_LAYER_OBJECT:
        return (SDL_Color){145, 100, 60, 255};
    case TILE_LAYER_STRUCTURE:
        return (SDL_Color){110, 110, 120, 255};
    case TILE_LAYER_UNKNOWN:
    case TILE_LAYER_COUNT:
    default:
        return (SDL_Color){95, 95, 95, 255};
    }
}

static SDL_FRect world_layer_rect(const World *world, int x, int y, TileLayer layer)
{
    const float tile_size = (float)world->tile_size;
    const float origin_x = (float)(x * world->tile_size);
    const float origin_y = (float)(y * world->tile_size);
    float inset = 0.0f;

    switch (layer)
    {
    case TILE_LAYER_FLOOR:
        inset = tile_size * 0.08f;
        break;
    case TILE_LAYER_OBJECT:
        inset = tile_size * 0.22f;
        break;
    case TILE_LAYER_STRUCTURE:
        inset = tile_size * 0.12f;
        break;
    case TILE_LAYER_GROUND:
    case TILE_LAYER_UNKNOWN:
    case TILE_LAYER_COUNT:
    default:
        inset = 0.0f;
        break;
    }

    SDL_FRect rect;
    rect.x = origin_x + inset;
    rect.y = origin_y + inset;
    rect.w = tile_size - (inset * 2.0f);
    rect.h = tile_size - (inset * 2.0f);
    return rect;
}

static bool world_allocate(World *world, int width, int height, int tile_size)
{
    if (world == NULL || width <= 0 || height <= 0 || tile_size <= 0)
    {
        return false;
    }

    world->width = width;
    world->height = height;
    world->tile_size = tile_size;
    world->ground_tiles = NULL;
    world->floor_tiles = NULL;
    world->object_tiles = NULL;
    world->structure_tiles = NULL;
    world->biomes = NULL;
    world->temperatures_c = NULL;

    world->player_x = (width * tile_size) / 2.0f;
    world->player_y = (height * tile_size) / 2.0f;
    world->player_speed = 200.0f;
    world->player_swim_speed = 130.0f;
    world->player_jump_duration = 0.2f;
    world->player_jump_time_remaining = 0.0f;
    world->player_jump_speed_multiplier = 1.75f;
    world->player_is_swimming = false;
    world->player_is_jumping = false;

    const int tile_count = width * height;
    world->ground_tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->ground_tiles == NULL)
    {
        world_shutdown(world);
        return false;
    }
    world->floor_tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->floor_tiles == NULL)
    {
        world_shutdown(world);
        return false;
    }
    world->object_tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->object_tiles == NULL)
    {
        world_shutdown(world);
        return false;
    }
    world->structure_tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->structure_tiles == NULL)
    {
        world_shutdown(world);
        return false;
    }
    world->biomes = (BiomeType *)malloc((size_t)tile_count * sizeof(BiomeType));
    if (world->biomes == NULL)
    {
        world_shutdown(world);
        return false;
    }
    world->temperatures_c = (float *)malloc((size_t)tile_count * sizeof(float));
    if (world->temperatures_c == NULL)
    {
        world_shutdown(world);
        return false;
    }

    for (int i = 0; i < tile_count; i++)
    {
        world->ground_tiles[i] = TILE_ID_COUNT;
        world->floor_tiles[i] = TILE_ID_COUNT;
        world->object_tiles[i] = TILE_ID_COUNT;
        world->structure_tiles[i] = TILE_ID_COUNT;
    }

    return true;
}

bool world_init(World *world, int width, int height, int tile_size)
{
    if (!world_allocate(world, width, height, tile_size))
    {
        return false;
    }

    const unsigned int seed = (unsigned int)time(NULL);
    if (!world_generate_procedural(world, seed))
    {
        world_shutdown(world);
        return false;
    }

    int spawn_tile_x = width / 2;
    int spawn_tile_y = height / 2;
    if (world_find_spawn_tile(world, &spawn_tile_x, &spawn_tile_y))
    {
        world->player_x = (float)(spawn_tile_x * tile_size);
        world->player_y = (float)(spawn_tile_y * tile_size);
    }

    return true;
}

bool world_init_flat(World *world,
                     int width,
                     int height,
                     int tile_size,
                     TileId ground_tile,
                     BiomeType biome_type,
                     float temperature_c)
{
    if (!world_allocate(world, width, height, tile_size))
    {
        return false;
    }

    const TileDefinition *ground = tiles_get_definition(ground_tile);
    if (ground == NULL || ground->layer != TILE_LAYER_GROUND)
    {
        world_shutdown(world);
        return false;
    }

    const int tile_count = width * height;
    for (int i = 0; i < tile_count; i++)
    {
        world->ground_tiles[i] = ground_tile;
        world->biomes[i] = biome_type;
        world->temperatures_c[i] = temperature_c;
    }

    world->player_x = (float)((width / 2) * tile_size);
    world->player_y = (float)((height / 2) * tile_size);
    return true;
}

void world_update(World *world, float delta_time, float move_x, float move_y, bool jump_pressed)
{
    if (world == NULL)
    {
        return;
    }

    bool swimming_here = false;
    world_can_occupy_tile(world,
                          (int)(world->player_x / (float)world->tile_size),
                          (int)(world->player_y / (float)world->tile_size),
                          &swimming_here,
                          NULL,
                          NULL);
    world->player_is_swimming = swimming_here;

    if (jump_pressed && !world->player_is_swimming)
    {
        world->player_jump_time_remaining = world->player_jump_duration;
    }

    if (world->player_jump_time_remaining > 0.0f)
    {
        world->player_jump_time_remaining -= delta_time;
        world->player_is_jumping = true;
    }
    else
    {
        world->player_jump_time_remaining = 0.0f;
        world->player_is_jumping = false;
    }

    if (move_x != 0.0f && move_y != 0.0f)
    {
        const float length = sqrtf((move_x * move_x) + (move_y * move_y));
        if (length > 0.0f)
        {
            move_x /= length;
            move_y /= length;
        }
    }

    float active_speed = world->player_is_swimming ? world->player_swim_speed : world->player_speed;
    if (world->player_is_jumping)
    {
        active_speed *= world->player_jump_speed_multiplier;
    }

    const float next_x = world->player_x + (move_x * active_speed * delta_time);
    const float next_y = world->player_y + (move_y * active_speed * delta_time);

    if (world_can_stand_on(world, next_x, world->player_y))
    {
        world->player_x = next_x;
    }

    if (world_can_stand_on(world, world->player_x, next_y))
    {
        world->player_y = next_y;
    }

    float max_x = ((float)(world->width * world->tile_size)) - (float)world->tile_size;
    float max_y = ((float)(world->height * world->tile_size)) - (float)world->tile_size;

    if (max_x < 0.0f)
    {
        max_x = 0.0f;
    }

    if (max_y < 0.0f)
    {
        max_y = 0.0f;
    }

    if (world->player_x < 0.0f)
    {
        world->player_x = 0.0f;
    }

    if (world->player_y < 0.0f)
    {
        world->player_y = 0.0f;
    }

    if (world->player_x > max_x)
    {
        world->player_x = max_x;
    }

    if (world->player_y > max_y)
    {
        world->player_y = max_y;
    }

    bool updated_swimming = false;
    world_can_occupy_tile(world,
                          (int)(world->player_x / (float)world->tile_size),
                          (int)(world->player_y / (float)world->tile_size),
                          &updated_swimming,
                          NULL,
                          NULL);
    world->player_is_swimming = updated_swimming;
}

void world_render(World *world, SDL_Renderer *renderer)
{
    if (world == NULL || renderer == NULL)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);

    for (int y = 0; y < world->height; y++)
    {
        for (int x = 0; x < world->width; x++)
        {
            SDL_FRect tile_rect;
            tile_rect.x = (float)(x * world->tile_size);
            tile_rect.y = (float)(y * world->tile_size);
            tile_rect.w = (float)world->tile_size;
            tile_rect.h = (float)world->tile_size;

            const TileDefinition *ground_tile = world_get_tile_at_layer(world, x, y, TILE_LAYER_GROUND);
            const TileDefinition *floor_tile = world_get_tile_at_layer(world, x, y, TILE_LAYER_FLOOR);
            const TileDefinition *object_tile = world_get_tile_at_layer(world, x, y, TILE_LAYER_OBJECT);
            const TileDefinition *structure_tile =
                world_get_tile_at_layer(world, x, y, TILE_LAYER_STRUCTURE);

            const TileDefinition *layers[] = {
                ground_tile,
                floor_tile,
                object_tile,
                structure_tile,
            };
            const TileLayer layer_ids[] = {
                TILE_LAYER_GROUND,
                TILE_LAYER_FLOOR,
                TILE_LAYER_OBJECT,
                TILE_LAYER_STRUCTURE,
            };

            bool rendered_layer = false;
            for (int layer_index = 0; layer_index < 4; layer_index++)
            {
                const TileDefinition *tile = layers[layer_index];
                if (tile == NULL)
                {
                    continue;
                }

                const SDL_Color fill_color = world_tile_color(tile);
                const SDL_FRect layer_rect = world_layer_rect(world, x, y, layer_ids[layer_index]);
                SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b,
                                       fill_color.a);
                SDL_RenderFillRect(renderer, &layer_rect);
                rendered_layer = true;
            }

            if (!rendered_layer)
            {
                const SDL_Color fill_color = world_tile_color(NULL);
                SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b,
                                       fill_color.a);
                SDL_RenderFillRect(renderer, &tile_rect);
            }

            SDL_SetRenderDrawColor(renderer, 55, 55, 55, 255);
            SDL_RenderRect(renderer, &tile_rect);
        }
    }

    SDL_FRect player_rect;
    player_rect.x = world->player_x;
    player_rect.y = world->player_y;
    player_rect.w = (float)world->tile_size;
    player_rect.h = (float)world->tile_size;

    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    SDL_RenderFillRect(renderer, &player_rect);
}

bool world_get_player_environment(const World *world,
                                  int *tile_x,
                                  int *tile_y,
                                  const TileDefinition **tile,
                                  const BiomeDefinition **biome,
                                  float *temperature_c)
{
    if (world == NULL || world->ground_tiles == NULL || world->biomes == NULL ||
        world->temperatures_c == NULL || world->tile_size <= 0 ||
        world->width <= 0 || world->height <= 0)
    {
        return false;
    }

    const int x = (int)(world->player_x / (float)world->tile_size);
    const int y = (int)(world->player_y / (float)world->tile_size);
    if (x < 0 || y < 0 || x >= world->width || y >= world->height)
    {
        return false;
    }

    const int index = world_index_from_xy(world, x, y);

    if (tile_x)
    {
        *tile_x = x;
    }
    if (tile_y)
    {
        *tile_y = y;
    }
    if (tile)
    {
        *tile = world_get_supporting_tile_definition_at(world, world->player_x, world->player_y);
    }
    if (biome)
    {
        *biome = biome_get_definition(world->biomes[index]);
    }
    if (temperature_c)
    {
        *temperature_c = world->temperatures_c[index];
    }

    return true;
}

void world_shutdown(World *world)
{
    if (world == NULL)
    {
        return;
    }

    free(world->ground_tiles);
    world->ground_tiles = NULL;
    free(world->floor_tiles);
    world->floor_tiles = NULL;
    free(world->object_tiles);
    world->object_tiles = NULL;
    free(world->structure_tiles);
    world->structure_tiles = NULL;
    free(world->biomes);
    world->biomes = NULL;
    free(world->temperatures_c);
    world->temperatures_c = NULL;

    world->width = 0;
    world->height = 0;
    world->tile_size = 0;
    world->player_x = 0.0f;
    world->player_y = 0.0f;
    world->player_speed = 0.0f;
    world->player_swim_speed = 0.0f;
    world->player_jump_duration = 0.0f;
    world->player_jump_time_remaining = 0.0f;
    world->player_jump_speed_multiplier = 0.0f;
    world->player_is_swimming = false;
    world->player_is_jumping = false;
}
