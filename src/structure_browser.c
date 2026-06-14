#include "structure_browser.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

static void set_error(char *out_error, size_t out_error_size, const char *message)
{
    if (out_error != NULL && out_error_size > 0)
    {
        snprintf(out_error, out_error_size, "%s", message != NULL ? message : "");
    }
}

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

    match = skip_json_whitespace(match + strlen(quoted_key));
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

static bool parse_json_object_key(const char *value, const char *expected_key, const char **out_after)
{
    char key_buffer[64];
    const char *after_string = NULL;
    if (!parse_json_string_value(skip_json_whitespace(value), key_buffer, sizeof(key_buffer),
                                 &after_string) ||
        strcmp(key_buffer, expected_key) != 0)
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

static StructureSaveKind parse_save_kind_or_default(const char *json)
{
    char kind_name[32];
    const char *value = find_json_value(json, "kind");
    if (value != NULL && parse_json_string_value(value, kind_name, sizeof(kind_name), NULL) &&
        strcmp(kind_name, "village") == 0)
    {
        return STRUCTURE_SAVE_KIND_VILLAGE;
    }

    return STRUCTURE_SAVE_KIND_STRUCTURE;
}

static BiomeType biome_find_by_name(const char *name)
{
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

static bool parse_json_biome_array(const char *value, bool allowed_biomes[BIOME_TYPE_COUNT])
{
    if (value == NULL || allowed_biomes == NULL)
    {
        return false;
    }

    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        allowed_biomes[i] = false;
    }

    value = skip_json_whitespace(value);
    if (*value != '[')
    {
        return false;
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

        const BiomeType biome = biome_find_by_name(biome_name);
        if (biome == BIOME_TYPE_COUNT)
        {
            return false;
        }
        allowed_biomes[biome] = true;

        value = skip_json_whitespace(after_string);
        if (*value == ',')
        {
            value = skip_json_whitespace(value + 1);
        }
    }

    return *value == ']';
}

static bool parse_json_support_tile_array(const char *value,
                                          bool allowed_support_tiles[TILE_ID_COUNT])
{
    if (value == NULL || allowed_support_tiles == NULL)
    {
        return false;
    }

    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        allowed_support_tiles[i] = false;
    }

    value = skip_json_whitespace(value);
    if (*value != '[')
    {
        return false;
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
        }
    }

    return *value == ']';
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

typedef struct BrowserCell
{
    int x;
    int y;
    TileId ground_tile_id;
    TileId floor_tile_id;
    TileId object_tile_id;
    TileId structure_tile_id;
} BrowserCell;

static bool parse_json_cell_object(const char *value, BrowserCell *out_cell, const char **out_after)
{
    if (value == NULL || out_cell == NULL)
    {
        return false;
    }

    const char *cursor = skip_json_whitespace(value);
    if (cursor == NULL || *cursor != '{')
    {
        return false;
    }

    *out_cell = (BrowserCell){
        .x = 0,
        .y = 0,
        .ground_tile_id = TILE_ID_COUNT,
        .floor_tile_id = TILE_ID_COUNT,
        .object_tile_id = TILE_ID_COUNT,
        .structure_tile_id = TILE_ID_COUNT,
    };

    cursor = skip_json_whitespace(cursor + 1);
    if (!parse_json_object_key(cursor, "x", &cursor) ||
        !parse_json_int_value(cursor, &out_cell->x, &cursor))
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
        !parse_json_int_value(cursor, &out_cell->y, &cursor))
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
        !parse_json_tile_name_or_null(cursor, &out_cell->ground_tile_id, &cursor))
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
        !parse_json_tile_name_or_null(cursor, &out_cell->floor_tile_id, &cursor))
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
        !parse_json_tile_name_or_null(cursor, &out_cell->object_tile_id, &cursor))
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
        !parse_json_tile_name_or_null(cursor, &out_cell->structure_tile_id, &cursor))
    {
        return false;
    }

    cursor = skip_json_whitespace(cursor);
    if (*cursor != '}')
    {
        return false;
    }

    if (out_after != NULL)
    {
        *out_after = cursor + 1;
    }
    return true;
}

