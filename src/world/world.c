#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "world_generation.h"

static int world_index_from_xy(const World *world, int x, int y)
{
    return (y * world->width) + x;
}

static bool world_can_stand_on(const World *world, float x, float y)
{
    if (world == NULL || world->tiles == NULL || world->tile_size <= 0)
    {
        return false;
    }

    const int tile_x = (int)(x / world->tile_size);
    const int tile_y = (int)(y / world->tile_size);

    if (tile_x < 0 || tile_y < 0 || tile_x >= world->width || tile_y >= world->height)
    {
        return false;
    }

    const int index = world_index_from_xy(world, tile_x, tile_y);
    const TileDefinition *tile = tiles_get_definition(world->tiles[index]);

    return tile != NULL && tile->walkable && !tile->blocks_land_movement;
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

    world->player_x = (width * tile_size) / 2.0f;
    world->player_y = (height * tile_size) / 2.0f;
    world->player_speed = 200.0f;

    const int tile_count = width * height;
    world->tiles = (TileId *)malloc((size_t)tile_count * sizeof(TileId));
    if (world->tiles == NULL)
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

void world_update(World *world, float delta_time, float move_x, float move_y)
{
    if (world == NULL)
    {
        return;
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

    const float next_x = world->player_x + (move_x * world->player_speed * delta_time);
    const float next_y = world->player_y + (move_y * world->player_speed * delta_time);

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

void world_shutdown(World *world)
{
    if (world == NULL)
    {
        return;
    }

    free(world->tiles);
    world->tiles = NULL;

    world->width = 0;
    world->height = 0;
    world->tile_size = 0;
    world->player_x = 0.0f;
    world->player_y = 0.0f;
    world->player_speed = 0.0f;
}
