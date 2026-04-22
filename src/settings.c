#include "settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#define SETTINGS_STRING_CAPACITY 128
#define SETTINGS_JSON_CAPACITY 8192
#define SETTINGS_PATH_CAPACITY 512

typedef struct SettingDefinition
{
    const char *name;
    SettingType type;
    int int_default;
    int int_min;
    int int_max;
    int int_step;
    bool bool_default;
    const char *string_default;
} SettingDefinition;

typedef struct SettingValue
{
    int int_value;
    bool bool_value;
    char string_value[SETTINGS_STRING_CAPACITY];
} SettingValue;

static const SettingDefinition g_setting_definitions[SETTING_COUNT] = {
    [SETTING_MASTER_VOLUME] = {"master_volume", SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_MUSIC_VOLUME] = {"music_volume", SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_AMBIENCE_VOLUME] = {"ambience_volume", SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_FOOTSTEPS_VOLUME] = {"footsteps_volume", SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
};

static SettingValue g_setting_values[SETTING_COUNT];
static bool g_settings_initialized = false;
static bool g_settings_loading = false;

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static int clamp_mci_volume(int value)
{
    if (value < 0)
    {
        return 0;
    }

    if (value > 1000)
    {
        return 1000;
    }

    return value;
}

static void copy_setting_string(SettingId id, const char *value)
{
    if (value == NULL)
    {
        value = "";
    }

    snprintf(g_setting_values[id].string_value,
             sizeof(g_setting_values[id].string_value), "%s", value);
}

static void settings_init_defaults_once(void)
{
    if (g_settings_initialized)
    {
        return;
    }

    for (int i = 0; i < SETTING_COUNT; i++)
    {
        const SettingDefinition *definition = &g_setting_definitions[i];
        g_setting_values[i].int_value =
            clamp_int(definition->int_default, definition->int_min,
                      definition->int_max);
        g_setting_values[i].bool_value = definition->bool_default;
        copy_setting_string((SettingId)i, definition->string_default);
    }

    g_settings_initialized = true;
}

static bool settings_get_file_path(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return false;
    }

    out[0] = '\0';

    char *pref_path = SDL_GetPrefPath("Liryna", "Liryna");
    if (pref_path == NULL)
    {
        return false;
    }

    const int written = snprintf(out, out_size, "%ssettings.json", pref_path);
    SDL_free(pref_path);
    return written > 0 && (size_t)written < out_size;
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

    match += strlen(quoted_key);
    match = skip_json_whitespace(match);
    if (match == NULL || *match != ':')
    {
        return NULL;
    }

    return skip_json_whitespace(match + 1);
}

static bool parse_json_bool(const char *value, bool *out)
{
    if (value == NULL || out == NULL)
    {
        return false;
    }

    if (strncmp(value, "true", 4) == 0)
    {
        *out = true;
        return true;
    }

    if (strncmp(value, "false", 5) == 0)
    {
        *out = false;
        return true;
    }

    return false;
}

static bool parse_json_string(const char *value, char *out, size_t out_size)
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
            break;
        }

        out[out_index] = next;
        out_index++;
        value++;
    }

    if (*value != '"')
    {
        return false;
    }

    out[out_index] = '\0';
    return true;
}

static void settings_apply_json_value(SettingId id, const char *json)
{
    const SettingDefinition *definition = &g_setting_definitions[id];
    const char *value = find_json_value(json, definition->name);
    if (value == NULL)
    {
        return;
    }

    switch (definition->type)
    {
    case SETTING_TYPE_INT:
    {
        char *end = NULL;
        const long parsed = strtol(value, &end, 10);
        if (end != value)
        {
            g_setting_values[id].int_value =
                clamp_int((int)parsed, definition->int_min, definition->int_max);
        }
        break;
    }
    case SETTING_TYPE_BOOL:
    {
        bool parsed = false;
        if (parse_json_bool(value, &parsed))
        {
            g_setting_values[id].bool_value = parsed;
        }
        break;
    }
    case SETTING_TYPE_STRING:
    {
        char parsed[SETTINGS_STRING_CAPACITY];
        if (parse_json_string(value, parsed, sizeof(parsed)))
        {
            copy_setting_string(id, parsed);
        }
        break;
    }
    default:
        break;
    }
}

static bool append_json_text(char *json, size_t json_size, const char *text)
{
    const size_t length = strlen(json);
    const int written = snprintf(json + length, json_size - length, "%s", text);
    return written >= 0 && (size_t)written < json_size - length;
}

static bool append_json_string(char *json, size_t json_size, const char *text)
{
    if (!append_json_text(json, json_size, "\""))
    {
        return false;
    }

    if (text == NULL)
    {
        text = "";
    }

    for (const char *cursor = text; *cursor != '\0'; cursor++)
    {
        char escaped[8];
        switch (*cursor)
        {
        case '"':
            snprintf(escaped, sizeof(escaped), "\\\"");
            break;
        case '\\':
            snprintf(escaped, sizeof(escaped), "\\\\");
            break;
        case '\n':
            snprintf(escaped, sizeof(escaped), "\\n");
            break;
        case '\r':
            snprintf(escaped, sizeof(escaped), "\\r");
            break;
        case '\t':
            snprintf(escaped, sizeof(escaped), "\\t");
            break;
        default:
            if ((unsigned char)*cursor < 32)
            {
                snprintf(escaped, sizeof(escaped), "\\u%04x",
                         (unsigned int)(unsigned char)*cursor);
            }
            else
            {
                snprintf(escaped, sizeof(escaped), "%c", *cursor);
            }
            break;
        }

        if (!append_json_text(json, json_size, escaped))
        {
            return false;
        }
    }

    return append_json_text(json, json_size, "\"");
}