static char *load_text_file(const char *path, size_t *out_size)
{
    size_t data_size = 0;
    char *data = (char *)SDL_LoadFile(path, &data_size);
    if (data == NULL)
    {
        return NULL;
    }

    char *text = (char *)malloc(data_size + 1);
    if (text == NULL)
    {
        SDL_free(data);
        return NULL;
    }

    memcpy(text, data, data_size);
    text[data_size] = '\0';
    SDL_free(data);
    if (out_size != NULL)
    {
        *out_size = data_size;
    }
    return text;
}

static bool load_entry_from_path(const char *path, const char *filename, StructureBrowserEntry *entry)
{
    if (path == NULL || entry == NULL)
    {
        return false;
    }

    char *json = load_text_file(path, NULL);
    if (json == NULL)
    {
        return false;
    }

    memset(entry, 0, sizeof(*entry));
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    snprintf(entry->filename, sizeof(entry->filename), "%s", filename != NULL ? filename : path);

    const char *value = find_json_value(json, "name");
    if (value == NULL ||
        !parse_json_string_value(value, entry->name, sizeof(entry->name), NULL))
    {
        free(json);
        return false;
    }

    entry->save_kind = parse_save_kind_or_default(json);
    value = find_json_value(json, "size");
    if (value == NULL || !parse_json_size_object(value, &entry->width, &entry->height))
    {
        entry->width = 0;
        entry->height = 0;
    }

    free(json);
    return true;
}

static int compare_structure_entries(const void *left, const void *right)
{
    const StructureBrowserEntry *left_entry = (const StructureBrowserEntry *)left;
    const StructureBrowserEntry *right_entry = (const StructureBrowserEntry *)right;
    return strcmp(left_entry->name, right_entry->name);
}

int structure_browser_list(StructureBrowserEntry *entries,
                           int entry_capacity,
                           char *out_error,
                           size_t out_error_size)
{
    if (entries == NULL || entry_capacity <= 0)
    {
        set_error(out_error, out_error_size, "Missing structure browser storage.");
        return 0;
    }

    SDL_PathInfo directory_info;
    if (!SDL_GetPathInfo("saves/structures", &directory_info) ||
        directory_info.type != SDL_PATHTYPE_DIRECTORY)
    {
        set_error(out_error, out_error_size, "No saved structures found.");
        return 0;
    }

    int file_count = 0;
    char **files = SDL_GlobDirectory("saves/structures", "*.json", SDL_GLOB_CASEINSENSITIVE,
                                     &file_count);
    if (files == NULL)
    {
        set_error(out_error, out_error_size, "Could not enumerate saved structures.");
        return 0;
    }

    int entry_count = 0;
    for (int i = 0; i < file_count && entry_count < entry_capacity; i++)
    {
        char path[STRUCTURE_BROWSER_PATH_CAPACITY];
        snprintf(path, sizeof(path), "saves/structures/%s", files[i]);
        if (load_entry_from_path(path, files[i], &entries[entry_count]))
        {
            entry_count++;
        }
    }

    SDL_free(files);
    if (entry_count > 1)
    {
        qsort(entries, (size_t)entry_count, sizeof(entries[0]), compare_structure_entries);
    }

    if (entry_count == 0)
    {
        set_error(out_error, out_error_size, "No readable saved structures found.");
    }
    else
    {
        set_error(out_error, out_error_size, "");
    }
    return entry_count;
}

