#include "engine.h"

#include <stdbool.h>

struct Engine
{
    bool running;
    int tick_count;
};

void engine_run(const EngineCallbacks *callbacks, void *userdata)
{
    Engine engine = {
        .running = true,
        .tick_count = 0,
    };

    if (callbacks && callbacks->init)
    {
        callbacks->init(&engine, userdata);
    }

    while (engine.running)
    {
        engine.tick_count++;

        if (callbacks && callbacks->update)
        {
            callbacks->update(&engine, userdata);
        }

        if (engine.running && callbacks && callbacks->render)
        {
            callbacks->render(&engine, userdata);
        }
    }

    if (callbacks && callbacks->shutdown)
    {
        callbacks->shutdown(&engine, userdata);
    }
}

void engine_stop(Engine *engine)
{
    if (engine)
    {
        engine->running = false;
    }
}

int engine_tick_count(const Engine *engine)
{
    return engine ? engine->tick_count : 0;
}
