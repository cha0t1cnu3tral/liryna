#include "structure_builder.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <SDL3/SDL.h>

typedef struct StructureBounds
{
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    bool has_tiles;
} StructureBounds;

static bool structure_builder_append_text(char **buffer,
                                          size_t *length,
                                          size_t *capacity,
                                          const char *text)
{
    if (buffer == NULL || length == NULL || capacity == NULL || text == NULL)
    {
        return false;
    }

    const size_t text_length = strlen(text);
    if (*length + text_length + 1 > *capacity)
    {
        size_t next_capacity = *capacity > 0 ? *capacity : 1024;
        while (*length + text_length + 1 > next_capacity)
        {
            next_capacity *= 2;
        }

        char *next_buffer = realloc(*buffer, next_capacity);
        if (next_buffer == NULL)
        {
            return false;
        }

        *buffer = next_buffer;
        *capacity = next_capacity;
    }

    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
    return true;
}

static bool structure_builder_append_format(char **buffer,
                                            size_t *length,
                                            size_t *capacity,
                                            const char *format,
                                            ...)
{
    if (buffer == NULL || length == NULL || capacity == NULL || format == NULL)
    {
        return false;
    }

    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    const int required = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (required < 0)
    {
        va_end(args);
        return false;
    }

    const size_t required_size = (size_t)required;
    if (*length + required_size + 1 > *capacity)
    {
        size_t next_capacity = *capacity > 0 ? *capacity : 1024;
        while (*length + required_size + 1 > next_capacity)
        {
            next_capacity *= 2;
        }

        char *next_buffer = realloc(*buffer, next_capacity);
        if (next_buffer == NULL)
        {
            va_end(args);
            return false;
        }

        *buffer = next_buffer;
        *capacity = next_capacity;
    }

    vsnprintf(*buffer + *length, *capacity - *length, format, args);
    va_end(args);
    *length += required_size;
    return true;
}

static bool structure_builder_append_json_string(char **buffer,
                                                 size_t *length,
                                                 size_t *capacity,
                                                 const char *text)
{
    if (!structure_builder_append_text(buffer, length, capacity, "\""))
    {
        return false;
    }

    for (const char *cursor = text != NULL ? text : ""; *cursor != '\0'; cursor++)
    {
        char escaped[8];
        switch (*cursor)
        {
        case '\\':
            if (!structure_builder_append_text(buffer, length, capacity, "\\\\"))
            {
                return false;
            }
            break;
        case '"':
            if (!structure_builder_append_text(buffer, length, capacity, "\\\""))
            {
                return false;
            }
            break;
        case '\n':
            if (!structure_builder_append_text(buffer, length, capacity, "\\n"))
            {
                return false;
            }
            break;
        case '\r':
            if (!structure_builder_append_text(buffer, length, capacity, "\\r"))
            {
                return false;
            }
            break;
        case '\t':
            if (!structure_builder_append_text(buffer, length, capacity, "\\t"))
            {
                return false;
            }
            break;
        default:
            if ((unsigned char)(*cursor) < 0x20U)
            {
                snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned char)(*cursor));
                if (!structure_builder_append_text(buffer, length, capacity, escaped))
                {
                    return false;
                }
            }
            else
            {
                char character[2] = {*cursor, '\0'};
                if (!structure_builder_append_text(buffer, length, capacity, character))
                {
                    return false;
                }
            }
            break;
        }
    }

    return structure_builder_append_text(buffer, length, capacity, "\"");
}

static bool structure_builder_tile_matches_default(const World *world, int x, int y, TileLayer layer)
{
    if (world == NULL)
    {
        return true;
    }

    const TileId tile_id = world_get_tile_id_at_layer(world, x, y, layer);
    switch (layer)
    {
    case TILE_LAYER_GROUND:
        return tile_id == TILE_GRASS;
    case TILE_LAYER_FLOOR:
    case TILE_LAYER_OBJECT:
    case TILE_LAYER_STRUCTURE:
        return tile_id == TILE_ID_COUNT;
    case TILE_LAYER_UNKNOWN:
    case TILE_LAYER_COUNT:
    default:
        return true;
    }
}

