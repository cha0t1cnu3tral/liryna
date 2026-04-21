#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "music_player.h"
#include "speech.h"
#include "ui/ui.h"
#include "world/world.h"

typedef struct Game
{
    UiState ui;
    World world;
    bool world_loaded;
    bool speech_ready;

    bool prev_up;
    bool prev_down;
    bool prev_enter;
    bool prev_back;
    bool prev_c;
    bool prev_b;
    bool prev_t;

    bool has_prev_tile;
    int prev_tile_x;
    int prev_tile_y;
} Game;

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static bool game_create_new_world(Game *game)
{
    enum
    {
        WORLD_SIZE_TILES = 200,
    };

    if (game == NULL)
    {
        return false;
    }

    if (game->world_loaded)
    {
        world_shutdown(&game->world);
        game->world_loaded = false;
    }

    if (!world_init(&game->world, WORLD_SIZE_TILES, WORLD_SIZE_TILES, 32))
    {
        fprintf(stderr, "game: world initialization failed\n");
        game_announce("New world generation failed.", true);
        return false;
    }

    game->world_loaded = true;
    game->has_prev_tile = false;
    game_announce("New world ready.", false);
    return true;
}

static void game_init(Engine *engine, void *userdata)
{
    Game *game = userdata;
    (void)engine;

    game->world_loaded = false;
    game->prev_up = false;
    game->prev_down = false;
    game->prev_enter = false;
    game->prev_back = false;
    game->prev_c = false;
    game->prev_b = false;
    game->prev_t = false;
    game->has_prev_tile = false;
    game->prev_tile_x = -1;
    game->prev_tile_y = -1;

    game->speech_ready = speech_init();
    if (!music_player_start_main_menu_music())
    {
        fprintf(stderr, "game: main menu music failed to start\n");
    }

    srand((unsigned int)time(NULL));
    ui_init(&game->ui, game->speech_ready ? game_announce : NULL);
    if (game->speech_ready)
    {
        char message[128];
        snprintf(message, sizeof(message), "Speech backend: %s.",
                 speech_backend_name());
        game_announce(message, false);
    }
}

static void game_update(Engine *engine, void *userdata)
{
    Game *game = userdata;

    const bool up_now = engine_key_down(engine, SDL_SCANCODE_UP);
    const bool down_now = engine_key_down(engine, SDL_SCANCODE_DOWN);
    const bool enter_now = engine_key_down(engine, SDL_SCANCODE_RETURN);
    const bool back_now = engine_key_down(engine, SDL_SCANCODE_ESCAPE);
    const bool c_now = engine_key_down(engine, SDL_SCANCODE_C);
    const bool b_now = engine_key_down(engine, SDL_SCANCODE_B);
    const bool t_now = engine_key_down(engine, SDL_SCANCODE_T);
    const bool alt_down = engine_key_down(engine, SDL_SCANCODE_LALT) ||
                          engine_key_down(engine, SDL_SCANCODE_RALT);

    const bool up_pressed = up_now && !game->prev_up;
    const bool down_pressed = down_now && !game->prev_down;
    const bool enter_pressed = enter_now && !game->prev_enter;
    const bool back_pressed = back_now && !game->prev_back;
    const bool c_pressed = c_now && !game->prev_c;
    const bool b_pressed = b_now && !game->prev_b;
    const bool t_pressed = t_now && !game->prev_t;

    game->prev_up = up_now;
    game->prev_down = down_now;
    game->prev_enter = enter_now;
    game->prev_back = back_now;
    game->prev_c = c_now;
    game->prev_b = b_now;
    game->prev_t = t_now;

    UiAction action = UI_ACTION_NONE;
    ui_update(&game->ui, up_pressed, down_pressed, enter_pressed, back_pressed,
              &action, game->speech_ready ? game_announce : NULL);

    if (action == UI_ACTION_EXIT)
    {
        game_announce("Exiting.", true);
        engine_stop(engine);
        return;
    }

    if (action == UI_ACTION_NEW_WORLD)
    {
        game_announce("Generating new world.", true);
        if (game_create_new_world(game))
        {
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
        }
        else
        {
            ui_init(&game->ui, game->speech_ready ? game_announce : NULL);
        }
    }

    if (ui_screen(&game->ui) != UI_SCREEN_WORLD || !game->world_loaded)
    {
        game->has_prev_tile = false;
        return;
    }

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

    int tile_x = 0;
    int tile_y = 0;
    const TileDefinition *tile = NULL;
    const BiomeDefinition *biome = NULL;
    float temperature_c = 0.0f;
    const bool has_tile = world_get_player_environment(&game->world, &tile_x, &tile_y, &tile,
                                                       &biome, &temperature_c);
    if (has_tile)
    {
        const bool tile_changed = !game->has_prev_tile ||
                                  tile_x != game->prev_tile_x ||
                                  tile_y != game->prev_tile_y;
        if (tile_changed)
        {
            char message[160];
            snprintf(message, sizeof(message), "%s.",
                     tile != NULL && tile->name != NULL ? tile->name : "Unknown");
            game_announce(message, true);

            game->prev_tile_x = tile_x;
            game->prev_tile_y = tile_y;
            game->has_prev_tile = true;
        }
    }

    if (c_pressed)
    {
        char message[160];
        if (has_tile && alt_down)
        {
            snprintf(message, sizeof(message), "%s.",
                     tile != NULL && tile->name != NULL ? tile->name : "Unknown");
        }
        else if (has_tile)
        {
            snprintf(message, sizeof(message), "X %d Y %d.", tile_x, tile_y);
        }
        else
        {
            snprintf(message, sizeof(message), "Unavailable.");
        }

        game_announce(message, true);
    }

    if (b_pressed)
    {
        char message[196];
        if (has_tile && biome != NULL)
        {
            snprintf(message, sizeof(message), "%s.", biome->name);
        }
        else
        {
            snprintf(message, sizeof(message), "Unavailable.");
        }

        game_announce(message, true);
    }

    if (t_pressed)
    {
        char message[196];
        if (has_tile && biome != NULL && alt_down)
        {
            snprintf(message, sizeof(message), "%.0f to %.0f.",
                     biome->min_temperature_c, biome->max_temperature_c);
        }
        else if (has_tile && biome != NULL)
        {
            snprintf(message, sizeof(message), "%.1f.", temperature_c);
        }
        else
        {
            snprintf(message, sizeof(message), "Unavailable.");
        }

        game_announce(message, true);
    }
}

static void game_render(Engine *engine, void *userdata)
{
    Game *game = userdata;
    SDL_Renderer *renderer = engine_renderer(engine);

    if (ui_screen(&game->ui) == UI_SCREEN_WORLD && game->world_loaded)
    {
        world_render(&game->world, renderer);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

static void game_shutdown(Engine *engine, void *userdata)
{
    (void)engine;

    Game *game = userdata;

    if (game->speech_ready)
    {
        game_announce("Shutting down.", true);
        speech_wait(800);
    }

    if (game->world_loaded)
    {
        world_shutdown(&game->world);
        game->world_loaded = false;
    }

    music_player_shutdown();
    speech_shutdown();
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
