#ifndef LIRYNA_ENGINE_H
#define LIRYNA_ENGINE_H

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

#endif
