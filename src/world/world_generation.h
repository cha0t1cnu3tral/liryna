#ifndef WORLD_GENERATION_H
#define WORLD_GENERATION_H

#include <stdbool.h>

#include "world.h"

bool world_generate_procedural(World *world, unsigned int seed);
bool world_find_spawn_tile(const World *world, int *out_x, int *out_y);

#endif
