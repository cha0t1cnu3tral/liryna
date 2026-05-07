#include "structure_generation.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/structure_builder.h"

typedef struct StructureCell
{
    int x;
    int y;
    TileId ground_tile_id;
    TileId floor_tile_id;
    TileId object_tile_id;
    TileId structure_tile_id;
} StructureCell;

typedef struct StructureDefinition
{
    char name[128];
    StructureSaveKind save_kind;
    int width;
    int height;
    bool allowed_biomes[BIOME_TYPE_COUNT];
    bool allowed_support_tiles[TILE_ID_COUNT];
    StructureCell *cells;
    int cell_count;
    int cell_capacity;
} StructureDefinition;

static const char *skip_json_whitespace(const char *text)
{
    while (text != NULL && *text != '\0' && isspace((unsigned char)*text))
    {
        text++;
    }

    return text;
}

static const char *find_json_value(const char *json, const char *key)
{
    if (json == NULL || key == NULL)
    {
        return NULL;
    }

    char quoted_key[96];
    snprintf(quoted_key, sizeof(quoted_key), "\"%s\"", key);

    const char *match = strstr(json, quoted_key);
    if (match == NULL)
    {
        return NULL;
    }

    match += strlen(quoted_key);
    match = skip_json_whitespace(match);
    if (match == NULL || *match != ':')
    {
        return NULL;
    }

    return skip_json_whitespace(match + 1);
}

static bool parse_json_string_value(const char *value,
                                    char *out,
                                    size_t out_size,
                                    const char **out_after)
{
    if (value == NULL || out == NULL || out_size == 0 || *value != '"')
    {
        return false;
    }

    value++;
    size_t out_index = 0;

    while (*value != '\0' && *value != '"')
    {
        char next = *value;
        if (next == '\\')
        {
            value++;
            switch (*value)
            {
            case '"':
            case '\\':
            case '/':
                next = *value;
                break;
            case 'n':
                next = '\n';
                break;
            case 'r':
                next = '\r';
                break;
            case 't':
                next = '\t';
                break;
            default:
                return false;
            }
        }

        if (out_index + 1 >= out_size)
        {
            return false;
        }

        out[out_index++] = next;
        value++;
    }

    if (*value != '"')
    {
        return false;
    }

    out[out_index] = '\0';
    if (out_after != NULL)
    {
        *out_after = value + 1;
    }
    return true;
}

static bool parse_json_int_value(const char *value, int *out, const char **out_after)
{
    if (value == NULL || out == NULL)
    {
        return false;
    }

    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (end == value)
    {
        return false;
    }

    *out = (int)parsed;
    if (out_after != NULL)
    {
        *out_after = end;
    }
    return true;
}

static bool parse_json_save_kind(const char *value, StructureSaveKind *out_kind)
{
    if (value == NULL || out_kind == NULL)
    {
        return false;
    }

    char kind_name[32];
    if (!parse_json_string_value(value, kind_name, sizeof(kind_name), NULL))
    {
        return false;
    }

    if (strcmp(kind_name, "village") == 0)
    {
        *out_kind = STRUCTURE_SAVE_KIND_VILLAGE;
        return true;
    }

    if (strcmp(kind_name, "structure") == 0)
    {
        *out_kind = STRUCTURE_SAVE_KIND_STRUCTURE;
        return true;
    }

    return false;
}

static bool parse_json_null_value(const char *value, const char **out_after)
{
    if (value == NULL || strncmp(value, "null", 4) != 0)
    {
        return false;
    }

    if (out_after != NULL)
    {
        *out_after = value + 4;
    }
    return true;
}

static bool parse_json_object_key(const char *value, const char *expected_key, const char **out_after)
{
    if (value == NULL || expected_key == NULL)
    {
        return false;
    }

    char key_buffer[64];
    const char *after_string = NULL;
    if (!parse_json_string_value(skip_json_whitespace(value), key_buffer, sizeof(key_buffer),
                                 &after_string))
    {
        return false;
    }

    if (strcmp(key_buffer, expected_key) != 0)
    {
        return false;
    }

    after_string = skip_json_whitespace(after_string);
    if (after_string == NULL || *after_string != ':')
    {
        return false;
    }

    if (out_after != NULL)
    {
        *out_after = skip_json_whitespace(after_string + 1);
    }
    return true;
}

