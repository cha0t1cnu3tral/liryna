#ifndef LIRYNA_SETTINGS_H
#define LIRYNA_SETTINGS_H

typedef enum SettingId
{
    SETTING_MASTER_VOLUME = 0,
    SETTING_MUSIC_VOLUME,
    SETTING_AMBIENCE_VOLUME,
    SETTING_FOOTSTEPS_VOLUME,
    SETTING_COUNT
} SettingId;

int settings_get_int(SettingId id);
void settings_set_int(SettingId id, int value);
int *settings_get_int_ptr(SettingId id);

int settings_effective_music_mci_volume(int base_volume);
int settings_effective_ambience_mci_volume(int base_volume);
int settings_effective_footsteps_mci_volume(int base_volume);

#endif
