#ifndef LIRYNA_WATER_BIOME_AUDIO_H
#define LIRYNA_WATER_BIOME_AUDIO_H

#include <stdbool.h>

typedef struct World World;

bool water_biome_audio_init(void);
void water_biome_audio_update(const World *world, float delta_time);
void water_biome_audio_shutdown(void);

#endif
