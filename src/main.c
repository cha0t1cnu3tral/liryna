#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "speech.h"
#include "world/world.h"

typedef struct Game
{
    World world;
} Game;

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static void game_init(Engine *engine, void *userdata)
{
    Game *game = userdata;

    bool speech_ready = speech_init();

    if (speech_ready)
    {
        game_announce("Life Simulation Engine.", true);
        game_announce("Initializing.", false);
    }

    srand((unsigned int)time(NULL));

    if (!world_init(&game->world, 40, 22, 32))
    {
        fprintf(stderr, "game: world initialization failed\n");
        engine_stop(engine);
        return;
    }

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
    Game *game = userdata;

    int tick_count = engine_tick_count(engine);
    float move_x = 0.0f;
    float move_y = 0.0f;

    if (engine_key_down(engine, SDL_SCANCODE_W) ||
        engine_key_down(engine, SDL_SCANCODE_UP))
    {
        move_y -= 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_S) ||
        engine_key_down(engine, SDL_SCANCODE_DOWN))
    {
        move_y += 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_A) ||
        engine_key_down(engine, SDL_SCANCODE_LEFT))
    {
        move_x -= 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_D) ||
        engine_key_down(engine, SDL_SCANCODE_RIGHT))
    {
        move_x += 1.0f;
    }

    world_update(&game->world, engine_delta_time(engine), move_x, move_y);

    if ((tick_count % 60) == 0)
    {
        printf("Simulation tick: %d\n", tick_count);
    }

    if (tick_count == 1 || tick_count == 300 || tick_count == 600)
    {
        char message[64];
        snprintf(message, sizeof(message), "Simulation tick %d.", tick_count);
        game_announce(message, false);
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

    Game *game = userdata;

    game_announce("Shutting down.", true);
    speech_wait(1500);
    speech_shutdown();
    world_shutdown(&game->world);

    printf("Shutting down.\n");
}

int main(void)
{
    Game game;

    const EngineCallbacks callbacks = {
        .init = game_init,
        .update = game_update,
        .render = game_render,
        .shutdown = game_shutdown,
    };

    engine_run(&callbacks, &game);
    return 0;
}