bool structure_browser_load_settings(const char *path,
                                     StructureBrowserSettings *out_settings,
                                     char *out_error,
                                     size_t out_error_size)
{
    if (path == NULL || out_settings == NULL)
    {
        set_error(out_error, out_error_size, "Missing structure selection.");
        return false;
    }

    char *json = load_text_file(path, NULL);
    if (json == NULL)
    {
        set_error(out_error, out_error_size, "Could not read structure file.");
        return false;
    }

    memset(out_settings, 0, sizeof(*out_settings));
    snprintf(out_settings->path, sizeof(out_settings->path), "%s", path);
    const char *value = find_json_value(json, "name");
    if (value == NULL ||
        !parse_json_string_value(value, out_settings->name, sizeof(out_settings->name), NULL))
    {
        free(json);
        set_error(out_error, out_error_size, "Structure name is missing.");
        return false;
    }

    out_settings->save_kind = parse_save_kind_or_default(json);
    value = find_json_value(json, "size");
    if (value != NULL)
    {
        parse_json_size_object(value, &out_settings->width, &out_settings->height);
    }

    value = find_json_value(json, "allowed_biomes");
    if (value == NULL || !parse_json_biome_array(value, out_settings->allowed_biomes))
    {
        free(json);
        set_error(out_error, out_error_size, "Allowed biome rules are invalid.");
        return false;
    }

    value = find_json_value(json, "allowed_support_tiles");
    if (value == NULL ||
        !parse_json_support_tile_array(value, out_settings->allowed_support_tiles))
    {
        free(json);
        set_error(out_error, out_error_size, "Allowed support tile rules are invalid.");
        return false;
    }

    free(json);
    set_error(out_error, out_error_size, "");
    return true;
}

static bool append_text(char **buffer,
                        size_t *length,
                        size_t *capacity,
                        const char *text)
{
    const size_t text_length = strlen(text);
    if (*length + text_length + 1 > *capacity)
    {
        size_t next_capacity = *capacity > 0 ? *capacity : 2048;
        while (*length + text_length + 1 > next_capacity)
        {
            next_capacity *= 2;
        }

        char *next = (char *)realloc(*buffer, next_capacity);
        if (next == NULL)
        {
            return false;
        }
        *buffer = next;
        *capacity = next_capacity;
    }

    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
    return true;
}

static bool append_format(char **buffer,
                          size_t *length,
                          size_t *capacity,
                          const char *format,
                          ...)
{
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
        size_t next_capacity = *capacity > 0 ? *capacity : 2048;
        while (*length + required_size + 1 > next_capacity)
        {
            next_capacity *= 2;
        }

        char *next = (char *)realloc(*buffer, next_capacity);
        if (next == NULL)
        {
            va_end(args);
            return false;
        }
        *buffer = next;
        *capacity = next_capacity;
    }

    vsnprintf(*buffer + *length, *capacity - *length, format, args);
    va_end(args);
    *length += required_size;
    return true;
}

static bool append_json_string(char **buffer,
                               size_t *length,
                               size_t *capacity,
                               const char *text)
{
    if (!append_text(buffer, length, capacity, "\""))
    {
        return false;
    }

    for (const char *cursor = text != NULL ? text : ""; *cursor != '\0'; cursor++)
    {
        switch (*cursor)
        {
        case '\\':
            if (!append_text(buffer, length, capacity, "\\\\"))
            {
                return false;
            }
            break;
        case '"':
            if (!append_text(buffer, length, capacity, "\\\""))
            {
                return false;
            }
            break;
        case '\n':
            if (!append_text(buffer, length, capacity, "\\n"))
            {
                return false;
            }
            break;
        case '\r':
            if (!append_text(buffer, length, capacity, "\\r"))
            {
                return false;
            }
            break;
        case '\t':
            if (!append_text(buffer, length, capacity, "\\t"))
            {
                return false;
            }
            break;
        default:
        {
            char one[2] = {*cursor, '\0'};
            if (!append_text(buffer, length, capacity, one))
            {
                return false;
            }
            break;
        }
        }
    }

    return append_text(buffer, length, capacity, "\"");
}

static bool append_string_array_item(char **buffer,
                                     size_t *length,
                                     size_t *capacity,
                                     bool *first,
                                     const char *text)
{
    if (!*first && !append_text(buffer, length, capacity, ", "))
    {
        return false;
    }
    *first = false;
    return append_json_string(buffer, length, capacity, text);
}

