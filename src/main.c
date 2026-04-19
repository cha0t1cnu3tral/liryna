#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "speech.h"

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static void game_init(Engine *engine, void *userdata)
{
    (void)engine;
    (void)userdata;

    bool speech_ready = speech_init();

    if (speech_ready)
    {
        game_announce("Life Simulation Engine.", true);
        game_announce("Initializing.", false);
    }

    srand((unsigned int)time(NULL));

    if (speech_ready)
    {
        char message[128];
        snprintf(message, sizeof(message), "Speech backend: %s.",
                 speech_backend_name());
        game_announce(message, false);
        game_announce("Ready.", false);
        speech_wait(3000);
    }
}

static void game_update(Engine *engine, void *userdata)
{
    (void)userdata;

    int tick_count = engine_tick_count(engine);

    printf("Simulation tick: %d\n", tick_count);

    if (tick_count == 1 || tick_count == 5 || tick_count == 10)
    {
        char message[64];
        snprintf(message, sizeof(message), "Simulation tick %d.", tick_count);
        game_announce(message, false);
    }

    if (tick_count >= 10)
    {
        engine_stop(engine);
    }
}

static void game_render(Engine *engine, void *userdata)
{
    (void)engine;
    (void)userdata;

    printf("Rendering world state...\n");
}

static void game_shutdown(Engine *engine, void *userdata)
{
    (void)engine;
    (void)userdata;

    game_announce("Shutting down.", true);
    speech_wait(1500);
    speech_shutdown();

    printf("Shutting down.\n");
}

int main(void)
{
    const EngineCallbacks callbacks = {
        .init = game_init,
        .update = game_update,
        .render = game_render,
        .shutdown = game_shutdown,
    };

    engine_run(&callbacks, NULL);
    return 0;
}
