#ifndef LIRYNA_OPENING_SCENE_H
#define LIRYNA_OPENING_SCENE_H

#include <stdbool.h>

typedef void (*OpeningSceneAnnounceFn)(const char *text, bool interrupt);

bool opening_scene_init(void);
void opening_scene_start(OpeningSceneAnnounceFn announce);
void opening_scene_update(float delta_time, bool enter_pressed, OpeningSceneAnnounceFn announce);
bool opening_scene_is_active(void);
void opening_scene_cancel(void);
void opening_scene_shutdown(void);

#endif
