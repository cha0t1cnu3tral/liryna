#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "speech.h"

static bool running = true;
static int tick_count = 0;

void game_init(void)
{
    printf("Life Simulation Engine\n");
    printf("Initializing...\n");

    srand((unsigned int)time(NULL));

    if (speech_init())
    {
        printf("Speech backend: %s\n", speech_backend_name());
        speech_say("Life Simulation Engine ready.", true);
    }

    printf("Ready.\n");
}

void game_update(void)
{
    tick_count++;

    printf("Simulation tick: %d\n", tick_count);

    if (tick_count == 1 || tick_count == 5 || tick_count == 10)
    {
        char message[64];
        snprintf(message, sizeof(message), "Simulation tick %d.", tick_count);
        speech_say(message, false);
    }

    if (tick_count >= 10)
    {
        running = false;
    }
}

void game_render(void)
{
    printf("Rendering world state...\n");
}

void game_shutdown(void)
{
    speech_say("Shutting down.", true);
    speech_shutdown();

    printf("Shutting down.\n");
}

void game_loop(void)
{
    while (running)
    {
        game_update();
        game_render();
    }
}

int main(void)
{
    game_init();
    game_loop();
    game_shutdown();
    return 0;
}
