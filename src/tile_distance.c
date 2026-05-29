#include "tile_distance.h"

#include <stdio.h>

TileOffset tile_distance_offset(int from_x, int from_y, int to_x, int to_y)
{
    TileOffset offset = {
        .east_west = to_x - from_x,
        .north_south = from_y - to_y,
    };
    return offset;
}

bool tile_distance_format_cardinal(TileOffset offset, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return false;
    }

    const int east_west_amount = offset.east_west >= 0 ? offset.east_west : -offset.east_west;
    const int north_south_amount =
        offset.north_south >= 0 ? offset.north_south : -offset.north_south;
    const char *east_west_name = offset.east_west >= 0 ? "east" : "west";
    const char *north_south_name = offset.north_south >= 0 ? "north" : "south";

    int written = 0;
    if (east_west_amount == 0 && north_south_amount == 0)
    {
        written = snprintf(out, out_size, "At current tile.");
    }
    else if (east_west_amount == 0)
    {
        written = snprintf(out, out_size, "%d %s.",
                           north_south_amount, north_south_name);
    }
    else if (north_south_amount == 0)
    {
        written = snprintf(out, out_size, "%d %s.",
                           east_west_amount, east_west_name);
    }
    else
    {
        written = snprintf(out, out_size, "%d %s, %d %s.",
                           east_west_amount, east_west_name,
                           north_south_amount, north_south_name);
    }

    return written > 0 && (size_t)written < out_size;
}
