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

void world_update(World *world, float delta_time, const Uint8 *keyboard_state)
{
    if (world == NULL || keyboard_state == NULL)
    {
        return;
    }

    float move_x = 0.0f;
    float move_y = 0.0f;

    if (keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_UP])
    {
        move_y -= 1.0f;
    }

    if (keyboard_state[SDL_SCANCODE_S] || keyboard_state[SDL_SCANCODE_DOWN])
    {
        move_y += 1.0f;
    }

    if (keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_LEFT])
    {
        move_x -= 1.0f;
    }

    if (keyboard_state[SDL_SCANCODE_D] || keyboard_state[SDL_SCANCODE_RIGHT])
    {
        move_x += 1.0f;
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
            SDL_Rect tile_rect;
            tile_rect.x = x * world->tile_size;
            tile_rect.y = y * world->tile_size;
            tile_rect.w = world->tile_size;
            tile_rect.h = world->tile_size;

            SDL_RenderDrawRect(renderer, &tile_rect);
        }
    }

    SDL_Rect player_rect;
    player_rect.x = (int)world->player_x;
    player_rect.y = (int)world->player_y;
    player_rect.w = world->tile_size;
    player_rect.h = world->tile_size;

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
