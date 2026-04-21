#ifndef LIRYNA_ENGINE_H
#define LIRYNA_ENGINE_H

#include <stdbool.h>

#include <SDL3/SDL.h>

typedef struct Engine Engine;

typedef struct EngineCallbacks
{
    void (*init)(Engine *engine, void *userdata);
    void (*update)(Engine *engine, void *userdata);
    void (*render)(Engine *engine, void *userdata);
    void (*shutdown)(Engine *engine, void *userdata);
} EngineCallbacks;

void engine_run(const EngineCallbacks *callbacks, void *userdata);
void engine_stop(Engine *engine);
int engine_tick_count(const Engine *engine);
float engine_delta_time(const Engine *engine);
bool engine_key_down(const Engine *engine, SDL_Scancode key);
const char *engine_text_input(const Engine *engine);
SDL_Renderer *engine_renderer(Engine *engine);

#endif