static BiomeType biome_find_by_name(const char *name)
{
    if (name == NULL)
    {
        return BIOME_TYPE_COUNT;
    }

    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        const BiomeDefinition *biome = biome_get_definition((BiomeType)i);
        if (biome != NULL && biome->name != NULL && strcmp(biome->name, name) == 0)
        {
            return (BiomeType)i;
        }
    }

    return BIOME_TYPE_COUNT;
}

static bool parse_json_tile_name_or_null(const char *value, TileId *out_tile_id, const char **out_after)
{
    if (value == NULL || out_tile_id == NULL)
    {
        return false;
    }

    value = skip_json_whitespace(value);
    if (*value == 'n')
    {
        if (!parse_json_null_value(value, out_after))
        {
            return false;
        }

        *out_tile_id = TILE_ID_COUNT;
        return true;
    }

    char tile_name[128];
    const char *after_string = NULL;
    if (!parse_json_string_value(value, tile_name, sizeof(tile_name), &after_string))
    {
        return false;
    }

    const TileId tile_id = tiles_find_by_name(tile_name);
    if (tile_id == TILE_ID_COUNT)
    {
        return false;
    }

    *out_tile_id = tile_id;
    if (out_after != NULL)
    {
        *out_after = after_string;
    }
    return true;
}

static void structure_definition_reset(StructureDefinition *structure)
{
    if (structure == NULL)
    {
        return;
    }

    memset(structure, 0, sizeof(*structure));
    structure->save_kind = STRUCTURE_SAVE_KIND_STRUCTURE;
    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        structure->allowed_support_tiles[i] = false;
    }
}

static void structure_definition_free(StructureDefinition *structure)
{
    if (structure == NULL)
    {
        return;
    }

    free(structure->cells);
    structure->cells = NULL;
    structure->cell_count = 0;
    structure->cell_capacity = 0;
}

static bool structure_definition_append_cell(StructureDefinition *structure, const StructureCell *cell)
{
    if (structure == NULL || cell == NULL)
    {
        return false;
    }

    if (structure->cell_count >= structure->cell_capacity)
    {
        const int next_capacity = structure->cell_capacity > 0 ? structure->cell_capacity * 2 : 32;
        StructureCell *next_cells =
            (StructureCell *)realloc(structure->cells, (size_t)next_capacity * sizeof(*next_cells));
        if (next_cells == NULL)
        {
            return false;
        }

        structure->cells = next_cells;
        structure->cell_capacity = next_capacity;
    }

    structure->cells[structure->cell_count++] = *cell;
    return true;
}

static bool parse_json_biome_array(const char *value, bool allowed_biomes[BIOME_TYPE_COUNT])
{
    if (value == NULL || allowed_biomes == NULL)
    {
        return false;
    }

    value = skip_json_whitespace(value);
    if (*value != '[')
    {
        return false;
    }

    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        allowed_biomes[i] = false;
    }

    value = skip_json_whitespace(value + 1);
    while (*value != '\0' && *value != ']')
    {
        char biome_name[64];
        const char *after_string = NULL;
        if (!parse_json_string_value(value, biome_name, sizeof(biome_name), &after_string))
        {
            return false;
        }

        const BiomeType biome_type = biome_find_by_name(biome_name);
        if (biome_type == BIOME_TYPE_COUNT)
        {
            return false;
        }
        allowed_biomes[biome_type] = true;

        value = skip_json_whitespace(after_string);
        if (*value == ',')
        {
            value = skip_json_whitespace(value + 1);
            continue;
        }
        if (*value != ']')
        {
            return false;
        }
    }

    return *value == ']';
}

