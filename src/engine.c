#include "engine.h"

#include <stdbool.h>
#include <stdio.h>

struct Engine
{
    bool running;
    int tick_count;
    float delta_time;
    Uint64 previous_counter;
    const bool *keyboard_state;
    char text_input[128];
    SDL_Window *window;
    SDL_Renderer *renderer;
};

void engine_run(const EngineCallbacks *callbacks, void *userdata)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "engine: SDL initialization failed: %s\n",
                SDL_GetError());
        return;
    }

    Engine engine = {
        .running = true,
        .tick_count = 0,
        .delta_time = 0.0f,
        .previous_counter = SDL_GetPerformanceCounter(),
        .keyboard_state = NULL,
        .text_input = "",
        .window = NULL,
        .renderer = NULL,
    };

    engine.window = SDL_CreateWindow("Liryna", 0, 0, SDL_WINDOW_FULLSCREEN);
    if (!engine.window)
    {
        fprintf(stderr, "engine: window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    engine.renderer = SDL_CreateRenderer(engine.window, NULL);
    if (!engine.renderer)
    {
        fprintf(stderr, "engine: renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(engine.window);
        SDL_Quit();
        return;
    }

    if (!SDL_StartTextInput(engine.window))
    {
        fprintf(stderr, "engine: text input start failed: %s\n", SDL_GetError());
    }

    if (callbacks && callbacks->init)
    {
        callbacks->init(&engine, userdata);
    }

    while (engine.running)
    {
        engine.text_input[0] = '\0';

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                engine_stop(&engine);
            }
            else if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text != NULL)
            {
                SDL_strlcat(engine.text_input, event.text.text, sizeof(engine.text_input));
            }
        }

        engine.keyboard_state = SDL_GetKeyboardState(NULL);

        Uint64 current_counter = SDL_GetPerformanceCounter();
        engine.delta_time =
            (float)((double)(current_counter - engine.previous_counter) /
                    (double)SDL_GetPerformanceFrequency());
        engine.previous_counter = current_counter;

        engine.tick_count++;

        if (callbacks && callbacks->update)
        {
            callbacks->update(&engine, userdata);
        }

        if (engine.running && callbacks && callbacks->render)
        {
            callbacks->render(&engine, userdata);
        }

        SDL_RenderPresent(engine.renderer);

        SDL_Delay(1);
    }

    if (callbacks && callbacks->shutdown)
    {
        callbacks->shutdown(&engine, userdata);
    }

    SDL_StopTextInput(engine.window);
    SDL_DestroyRenderer(engine.renderer);
    SDL_DestroyWindow(engine.window);
    SDL_Quit();
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

float engine_delta_time(const Engine *engine)
{
    return engine ? engine->delta_time : 0.0f;
}

bool engine_key_down(const Engine *engine, SDL_Scancode key)
{
    if (!engine || !engine->keyboard_state)
    {
        return false;
    }

    return engine->keyboard_state[key];
}

const char *engine_text_input(const Engine *engine)
{
    return engine ? engine->text_input : "";
}

SDL_Renderer *engine_renderer(Engine *engine)
{
    return engine ? engine->renderer : NULL;
}