static int effective_category_volume(int base_volume, SettingId category_id)
{
    const int master = settings_get_int(SETTING_MASTER_VOLUME);
    const int category = settings_get_int(category_id);
    return clamp_mci_volume((base_volume * master * category) / 10000);
}

bool settings_id_is_valid(SettingId id)
{
    return id >= 0 && id < SETTING_COUNT;
}

SettingType settings_get_type(SettingId id)
{
    if (!settings_id_is_valid(id))
    {
        return SETTING_TYPE_INT;
    }

    return g_setting_definitions[id].type;
}

bool settings_load(void)
{
    settings_init_defaults_once();

    char path[SETTINGS_PATH_CAPACITY];
    if (!settings_get_file_path(path, sizeof(path)))
    {
        return false;
    }

    size_t data_size = 0;
    char *data = SDL_LoadFile(path, &data_size);
    if (data == NULL)
    {
        return true;
    }

    char *json = malloc(data_size + 1);
    if (json == NULL)
    {
        SDL_free(data);
        return false;
    }

    memcpy(json, data, data_size);
    json[data_size] = '\0';
    SDL_free(data);

    g_settings_loading = true;
    for (int i = 0; i < SETTING_COUNT; i++)
    {
        settings_apply_json_value((SettingId)i, json);
    }
    g_settings_loading = false;

    free(json);
    return true;
}

bool settings_save(void)
{
    settings_init_defaults_once();

    char json[SETTINGS_JSON_CAPACITY] = "{\n";

    for (int i = 0; i < SETTING_COUNT; i++)
    {
        const SettingId id = (SettingId)i;
        const SettingDefinition *definition = &g_setting_definitions[id];

        if (!append_json_text(json, sizeof(json), "  ") ||
            !append_json_string(json, sizeof(json), definition->name) ||
            !append_json_text(json, sizeof(json), ": "))
        {
            return false;
        }

        char value[160];
        switch (definition->type)
        {
        case SETTING_TYPE_INT:
            snprintf(value, sizeof(value), "%d", g_setting_values[id].int_value);
            if (!append_json_text(json, sizeof(json), value))
            {
                return false;
            }
            break;
        case SETTING_TYPE_BOOL:
            if (!append_json_text(json, sizeof(json),
                                  g_setting_values[id].bool_value ? "true" : "false"))
            {
                return false;
            }
            break;
        case SETTING_TYPE_STRING:
            if (!append_json_string(json, sizeof(json),
                                    g_setting_values[id].string_value))
            {
                return false;
            }
            break;
        default:
            return false;
        }

        if (!append_json_text(json, sizeof(json),
                              i + 1 < SETTING_COUNT ? ",\n" : "\n"))
        {
            return false;
        }
    }

    if (!append_json_text(json, sizeof(json), "}\n"))
    {
        return false;
    }

    char path[SETTINGS_PATH_CAPACITY];
    if (!settings_get_file_path(path, sizeof(path)))
    {
        return false;
    }

    return SDL_SaveFile(path, json, strlen(json));
}

int settings_get_int(SettingId id)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return 0;
    }

    return g_setting_values[id].int_value;
}

void settings_set_int(SettingId id, int value)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return;
    }

    const SettingDefinition *definition = &g_setting_definitions[id];
    const int next_value = clamp_int(value, definition->int_min, definition->int_max);
    if (g_setting_values[id].int_value == next_value)
    {
        return;
    }

    g_setting_values[id].int_value = next_value;
    if (!g_settings_loading)
    {
        settings_save();
    }
}

int settings_get_int_min(SettingId id)
{
    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return 0;
    }

    return g_setting_definitions[id].int_min;
}

int settings_get_int_max(SettingId id)
{
    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return 0;
    }

    return g_setting_definitions[id].int_max;
}

int settings_get_int_step(SettingId id)
{
    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return 1;
    }

    return g_setting_definitions[id].int_step > 0
               ? g_setting_definitions[id].int_step
               : 1;
}

bool settings_get_bool(SettingId id)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_BOOL)
    {
        return false;
    }

    return g_setting_values[id].bool_value;
}

void settings_set_bool(SettingId id, bool value)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_BOOL)
    {
        return;
    }

    if (g_setting_values[id].bool_value == value)
    {
        return;
    }

    g_setting_values[id].bool_value = value;
    if (!g_settings_loading)
    {
        settings_save();
    }
}

const char *settings_get_string(SettingId id)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_STRING)
    {
        return "";
    }

    return g_setting_values[id].string_value;
}

void settings_set_string(SettingId id, const char *value)
{
    settings_init_defaults_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_STRING)
    {
        return;
    }

    if (value == NULL)
    {
        value = "";
    }

    if (strcmp(g_setting_values[id].string_value, value) == 0)
    {
        return;
    }

    copy_setting_string(id, value);
    if (!g_settings_loading)
    {
        settings_save();
    }
}

size_t settings_get_string_capacity(SettingId id)
{
    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_STRING)
    {
        return 0;
    }

    return sizeof(g_setting_values[id].string_value);
}

int settings_effective_music_mci_volume(int base_volume)
{
    return effective_category_volume(base_volume, SETTING_MUSIC_VOLUME);
}

int settings_effective_ambience_mci_volume(int base_volume)
{
    return effective_category_volume(base_volume, SETTING_AMBIENCE_VOLUME);
}

int settings_effective_footsteps_mci_volume(int base_volume)
{
    return effective_category_volume(base_volume, SETTING_FOOTSTEPS_VOLUME);
}
