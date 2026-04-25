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
    TileId *ground_tiles;
    TileId *floor_tiles;
    TileId *object_tiles;
    TileId *structure_tiles;
    BiomeType *biomes;
    float *temperatures_c;

    float player_x;
    float player_y;
    float player_speed;
    float player_swim_speed;
    float player_jump_duration;
    float player_jump_time_remaining;
    float player_jump_speed_multiplier;
    bool player_is_swimming;
    bool player_is_jumping;
} World;

bool world_is_in_bounds(const World *world, int tile_x, int tile_y);
TileId world_get_tile_id_at_layer(const World *world, int tile_x, int tile_y, TileLayer layer);
const TileDefinition *world_get_tile_at_layer(const World *world,
                                              int tile_x,
                                              int tile_y,
                                              TileLayer layer);
const TileDefinition *world_get_top_tile_at(const World *world, int tile_x, int tile_y);
const TileDefinition *world_get_supporting_tile_at(const World *world, int tile_x, int tile_y);
bool world_can_occupy_tile(const World *world,
                           int tile_x,
                           int tile_y,
                           bool *out_swimming,
                           const TileDefinition **out_support_tile,
                           const TileDefinition **out_top_tile);
bool world_set_tile_at_layer(World *world, int tile_x, int tile_y, TileLayer layer, TileId tile_id);
bool world_set_tile(World *world, int tile_x, int tile_y, TileId tile_id);
bool world_clear_tile_at_layer(World *world, int tile_x, int tile_y, TileLayer layer);
bool world_init(World *world, int width, int height, int tile_size);
void world_update(World *world, float delta_time, float move_x, float move_y, bool jump_pressed);
void world_render(World *world, SDL_Renderer *renderer);
bool world_get_player_environment(const World *world,
                                  int *tile_x,
                                  int *tile_y,
                                  const TileDefinition **tile,
                                  const BiomeDefinition **biome,
                                  float *temperature_c);
void world_shutdown(World *world);

#endif