bool structure_browser_save_settings(const StructureBrowserSettings *settings,
                                     char *out_path,
                                     size_t out_path_size,
                                     char *out_error,
                                     size_t out_error_size)
{
    if (out_path != NULL && out_path_size > 0)
    {
        out_path[0] = '\0';
    }

    if (settings == NULL || settings->path[0] == '\0')
    {
        set_error(out_error, out_error_size, "No structure selected.");
        return false;
    }

    char *original = load_text_file(settings->path, NULL);
    if (original == NULL)
    {
        set_error(out_error, out_error_size, "Could not read selected structure.");
        return false;
    }

    const char *cells_value = find_json_value(original, "cells");
    if (cells_value == NULL)
    {
        free(original);
        set_error(out_error, out_error_size, "Selected structure has no cells array.");
        return false;
    }

    const char *cells_key = strstr(original, "\"cells\"");
    if (cells_key == NULL)
    {
        free(original);
        set_error(out_error, out_error_size, "Selected structure has no cells key.");
        return false;
    }

    char *json = NULL;
    size_t length = 0;
    size_t capacity = 0;
    if (!append_text(&json, &length, &capacity, "{\n  \"name\": ") ||
        !append_json_string(&json, &length, &capacity, settings->name) ||
        !append_format(&json, &length, &capacity,
                       ",\n  \"kind\": \"%s\",\n  \"size\": { \"width\": %d, \"height\": %d },\n",
                       structure_builder_save_kind_name(settings->save_kind),
                       settings->width,
                       settings->height) ||
        !append_text(&json, &length, &capacity, "  \"rules\": {\n    \"allowed_biomes\": ["))
    {
        goto oom;
    }

    bool first = true;
    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        if (!settings->allowed_biomes[i])
        {
            continue;
        }
        const BiomeDefinition *biome = biome_get_definition((BiomeType)i);
        if (biome != NULL && !append_string_array_item(&json, &length, &capacity, &first,
                                                       biome->name))
        {
            goto oom;
        }
    }

    if (!append_text(&json, &length, &capacity,
                     "],\n    \"allowed_support_tiles\": ["))
    {
        goto oom;
    }

    first = true;
    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        if (!settings->allowed_support_tiles[i] || !structure_builder_is_support_tile((TileId)i))
        {
            continue;
        }
        const TileDefinition *tile = tiles_get_definition((TileId)i);
        if (tile != NULL && !append_string_array_item(&json, &length, &capacity, &first,
                                                      tile->name))
        {
            goto oom;
        }
    }

    if (!append_text(&json, &length, &capacity, "]\n  },\n  ") ||
        !append_text(&json, &length, &capacity, cells_key))
    {
        goto oom;
    }

    if (!SDL_SaveFile(settings->path, json, length))
    {
        free(original);
        free(json);
        set_error(out_error, out_error_size, "Could not write structure settings.");
        return false;
    }

    if (out_path != NULL && out_path_size > 0)
    {
        snprintf(out_path, out_path_size, "%s", settings->path);
    }

    free(original);
    free(json);
    set_error(out_error, out_error_size, "");
    return true;

oom:
    free(original);
    free(json);
    set_error(out_error, out_error_size, "Out of memory while saving settings.");
    return false;
}

static BiomeType first_allowed_biome(const bool allowed_biomes[BIOME_TYPE_COUNT])
{
    if (allowed_biomes != NULL)
    {
        for (int i = 0; i < BIOME_TYPE_COUNT; i++)
        {
            if (allowed_biomes[i])
            {
                return (BiomeType)i;
            }
        }
    }

    return BIOME_PLAINS;
}

static bool find_world_spawn_inside_loaded_structure(World *world,
                                                     int origin_x,
                                                     int origin_y,
                                                     int width,
                                                     int height,
                                                     int *out_x,
                                                     int *out_y)
{
    if (world == NULL || out_x == NULL || out_y == NULL)
    {
        return false;
    }

    bool swimming = false;
    const TileDefinition *support = NULL;
    const TileDefinition *top = NULL;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const int world_x = origin_x + x;
            const int world_y = origin_y + y;
            if (world_can_occupy_tile(world, world_x, world_y, &swimming, &support, &top) &&
                !swimming)
            {
                *out_x = world_x;
                *out_y = world_y;
                return true;
            }
        }
    }

    return false;
}

