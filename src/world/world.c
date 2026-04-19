#include "world.h"

#include <math.h>

bool world_init(World *world, int width, int height, int tile_size)
{
    if (world == NULL || width <= 0 || height <= 0 || tile_size <= 0)
    {
        return false;
    }

    world->width = width;
    world->height = height;
    world->tile_size = tile_size;

    world->player_x = (width * tile_size) / 2.0f;
    world->player_y = (height * tile_size) / 2.0f;
    world->player_speed = 200.0f;

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

    world->player_x += move_x * world->player_speed * delta_time;
    world->player_y += move_y * world->player_speed * delta_time;

    float max_x = (world->width * world->tile_size) - world->tile_size;
    float max_y = (world->height * world->tile_size) - world->tile_size;

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

    world->width = 0;
    world->height = 0;
    world->tile_size = 0;
    world->player_x = 0.0f;
    world->player_y = 0.0f;
    world->player_speed = 0.0f;
}
