#include "settings.h"

#include <stdio.h>

#define SETTINGS_STRING_CAPACITY 128

typedef struct SettingDefinition
{
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
    [SETTING_MASTER_VOLUME] = {SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_MUSIC_VOLUME] = {SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_AMBIENCE_VOLUME] = {SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
    [SETTING_FOOTSTEPS_VOLUME] = {SETTING_TYPE_INT, 100, 0, 100, 5, false, ""},
};

static SettingValue g_setting_values[SETTING_COUNT];
static bool g_settings_initialized = false;

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

static void settings_init_once(void)
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

int settings_get_int(SettingId id)
{
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return 0;
    }

    return g_setting_values[id].int_value;
}

void settings_set_int(SettingId id, int value)
{
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_INT)
    {
        return;
    }

    const SettingDefinition *definition = &g_setting_definitions[id];
    g_setting_values[id].int_value =
        clamp_int(value, definition->int_min, definition->int_max);
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
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_BOOL)
    {
        return false;
    }

    return g_setting_values[id].bool_value;
}

void settings_set_bool(SettingId id, bool value)
{
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_BOOL)
    {
        return;
    }

    g_setting_values[id].bool_value = value;
}

const char *settings_get_string(SettingId id)
{
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_STRING)
    {
        return "";
    }

    return g_setting_values[id].string_value;
}

void settings_set_string(SettingId id, const char *value)
{
    settings_init_once();

    if (!settings_id_is_valid(id) ||
        g_setting_definitions[id].type != SETTING_TYPE_STRING)
    {
        return;
    }

    copy_setting_string(id, value);
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
