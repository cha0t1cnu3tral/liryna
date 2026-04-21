#ifndef WORLD_H
#define WORLD_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "tiles.h"
#include "biome_tile_sets.h"

typedef struct World
{
    int width;
    int height;
    int tile_size;
    TileId *tiles;
    BiomeType *biomes;
    float *temperatures_c;

    float player_x;
    float player_y;
    float player_speed;
} World;

bool world_init(World *world, int width, int height, int tile_size);
void world_update(World *world, float delta_time, float move_x, float move_y);
void world_render(World *world, SDL_Renderer *renderer);
bool world_get_player_environment(const World *world,
                                  int *tile_x,
                                  int *tile_y,
                                  const TileDefinition **tile,
                                  const BiomeDefinition **biome,
                                  float *temperature_c);
void world_shutdown(World *world);

#endif
