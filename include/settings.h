#ifndef LIRYNA_SETTINGS_H
#define LIRYNA_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum SettingId
{
    SETTING_MASTER_VOLUME = 0,
    SETTING_MUSIC_VOLUME,
    SETTING_AMBIENCE_VOLUME,
    SETTING_FOOTSTEPS_VOLUME,
    SETTING_COUNT
} SettingId;

typedef enum SettingType
{
    SETTING_TYPE_INT = 0,
    SETTING_TYPE_BOOL,
    SETTING_TYPE_STRING,
} SettingType;

bool settings_id_is_valid(SettingId id);
SettingType settings_get_type(SettingId id);

bool settings_load(void);
bool settings_save(void);

int settings_get_int(SettingId id);
void settings_set_int(SettingId id, int value);
int settings_get_int_min(SettingId id);
int settings_get_int_max(SettingId id);
int settings_get_int_step(SettingId id);

bool settings_get_bool(SettingId id);
void settings_set_bool(SettingId id, bool value);

const char *settings_get_string(SettingId id);
void settings_set_string(SettingId id, const char *value);
size_t settings_get_string_capacity(SettingId id);

int settings_effective_music_mci_volume(int base_volume);
int settings_effective_ambience_mci_volume(int base_volume);
int settings_effective_footsteps_mci_volume(int base_volume);

#endif
