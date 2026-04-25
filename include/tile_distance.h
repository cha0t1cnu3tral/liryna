#ifndef LIRYNA_TILE_DISTANCE_H
#define LIRYNA_TILE_DISTANCE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct TileOffset
{
    int east_west;
    int north_south;
} TileOffset;

TileOffset tile_distance_offset(int from_x, int from_y, int to_x, int to_y);
bool tile_distance_format_cardinal(TileOffset offset, char *out, size_t out_size);

#endif
