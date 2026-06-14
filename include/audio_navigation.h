#ifndef LIRYNA_AUDIO_NAVIGATION_H
#define LIRYNA_AUDIO_NAVIGATION_H

#include <stdbool.h>

bool audio_navigation_init(void);
void audio_navigation_shutdown(void);

bool audio_navigation_is_active(void);

/* Begins navigation: plays the start cue and starts the looping ping. */
void audio_navigation_start(void);

/* Stops the looping ping and deactivates navigation. */
void audio_navigation_stop(void);

/*
 * Drives the looping ping each frame while navigation is active.
 *
 * The ping is panned and pitched toward the tracked object, with volume scaled
 * by distance. When the player reaches the object (within one tile), the goal
 * cue plays and navigation deactivates automatically.
 *
 * When has_target is false the ping falls silent but navigation stays active.
 */
void audio_navigation_update(bool has_target,
                             int player_tile_x,
                             int player_tile_y,
                             int target_tile_x,
                             int target_tile_y,
                             float delta_time);

#endif
