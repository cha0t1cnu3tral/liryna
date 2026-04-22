#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "world_generation.h"

static int world_index_from_xy(const World *world, int x, int y)
{
    return (y * world->width) + x;
}

static const TileDefinition *world_get_tile_definition_at(const World *world, float x, float y)
{
    if (world == NULL || world->tiles == NULL || world->tile_size <= 0)
    {
        return NULL;
    }

    const int tile_x = (int)(x / (float)world->tile_size);
    const int tile_y = (int)(y / (float)world->tile_size);
    if (tile_x < 0 || tile_y < 0 || tile_x >= world->width || tile_y >= world->height)
    {
        return NULL;
    }

    return tiles_get_definition(world->tiles[world_index_from_xy(world, tile_x, tile_y)]);
}

static bool world_can_stand_on(const World *world, float x, float y)
{
    const TileDefinition *tile = world_get_tile_definition_at(world, x, y);
    if (tile == NULL)
    {
        return false;
    }

    const bool can_walk = tile->walkable && !tile->blocks_land_movement;
    const bool can_swim = tile->is_liquid && !tile->blocks_swimming;
    return can_walk || can_swim;
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

bool world_init(World *world, int width, int height, int tile_size)
{
    if (world == NULL || width <= 0 || height <= 0 || tile_size <= 0)
    {
        return false;
    }

    world->width = width;
    world->height = height;
    world->tile_size = tile_size;
    world->tiles = NULL;
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
    world->tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->tiles == NULL)
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

void world_update(World *world, float delta_time, float move_x, float move_y, bool jump_pressed)
{
    if (world == NULL)
    {
        return;
    }

    const TileDefinition *current_tile =
        world_get_tile_definition_at(world, world->player_x, world->player_y);
    const bool can_swim_here =
        current_tile != NULL && current_tile->is_liquid && !current_tile->blocks_swimming;
    world->player_is_swimming = can_swim_here;

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

    const TileDefinition *updated_tile =
        world_get_tile_definition_at(world, world->player_x, world->player_y);
    world->player_is_swimming =
        updated_tile != NULL && updated_tile->is_liquid && !updated_tile->blocks_swimming;
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

            const int index = world_index_from_xy(world, x, y);
            const TileDefinition *tile = tiles_get_definition(world->tiles[index]);
            const SDL_Color fill_color = world_tile_color(tile);

            SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b,
                                   fill_color.a);
            SDL_RenderFillRect(renderer, &tile_rect);

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
    if (world == NULL || world->tiles == NULL || world->biomes == NULL ||
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
        *tile = tiles_get_definition(world->tiles[index]);
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

    free(world->tiles);
    world->tiles = NULL;
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
