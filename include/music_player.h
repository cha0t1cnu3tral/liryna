#ifndef LIRYNA_MUSIC_PLAYER_H
#define LIRYNA_MUSIC_PLAYER_H

#include <stdbool.h>

bool music_player_start_main_menu_music(void);
void music_player_update(bool in_world, float delta_time);
void music_player_set_suspended(bool suspended);
void music_player_update_volume(void);
void music_player_shutdown(void);

#endif
