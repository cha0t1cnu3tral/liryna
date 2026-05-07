#ifndef LIRYNA_AUDIO_BACKEND_H
#define LIRYNA_AUDIO_BACKEND_H

#include <stdbool.h>

typedef struct ma_engine ma_engine;

bool audio_backend_init(void);
void audio_backend_shutdown(void);
bool audio_backend_is_ready(void);
const char *audio_backend_name(void);
ma_engine *audio_backend_engine(void);

#endif
