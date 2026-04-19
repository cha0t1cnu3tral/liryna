#ifndef LIRYNA_SPEECH_H
#define LIRYNA_SPEECH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool speech_init(void);
void speech_shutdown(void);
bool speech_say(const char *text, bool interrupt);
bool speech_output(const char *text, bool interrupt);
void speech_wait(int timeout_ms);
void speech_stop(void);
const char *speech_backend_name(void);

#ifdef __cplusplus
}
#endif

#endif
