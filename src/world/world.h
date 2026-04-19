#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct World
{
    int width;
    int height;
    int tile_size;

    float player_x;
    float player_y;
    float player_speed;
} World;

bool world_init(World *world, int width, int height, int tile_size);
void world_update(World *world, float delta_time, float move_x, float move_y);
void world_render(World *world, SDL_Renderer *renderer);
void world_shutdown(World *world);

#endif
