#include "settings.h"

#include <stdbool.h>
#include <stddef.h>

static int g_settings_values[SETTING_COUNT] = {
    100,
    100,
    100,
    100,
};

static bool setting_id_is_valid(SettingId id)
{
    return id >= 0 && id < SETTING_COUNT;
}

static int clamp_percent(int value)
{
    if (value < 0)
    {
        return 0;
    }

    if (value > 100)
    {
        return 100;
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

static int effective_category_volume(int base_volume, SettingId category_id)
{
    const int master = settings_get_int(SETTING_MASTER_VOLUME);
    const int category = settings_get_int(category_id);
    return clamp_mci_volume((base_volume * master * category) / 10000);
}

int settings_get_int(SettingId id)
{
    if (!setting_id_is_valid(id))
    {
        return 0;
    }

    return g_settings_values[id];
}

void settings_set_int(SettingId id, int value)
{
    if (!setting_id_is_valid(id))
    {
        return;
    }

    g_settings_values[id] = clamp_percent(value);
}

int *settings_get_int_ptr(SettingId id)
{
    if (!setting_id_is_valid(id))
    {
        return NULL;
    }

    return &g_settings_values[id];
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