bool structure_browser_load_into_world(const StructureBrowserSettings *settings,
                                       World *world,
                                       StructureBuilderConfig *config,
                                       char *out_error,
                                       size_t out_error_size)
{
    if (settings == NULL || world == NULL || config == NULL || settings->path[0] == '\0')
    {
        set_error(out_error, out_error_size, "No selected structure to edit.");
        return false;
    }

    if (settings->width <= 0 || settings->height <= 0 ||
        settings->width > world->width || settings->height > world->height)
    {
        set_error(out_error, out_error_size, "Selected structure does not fit in builder world.");
        return false;
    }

    char *json = load_text_file(settings->path, NULL);
    if (json == NULL)
    {
        set_error(out_error, out_error_size, "Could not read selected structure cells.");
        return false;
    }

    snprintf(config->name, sizeof(config->name), "%s", settings->name);
    config->builder_biome = first_allowed_biome(settings->allowed_biomes);
    config->save_kind = settings->save_kind;
    for (int i = 0; i < BIOME_TYPE_COUNT; i++)
    {
        config->allowed_biomes[i] = settings->allowed_biomes[i];
    }
    for (int i = 0; i < TILE_ID_COUNT; i++)
    {
        config->allowed_support_tiles[i] = settings->allowed_support_tiles[i];
    }

    const BiomeType builder_biome = config->builder_biome;
    const TileId builder_ground = structure_builder_biome_primary_tile(builder_biome);
    for (int y = 0; y < world->height; y++)
    {
        for (int x = 0; x < world->width; x++)
        {
            const int index = (y * world->width) + x;
            world->biomes[index] = builder_biome;
            world->temperatures_c[index] = 18.0f;
            world_set_tile_at_layer(world, x, y, TILE_LAYER_GROUND, builder_ground);
            world_set_tile_at_layer(world, x, y, TILE_LAYER_FLOOR, TILE_ID_COUNT);
            world_set_tile_at_layer(world, x, y, TILE_LAYER_OBJECT, TILE_ID_COUNT);
            world_set_tile_at_layer(world, x, y, TILE_LAYER_STRUCTURE, TILE_ID_COUNT);
        }
    }

    const int origin_x = (world->width - settings->width) / 2;
    const int origin_y = (world->height - settings->height) / 2;
    const char *cells = find_json_value(json, "cells");
    if (cells == NULL)
    {
        free(json);
        set_error(out_error, out_error_size, "Selected structure has no cells.");
        return false;
    }

    const char *cursor = skip_json_whitespace(cells);
    if (cursor == NULL || *cursor != '[')
    {
        free(json);
        set_error(out_error, out_error_size, "Selected structure cells are invalid.");
        return false;
    }
    cursor = skip_json_whitespace(cursor + 1);

    while (*cursor != '\0' && *cursor != ']')
    {
        BrowserCell cell;
        if (!parse_json_cell_object(cursor, &cell, &cursor))
        {
            free(json);
            set_error(out_error, out_error_size, "Could not parse selected structure cells.");
            return false;
        }

        const int world_x = origin_x + cell.x;
        const int world_y = origin_y + cell.y;
        if (!world_is_in_bounds(world, world_x, world_y))
        {
            free(json);
            set_error(out_error, out_error_size, "Selected structure cell is out of bounds.");
            return false;
        }

        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_GROUND,
                                cell.ground_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_FLOOR,
                                cell.floor_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_OBJECT,
                                cell.object_tile_id);
        world_set_tile_at_layer(world, world_x, world_y, TILE_LAYER_STRUCTURE,
                                cell.structure_tile_id);

        cursor = skip_json_whitespace(cursor);
        if (*cursor == ',')
        {
            cursor = skip_json_whitespace(cursor + 1);
        }
    }

    free(json);

    int spawn_x = origin_x;
    int spawn_y = origin_y;
    if (!find_world_spawn_inside_loaded_structure(world, origin_x, origin_y, settings->width,
                                                  settings->height, &spawn_x, &spawn_y))
    {
        spawn_x = origin_x + (settings->width / 2);
        spawn_y = origin_y + (settings->height / 2);
    }

    world->player_x = (float)(spawn_x * world->tile_size);
    world->player_y = (float)(spawn_y * world->tile_size);
    set_error(out_error, out_error_size, "");
    return true;
}