static StructureBounds structure_builder_find_bounds(const World *world)
{
    StructureBounds bounds = {
        .min_x = 0,
        .min_y = 0,
        .max_x = 0,
        .max_y = 0,
        .has_tiles = false,
    };

    if (world == NULL)
    {
        return bounds;
    }

    for (int y = 0; y < world->height; y++)
    {
        for (int x = 0; x < world->width; x++)
        {
            const bool changed =
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_GROUND) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_FLOOR) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_OBJECT) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_STRUCTURE);
            if (!changed)
            {
                continue;
            }

            if (!bounds.has_tiles)
            {
                bounds.min_x = x;
                bounds.max_x = x;
                bounds.min_y = y;
                bounds.max_y = y;
                bounds.has_tiles = true;
                continue;
            }

            if (x < bounds.min_x)
            {
                bounds.min_x = x;
            }
            if (x > bounds.max_x)
            {
                bounds.max_x = x;
            }
            if (y < bounds.min_y)
            {
                bounds.min_y = y;
            }
            if (y > bounds.max_y)
            {
                bounds.max_y = y;
            }
        }
    }

    return bounds;
}

static void structure_builder_sanitize_name(const char *name, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return;
    }

    size_t written = 0;
    for (const char *cursor = name != NULL ? name : ""; *cursor != '\0' && written + 1 < out_size;
         cursor++)
    {
        const unsigned char ch = (unsigned char)(*cursor);
        if (isalnum(ch))
        {
            out[written++] = (char)tolower(ch);
        }
        else if ((ch == ' ' || ch == '-' || ch == '_') && written > 0 &&
                 out[written - 1] != '_')
        {
            out[written++] = '_';
        }
    }

    while (written > 0 && out[written - 1] == '_')
    {
        written--;
    }

    if (written == 0)
    {
        snprintf(out, out_size, "structure");
        return;
    }

    out[written] = '\0';
}

TileId structure_builder_biome_primary_tile(BiomeType biome_type)
{
    const BiomeDefinition *biome = biome_get_definition(biome_type);
    if (biome != NULL)
    {
        for (size_t i = 0; i < biome->tile_count; i++)
        {
            const TileId tile_id = biome->tiles[i];
            const TileDefinition *tile = tiles_get_definition(tile_id);
            if (tile != NULL && tile->layer == TILE_LAYER_GROUND &&
                tile->buildable_on && !tile->is_liquid)
            {
                return tile_id;
            }
        }
    }

    return TILE_GRASS;
}

void structure_builder_set_allowed_supports_for_biome(StructureBuilderConfig *config,
                                                      BiomeType biome_type)
{
    if (config == NULL)
    {
        return;
    }

    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        config->allowed_support_tiles[tile_id] = false;
    }

    const BiomeDefinition *biome = biome_get_definition(biome_type);
    if (biome != NULL)
    {
        for (size_t i = 0; i < biome->tile_count; i++)
        {
            const TileId tile_id = biome->tiles[i];
            if (structure_builder_is_support_tile(tile_id))
            {
                config->allowed_support_tiles[tile_id] = true;
            }
        }
    }

    const TileId primary_tile = structure_builder_biome_primary_tile(biome_type);
    if (primary_tile >= 0 && primary_tile < TILE_ID_COUNT)
    {
        config->allowed_support_tiles[primary_tile] = true;
    }
}

const char *structure_builder_save_kind_name(StructureSaveKind save_kind)
{
    switch (save_kind)
    {
    case STRUCTURE_SAVE_KIND_VILLAGE:
        return "village";
    case STRUCTURE_SAVE_KIND_STRUCTURE:
    case STRUCTURE_SAVE_KIND_COUNT:
    default:
        return "structure";
    }
}

