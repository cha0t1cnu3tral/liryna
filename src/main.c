#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "speech.h"

static bool running = true;
static int tick_count = 0;

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

void game_init(void)
{
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
    speech_wait(1500);
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