static bool parse_json_support_tile_array(const char *value, bool allowed_support_tiles[TILE_ID_COUNT])
{
    if (value == NULL || allowed_support_tiles == NULL)
    {
        return false;
    }

    value = skip_json_whitespace(value);
    if (*value != '[')
    {
        return false;
    }

    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        allowed_support_tiles[i] = false;
    }

    value = skip_json_whitespace(value + 1);
    while (*value != '\0' && *value != ']')
    {
        char tile_name[128];
        const char *after_string = NULL;
        if (!parse_json_string_value(value, tile_name, sizeof(tile_name), &after_string))
        {
            return false;
        }

        const TileId tile_id = tiles_find_by_name(tile_name);
        if (tile_id == TILE_ID_COUNT)
        {
            return false;
        }
        allowed_support_tiles[tile_id] = true;

        value = skip_json_whitespace(after_string);
        if (*value == ',')
        {
            value = skip_json_whitespace(value + 1);
            continue;
        }
        if (*value != ']')
        {
            return false;
        }
    }

    return *value == ']';
}

static bool parse_json_size_object(const char *value, int *out_width, int *out_height)
{
    if (value == NULL || out_width == NULL || out_height == NULL)
    {
        return false;
    }

    value = skip_json_whitespace(value);
    if (*value != '{')
    {
        return false;
    }

    const char *cursor = skip_json_whitespace(value + 1);
    if (!parse_json_object_key(cursor, "width", &cursor) ||
        !parse_json_int_value(cursor, out_width, &cursor))
    {
        return false;
    }

    cursor = skip_json_whitespace(cursor);
    if (*cursor != ',')
    {
        return false;
    }
    cursor = skip_json_whitespace(cursor + 1);

    if (!parse_json_object_key(cursor, "height", &cursor) ||
        !parse_json_int_value(cursor, out_height, &cursor))
    {
        return false;
    }

    cursor = skip_json_whitespace(cursor);
    return *cursor == '}';
}