void structure_builder_config_reset(StructureBuilderConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    config->name[0] = '\0';
    config->builder_biome = BIOME_PLAINS;
    config->save_kind = STRUCTURE_SAVE_KIND_STRUCTURE;

    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        config->allowed_biomes[biome_index] = true;
    }

    structure_builder_set_allowed_supports_for_biome(config, config->builder_biome);
}

bool structure_builder_is_support_tile(TileId tile_id)
{
    const TileDefinition *tile = tiles_get_definition(tile_id);
    if (tile == NULL)
    {
        return false;
    }

    if (tile->layer != TILE_LAYER_GROUND && tile->layer != TILE_LAYER_FLOOR)
    {
        return false;
    }

    return tile->buildable_on && !tile->is_liquid;
}

bool structure_builder_save(const World *world,
                            const StructureBuilderConfig *config,
                            char *out_path,
                            size_t out_path_size,
                            char *out_error,
                            size_t out_error_size)
{
    if (out_path != NULL && out_path_size > 0)
    {
        out_path[0] = '\0';
    }
    if (out_error != NULL && out_error_size > 0)
    {
        out_error[0] = '\0';
    }

    if (world == NULL || config == NULL)
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Missing builder state.");
        }
        return false;
    }

    if (config->name[0] == '\0')
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Structure name is required.");
        }
        return false;
    }

    const StructureBounds bounds = structure_builder_find_bounds(world);
    if (!bounds.has_tiles)
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Nothing has been built yet.");
        }
        return false;
    }

    if (!SDL_CreateDirectory("saves/structures"))
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Could not create saves/structures.");
        }
        return false;
    }

    char slug[STRUCTURE_BUILDER_NAME_CAPACITY];
    structure_builder_sanitize_name(config->name, slug, sizeof(slug));

    char path[256];
    snprintf(path, sizeof(path), "saves/structures/%s.json", slug);

    char *json = NULL;
    size_t json_length = 0;
    size_t json_capacity = 0;

    if (!structure_builder_append_text(&json, &json_length, &json_capacity, "{\n  \"name\": "))
    {
        goto oom;
    }
    if (!structure_builder_append_json_string(&json, &json_length, &json_capacity, config->name) ||
        !structure_builder_append_format(&json, &json_length, &json_capacity,
                                        ",\n  \"kind\": \"%s\",\n  \"size\": { \"width\": %d, \"height\": %d },\n",
                                        structure_builder_save_kind_name(config->save_kind),
                                        bounds.max_x - bounds.min_x + 1,
                                        bounds.max_y - bounds.min_y + 1))
    {
        goto oom;
    }

    if (!structure_builder_append_text(&json, &json_length, &json_capacity,
                                       "  \"rules\": {\n    \"allowed_biomes\": ["))
    {
        goto oom;
    }
    bool first = true;
    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        if (!config->allowed_biomes[biome_index])
        {
            continue;
        }

        const BiomeDefinition *biome = biome_get_definition((BiomeType)biome_index);
        if (biome == NULL)
        {
            continue;
        }

        if (!first && !structure_builder_append_text(&json, &json_length, &json_capacity, ", "))
        {
            goto oom;
        }
        if (!structure_builder_append_json_string(&json, &json_length, &json_capacity, biome->name))
        {
            goto oom;
        }
        first = false;
    }

    if (!structure_builder_append_text(&json, &json_length, &json_capacity,
                                       "],\n    \"allowed_support_tiles\": ["))
    {
        goto oom;
    }
    first = true;
    for (int tile_id = 0; tile_id < TILE_ID_COUNT; tile_id++)
    {
        if (!config->allowed_support_tiles[tile_id] ||
            !structure_builder_is_support_tile((TileId)tile_id))
        {
            continue;
        }

        const TileDefinition *tile = tiles_get_definition((TileId)tile_id);
        if (tile == NULL)
        {
            continue;
        }

        if (!first && !structure_builder_append_text(&json, &json_length, &json_capacity, ", "))
        {
            goto oom;
        }
        if (!structure_builder_append_json_string(&json, &json_length, &json_capacity, tile->name))
        {
            goto oom;
        }
        first = false;
    }

    if (!structure_builder_append_text(&json, &json_length, &json_capacity,
                                       "]\n  },\n  \"cells\": [\n"))
    {
        goto oom;
    }

    first = true;
    for (int y = bounds.min_y; y <= bounds.max_y; y++)
    {
        for (int x = bounds.min_x; x <= bounds.max_x; x++)
        {
            const bool changed =
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_GROUND) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_FLOOR) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_OBJECT) ||
                !structure_builder_tile_matches_default(world, x, y, TILE_LAYER_STRUCTURE);
            if (!changed)
            {
                continue;
            }

            const TileDefinition *ground =
                world_get_tile_at_layer(world, x, y, TILE_LAYER_GROUND);
            const TileDefinition *floor =
                world_get_tile_at_layer(world, x, y, TILE_LAYER_FLOOR);
            const TileDefinition *object =
                world_get_tile_at_layer(world, x, y, TILE_LAYER_OBJECT);
            const TileDefinition *structure =
                world_get_tile_at_layer(world, x, y, TILE_LAYER_STRUCTURE);

            if (!first && !structure_builder_append_text(&json, &json_length, &json_capacity, ",\n"))
            {
                goto oom;
            }
            first = false;

            if (!structure_builder_append_format(&json, &json_length, &json_capacity,
                                                "    { \"x\": %d, \"y\": %d, ",
                                                x - bounds.min_x, y - bounds.min_y))
            {
                goto oom;
            }

            if (!structure_builder_append_text(&json, &json_length, &json_capacity, "\"ground\": "))
            {
                goto oom;
            }
            if (ground != NULL)
            {
                if (!structure_builder_append_json_string(&json, &json_length, &json_capacity,
                                                          ground->name))
                {
                    goto oom;
                }
            }
            else if (!structure_builder_append_text(&json, &json_length, &json_capacity, "null"))
            {
                goto oom;
            }

            if (!structure_builder_append_text(&json, &json_length, &json_capacity, ", \"floor\": "))
            {
                goto oom;
            }
            if (floor != NULL)
            {
                if (!structure_builder_append_json_string(&json, &json_length, &json_capacity,
                                                          floor->name))
                {
                    goto oom;
                }
            }
            else if (!structure_builder_append_text(&json, &json_length, &json_capacity, "null"))
            {
                goto oom;
            }

            if (!structure_builder_append_text(&json, &json_length, &json_capacity, ", \"object\": "))
            {
                goto oom;
            }
            if (object != NULL)
            {
                if (!structure_builder_append_json_string(&json, &json_length, &json_capacity,
                                                          object->name))
                {
                    goto oom;
                }
            }
            else if (!structure_builder_append_text(&json, &json_length, &json_capacity, "null"))
            {
                goto oom;
            }

            if (!structure_builder_append_text(&json, &json_length, &json_capacity,
                                               ", \"structure\": "))
            {
                goto oom;
            }
            if (structure != NULL)
            {
                if (!structure_builder_append_json_string(&json, &json_length, &json_capacity,
                                                          structure->name))
                {
                    goto oom;
                }
            }
            else if (!structure_builder_append_text(&json, &json_length, &json_capacity, "null"))
            {
                goto oom;
            }

            if (!structure_builder_append_text(&json, &json_length, &json_capacity, " }"))
            {
                goto oom;
            }
        }
    }

    if (!structure_builder_append_text(&json, &json_length, &json_capacity, "\n  ]\n}\n"))
    {
        goto oom;
    }

    if (!SDL_SaveFile(path, json, json_length))
    {
        if (out_error != NULL && out_error_size > 0)
        {
            snprintf(out_error, out_error_size, "Could not write %s.", path);
        }
        free(json);
        return false;
    }

    if (out_path != NULL && out_path_size > 0)
    {
        snprintf(out_path, out_path_size, "%s", path);
    }

    free(json);
    return true;

oom:
    if (out_error != NULL && out_error_size > 0)
    {
        snprintf(out_error, out_error_size, "Out of memory while saving structure.");
    }
    free(json);
    return false;
}