static bool parse_json_cells_array(const char *value, StructureDefinition *structure)
{
    if (value == NULL || structure == NULL)
    {
        return false;
    }

    value = skip_json_whitespace(value);
    if (*value != '[')
    {
        return false;
    }

    const char *cursor = skip_json_whitespace(value + 1);
    while (*cursor != '\0' && *cursor != ']')
    {
        if (*cursor != '{')
        {
            return false;
        }

        StructureCell cell = {
            .x = 0,
            .y = 0,
            .ground_tile_id = TILE_ID_COUNT,
            .floor_tile_id = TILE_ID_COUNT,
            .object_tile_id = TILE_ID_COUNT,
            .structure_tile_id = TILE_ID_COUNT,
        };

        cursor = skip_json_whitespace(cursor + 1);
        if (!parse_json_object_key(cursor, "x", &cursor) ||
            !parse_json_int_value(cursor, &cell.x, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != ',')
        {
            return false;
        }
        cursor = skip_json_whitespace(cursor + 1);

        if (!parse_json_object_key(cursor, "y", &cursor) ||
            !parse_json_int_value(cursor, &cell.y, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != ',')
        {
            return false;
        }
        cursor = skip_json_whitespace(cursor + 1);

        if (!parse_json_object_key(cursor, "ground", &cursor) ||
            !parse_json_tile_name_or_null(cursor, &cell.ground_tile_id, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != ',')
        {
            return false;
        }
        cursor = skip_json_whitespace(cursor + 1);

        if (!parse_json_object_key(cursor, "floor", &cursor) ||
            !parse_json_tile_name_or_null(cursor, &cell.floor_tile_id, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != ',')
        {
            return false;
        }
        cursor = skip_json_whitespace(cursor + 1);

        if (!parse_json_object_key(cursor, "object", &cursor) ||
            !parse_json_tile_name_or_null(cursor, &cell.object_tile_id, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != ',')
        {
            return false;
        }
        cursor = skip_json_whitespace(cursor + 1);

        if (!parse_json_object_key(cursor, "structure", &cursor) ||
            !parse_json_tile_name_or_null(cursor, &cell.structure_tile_id, &cursor))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor);
        if (*cursor != '}')
        {
            return false;
        }

        if (!structure_definition_append_cell(structure, &cell))
        {
            return false;
        }

        cursor = skip_json_whitespace(cursor + 1);
        if (*cursor == ',')
        {
            cursor = skip_json_whitespace(cursor + 1);
            continue;
        }
        if (*cursor != ']')
        {
            return false;
        }
    }

    return *cursor == ']';
}

static bool load_structure_definition(const char *path, StructureDefinition *out_structure)
{
    if (path == NULL || out_structure == NULL)
    {
        return false;
    }

    structure_definition_reset(out_structure);

    size_t data_size = 0;
    char *data = (char *)SDL_LoadFile(path, &data_size);
    if (data == NULL)
    {
        return false;
    }

    char *json = (char *)malloc(data_size + 1);
    if (json == NULL)
    {
        SDL_free(data);
        return false;
    }

    memcpy(json, data, data_size);
    json[data_size] = '\0';
    SDL_free(data);

    bool loaded = false;
    const char *value = find_json_value(json, "name");
    if (value == NULL ||
        !parse_json_string_value(value, out_structure->name, sizeof(out_structure->name), NULL))
    {
        goto done;
    }

    value = find_json_value(json, "kind");
    if (value != NULL && !parse_json_save_kind(value, &out_structure->save_kind))
    {
        goto done;
    }

    value = find_json_value(json, "size");
    if (value == NULL ||
        !parse_json_size_object(value, &out_structure->width, &out_structure->height))
    {
        goto done;
    }

    value = find_json_value(json, "allowed_biomes");
    if (value == NULL || !parse_json_biome_array(value, out_structure->allowed_biomes))
    {
        goto done;
    }

    value = find_json_value(json, "allowed_support_tiles");
    if (value == NULL ||
        !parse_json_support_tile_array(value, out_structure->allowed_support_tiles))
    {
        goto done;
    }

    value = find_json_value(json, "cells");
    if (value == NULL || !parse_json_cells_array(value, out_structure))
    {
        goto done;
    }

    if (out_structure->width <= 0 || out_structure->height <= 0 || out_structure->cell_count <= 0)
    {
        goto done;
    }

    loaded = true;

done:
    free(json);
    if (!loaded)
    {
        structure_definition_free(out_structure);
    }
    return loaded;
}

static unsigned int structure_hash_u32(unsigned int value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static unsigned int structure_name_seed(const char *name)
{
    unsigned int value = 2166136261U;
    for (const unsigned char *cursor = (const unsigned char *)(name != NULL ? name : "");
         *cursor != '\0';
         cursor++)
    {
        value ^= (unsigned int)(*cursor);
        value *= 16777619U;
    }

    return structure_hash_u32(value);
}

static int structure_footprint_area(const StructureDefinition *structure)
{
    if (structure == NULL || structure->width <= 0 || structure->height <= 0)
    {
        return 0;
    }

    return structure->width * structure->height;
}

static bool structure_has_allowed_biomes(const StructureDefinition *structure)
{
    if (structure == NULL)
    {
        return false;
    }

    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        if (structure->allowed_biomes[i])
        {
            return true;
        }
    }

    return false;
}

static bool structure_has_allowed_support_tiles(const StructureDefinition *structure)
{
    if (structure == NULL)
    {
        return false;
    }

    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        if (structure->allowed_support_tiles[i])
        {
            return true;
        }
    }

    return false;
}

static int desired_structure_placements(const World *world, const StructureDefinition *structure)
{
    if (world == NULL || structure == NULL || !structure_has_allowed_biomes(structure) ||
        !structure_has_allowed_support_tiles(structure))
    {
        return 0;
    }

    const int footprint_area = structure_footprint_area(structure);
    if (footprint_area <= 0)
    {
        return 0;
    }

    if (structure->save_kind == STRUCTURE_SAVE_KIND_VILLAGE)
    {
        int desired = (world->width * world->height) / (footprint_area * 4200);
        if (desired < 1)
        {
            desired = 1;
        }
        if (desired > 2)
        {
            desired = 2;
        }
        return desired;
    }

    int desired = (world->width * world->height) / (footprint_area * 1800);
    if (desired < 1)
    {
        desired = 1;
    }
    if (desired > 6)
    {
        desired = 6;
    }

    return desired;
}

static bool structure_can_place_at(const World *world,
                                   const StructureDefinition *structure,
                                   int origin_x,
                                   int origin_y)
{
    if (world == NULL || structure == NULL)
    {
        return false;
    }

    for (int i = 0; i < structure->cell_count; i++)
    {
        const StructureCell *cell = &structure->cells[i];
        const int world_x = origin_x + cell->x;
        const int world_y = origin_y + cell->y;
        if (!world_is_in_bounds(world, world_x, world_y))
        {
            return false;
        }

        const int index = (world_y * world->width) + world_x;
        const BiomeType biome_type = world->biomes[index];
        if (biome_type < 0 || biome_type >= BIOME_TYPE_COUNT ||
            !structure->allowed_biomes[biome_type])
        {
            return false;
        }

        const TileDefinition *support_tile = world_get_supporting_tile_at(world, world_x, world_y);
        if (support_tile == NULL || support_tile->id < 0 || support_tile->id >= TILE_ID_COUNT ||
            !structure->allowed_support_tiles[support_tile->id])
        {
            return false;
        }
    }

    return true;
}

static void structure_apply_at(World *world, const StructureDefinition *structure, int origin_x, int origin_y)
{
    if (world == NULL || structure == NULL)
    {
        return;
    }

    for (int i = 0; i < structure->cell_count; i++)
    {
        const StructureCell *cell = &structure->cells[i];
        const int world_x = origin_x + cell->x;
        const int world_y = origin_y + cell->y;

        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_GROUND, cell->ground_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_FLOOR, cell->floor_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_OBJECT, cell->object_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_STRUCTURE,
                                cell->structure_tile_id);
    }
}

static int compare_structure_paths(const void *left, const void *right)
{
    const char *const *left_path = (const char *const *)left;
    const char *const *right_path = (const char *const *)right;
    return strcmp(*left_path, *right_path);
}

bool world_generation_apply_saved_structures(World *world, unsigned int seed)
{
    if (world == NULL || world->width <= 0 || world->height <= 0 || world->biomes == NULL)
    {
        return false;
    }

    SDL_PathInfo directory_info;
    if (!SDL_GetPathInfo("saves/structures", &directory_info) ||
        directory_info.type != SDL_PATHTYPE_DIRECTORY)
    {
        return true;
    }

    int entry_count = 0;
    char **entries = SDL_GlobDirectory("saves/structures", "*.json", SDL_GLOB_CASEINSENSITIVE,
                                       &entry_count);
    if (entries == NULL)
    {
        fprintf(stderr, "world_generation: failed to enumerate saves/structures: %s\n",
                SDL_GetError());
        return true;
    }

    if (entry_count > 1)
    {
        qsort(entries, (size_t)entry_count, sizeof(entries[0]), compare_structure_paths);
    }

    for (int entry_index = 0; entry_index < entry_count; entry_index++)
    {
        char path[512];
        snprintf(path, sizeof(path), "saves/structures/%s", entries[entry_index]);

        StructureDefinition structure;
        if (!load_structure_definition(path, &structure))
        {
            fprintf(stderr, "world_generation: failed to load structure %s\n", path);
            continue;
        }

        if (structure.width > world->width || structure.height > world->height)
        {
            structure_definition_free(&structure);
            continue;
        }

        const int desired_placements = desired_structure_placements(world, &structure);
        const int max_origin_x = world->width - structure.width;
        const int max_origin_y = world->height - structure.height;
        const unsigned int name_seed = structure_name_seed(structure.name);

        int placed_count = 0;
        const int attempt_count = desired_placements * 96;
        for (int attempt = 0; attempt < attempt_count && placed_count < desired_placements; attempt++)
        {
            const unsigned int attempt_seed =
                structure_hash_u32(seed ^ name_seed ^ ((unsigned int)attempt * 4099U));
            const int origin_x = max_origin_x > 0
                                     ? (int)(attempt_seed % (unsigned int)(max_origin_x + 1))
                                     : 0;
            const int origin_y = max_origin_y > 0
                                     ? (int)(structure_hash_u32(attempt_seed ^ 0x9e3779b9U) %
                                             (unsigned int)(max_origin_y + 1))
                                     : 0;

            if (!structure_can_place_at(world, &structure, origin_x, origin_y))
            {
                continue;
            }

            structure_apply_at(world, &structure, origin_x, origin_y);
            placed_count++;
        }

        structure_definition_free(&structure);
    }

    SDL_free(entries);
    return true;
}
