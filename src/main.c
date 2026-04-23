#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "inventory.h"
#include "music_player.h"
#include "opening_scene.h"
#include "settings.h"
#include "speech.h"
#include "tile_categories.h"
#include "water_biome_audio.h"
#include "ui/ui.h"
#include "world/world.h"

typedef struct Game
{
    UiState ui;
    World world;
    bool world_loaded;
    bool speech_ready;
    GameMode game_mode;
    Inventory inventory;

    bool prev_up;
    bool prev_down;
    bool prev_left;
    bool prev_right;
    bool prev_enter;
    bool prev_back;
    bool prev_backspace;
    bool prev_c;
    bool prev_b;
    bool prev_t;
    bool prev_page_up;
    bool prev_page_down;
    bool prev_home;
    bool prev_tab;
    bool prev_space;
    bool prev_e;

    bool has_prev_tile;
    int prev_tile_x;
    int prev_tile_y;
    bool has_prev_blocked_tile;
    int prev_blocked_tile_x;
    int prev_blocked_tile_y;

    int tracker_category_index;
    int tracker_object_index;
} Game;

static const TileCategory k_tracker_categories[] = {
    TILE_CATEGORY_TREES,
    TILE_CATEGORY_PLANTS,
    TILE_CATEGORY_ROCKS,
    TILE_CATEGORY_FURNITURE,
    TILE_CATEGORY_WATER,
    TILE_CATEGORY_STRUCTURES,
    TILE_CATEGORY_TERRAIN,
    TILE_CATEGORY_MISC,
};

static const int k_tracker_category_count =
    (int)(sizeof(k_tracker_categories) / sizeof(k_tracker_categories[0]));

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static const TileDefinition *game_get_tile_at(const World *world, int tile_x, int tile_y)
{
    if (world == NULL || world->tiles == NULL || world->width <= 0 || world->height <= 0)
    {
        return NULL;
    }

    if (tile_x < 0 || tile_y < 0 || tile_x >= world->width || tile_y >= world->height)
    {
        return NULL;
    }

    const int index = (tile_y * world->width) + tile_x;
    return tiles_get_definition(world->tiles[index]);
}

static bool game_is_feature_blocking_tile(const TileDefinition *tile)
{
    return tile != NULL &&
           tile->blocks_land_movement &&
           (tile->layer == TILE_LAYER_OBJECT || tile->layer == TILE_LAYER_STRUCTURE);
}

static void game_set_world_tile_if_in_bounds(World *world, int tile_x, int tile_y, TileId tile_id)
{
    if (world == NULL || world->tiles == NULL || world->width <= 0 || world->height <= 0)
    {
        return;
    }

    if (tile_x < 0 || tile_y < 0 || tile_x >= world->width || tile_y >= world->height)
    {
        return;
    }

    world->tiles[(tile_y * world->width) + tile_x] = tile_id;
}

static void game_place_opening_wreckage(Game *game)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return;
    }

    const int spawn_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int spawn_y = (int)(game->world.player_y / (float)game->world.tile_size);

    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 1, spawn_y, TILE_SMALLAXE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 2, spawn_y, TILE_PICKAXE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 1, spawn_y + 1, TILE_READER);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x, spawn_y + 1, TILE_RADIO);

    game_set_world_tile_if_in_bounds(&game->world, spawn_x - 1, spawn_y - 1, TILE_SHIPPIECE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x - 2, spawn_y - 1, TILE_SHIPPIECE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x - 1, spawn_y - 2, TILE_SHIPPIECE);
}

static void game_announce_blocked_feature_ahead(Game *game, int move_dir_x, int move_dir_y)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return;
    }

    if (move_dir_x == 0 && move_dir_y == 0)
    {
        game->has_prev_blocked_tile = false;
        return;
    }

    const int tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int tile_y = (int)(game->world.player_y / (float)game->world.tile_size);

    int candidates_x[3];
    int candidates_y[3];
    int candidate_count = 0;
    if (move_dir_x != 0 && move_dir_y != 0)
    {
        candidates_x[candidate_count] = tile_x + move_dir_x;
        candidates_y[candidate_count] = tile_y + move_dir_y;
        candidate_count++;
    }
    if (move_dir_x != 0)
    {
        candidates_x[candidate_count] = tile_x + move_dir_x;
        candidates_y[candidate_count] = tile_y;
        candidate_count++;
    }
    if (move_dir_y != 0)
    {
        candidates_x[candidate_count] = tile_x;
        candidates_y[candidate_count] = tile_y + move_dir_y;
        candidate_count++;
    }

    for (int i = 0; i < candidate_count; i++)
    {
        const int next_x = candidates_x[i];
        const int next_y = candidates_y[i];
        const TileDefinition *tile = game_get_tile_at(&game->world, next_x, next_y);
        if (!game_is_feature_blocking_tile(tile))
        {
            continue;
        }

        const bool already_announced =
            game->has_prev_blocked_tile &&
            game->prev_blocked_tile_x == next_x &&
            game->prev_blocked_tile_y == next_y;
        if (!already_announced)
        {
            char message[160];
            snprintf(message, sizeof(message), "%s.",
                     tile->name != NULL ? tile->name : "Obstacle");
            game_announce(message, true);
        }

        game->has_prev_blocked_tile = true;
        game->prev_blocked_tile_x = next_x;
        game->prev_blocked_tile_y = next_y;
        return;
    }

    game->has_prev_blocked_tile = false;
}

static bool game_find_object_in_category(const World *world,
                                         TileCategory category,
                                         int target_index,
                                         int *out_total,
                                         int *out_x,
                                         int *out_y,
                                         const TileDefinition **out_tile)
{
    if (world == NULL || world->tiles == NULL || world->width <= 0 ||
        world->height <= 0 || target_index < 0)
    {
        return false;
    }

    int found_count = 0;
    bool found_target = false;
    int found_x = 0;
    int found_y = 0;
    const TileDefinition *found_tile = NULL;

    for (int y = 0; y < world->height; y++)
    {
        for (int x = 0; x < world->width; x++)
        {
            const int index = (y * world->width) + x;
            const TileDefinition *tile = tiles_get_definition(world->tiles[index]);
            if (tile == NULL || tile_category_for_definition(tile) != category)
            {
                continue;
            }

            if (found_count == target_index)
            {
                found_x = x;
                found_y = y;
                found_tile = tile;
                found_target = true;
            }

            found_count++;
        }
    }

    if (out_total != NULL)
    {
        *out_total = found_count;
    }

    if (!found_target)
    {
        return false;
    }

    if (out_x != NULL)
    {
        *out_x = found_x;
    }
    if (out_y != NULL)
    {
        *out_y = found_y;
    }
    if (out_tile != NULL)
    {
        *out_tile = found_tile;
    }

    return true;
}

static void game_announce_tracker_focus(Game *game, bool interrupt)
{
    if (game == NULL || !game->world_loaded || game->tracker_category_index < 0 ||
        game->tracker_category_index >= k_tracker_category_count)
    {
        return;
    }

    const TileCategory category = k_tracker_categories[game->tracker_category_index];
    const char *category_name = tile_category_name(category);
    int total = 0;
    int object_x = 0;
    int object_y = 0;
    const TileDefinition *object_tile = NULL;

    const bool has_object = game_find_object_in_category(
        &game->world, category, game->tracker_object_index, &total, &object_x, &object_y,
        &object_tile);

    if (!has_object || total <= 0)
    {
        char message[128];
        snprintf(message, sizeof(message), "%s category. No objects found.",
                 category_name);
        game_announce(message, interrupt);
        return;
    }

    char message[224];
    snprintf(message, sizeof(message), "%s category. %s at X %d Y %d. %d of %d.",
             category_name,
             object_tile != NULL && object_tile->name != NULL ? object_tile->name : "Unknown",
             object_x, object_y, game->tracker_object_index + 1, total);
    game_announce(message, interrupt);
}

static void game_announce_tracker_coordinates(Game *game, bool interrupt)
{
    if (game == NULL || !game->world_loaded || game->tracker_category_index < 0 ||
        game->tracker_category_index >= k_tracker_category_count)
    {
        return;
    }

    const TileCategory category = k_tracker_categories[game->tracker_category_index];
    int total = 0;
    int object_x = 0;
    int object_y = 0;
    const TileDefinition *object_tile = NULL;
    const bool has_object = game_find_object_in_category(
        &game->world, category, game->tracker_object_index, &total, &object_x, &object_y,
        &object_tile);

    if (!has_object || total <= 0)
    {
        game_announce("Object coordinates unavailable.", interrupt);
        return;
    }

    char message[192];
    snprintf(message, sizeof(message), "%s coordinates X %d Y %d.",
             object_tile != NULL && object_tile->name != NULL ? object_tile->name : "Object",
             object_x, object_y);
    game_announce(message, interrupt);
}

static bool game_create_new_world(Game *game, GameMode mode)
{
    enum
    {
        WORLD_WIDTH_TILES = 360,
        WORLD_HEIGHT_TILES = 240,
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

    if (!world_init(&game->world, WORLD_WIDTH_TILES, WORLD_HEIGHT_TILES, 32))
    {
        fprintf(stderr, "game: world initialization failed\n");
        game_announce("New world generation failed.", true);
        return false;
    }

    game->world_loaded = true;
    game->game_mode = mode;
    inventory_init(&game->inventory, mode);
    if (mode == GAME_MODE_SURVIVAL)
    {
        game_place_opening_wreckage(game);
    }
    game->has_prev_tile = false;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game_announce(mode == GAME_MODE_CREATIVE ? "Creative world ready."
                                             : "Survival world ready.",
                  false);
    return true;
}

static void game_init(Engine *engine, void *userdata)
{
    Game *game = userdata;
    (void)engine;

    game->world_loaded = false;
    game->prev_up = false;
    game->prev_down = false;
    game->prev_left = false;
    game->prev_right = false;
    game->prev_enter = false;
    game->prev_back = false;
    game->prev_backspace = false;
    game->prev_c = false;
    game->prev_b = false;
    game->prev_t = false;
    game->prev_page_up = false;
    game->prev_page_down = false;
    game->prev_home = false;
    game->prev_tab = false;
    game->prev_space = false;
    game->prev_e = false;
    game->has_prev_tile = false;
    game->prev_tile_x = -1;
    game->prev_tile_y = -1;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->game_mode = GAME_MODE_SURVIVAL;
    inventory_init(&game->inventory, GAME_MODE_SURVIVAL);

    game->speech_ready = speech_init();
    if (!settings_load())
    {
        fprintf(stderr, "game: settings load failed\n");
    }

    if (!music_player_start_main_menu_music())
    {
        fprintf(stderr, "game: main menu music failed to start\n");
    }
    if (!water_biome_audio_init())
    {
        fprintf(stderr, "game: water biome audio failed to start\n");
    }
    if (!opening_scene_init())
    {
        fprintf(stderr, "game: opening scene audio failed to start\n");
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
    const bool left_now = engine_key_down(engine, SDL_SCANCODE_LEFT);
    const bool right_now = engine_key_down(engine, SDL_SCANCODE_RIGHT);
    const bool enter_now = engine_key_down(engine, SDL_SCANCODE_RETURN);
    const bool back_now = engine_key_down(engine, SDL_SCANCODE_ESCAPE);
    const bool backspace_now = engine_key_down(engine, SDL_SCANCODE_BACKSPACE);
    const bool c_now = engine_key_down(engine, SDL_SCANCODE_C);
    const bool b_now = engine_key_down(engine, SDL_SCANCODE_B);
    const bool t_now = engine_key_down(engine, SDL_SCANCODE_T);
    const bool page_up_now = engine_key_down(engine, SDL_SCANCODE_PAGEUP);
    const bool page_down_now = engine_key_down(engine, SDL_SCANCODE_PAGEDOWN);
    const bool home_now = engine_key_down(engine, SDL_SCANCODE_HOME);
    const bool tab_now = engine_key_down(engine, SDL_SCANCODE_TAB);
    const bool space_now = engine_key_down(engine, SDL_SCANCODE_SPACE);
    const bool e_now = engine_key_down(engine, SDL_SCANCODE_E);
    const bool shift_now = engine_key_down(engine, SDL_SCANCODE_LSHIFT) ||
                           engine_key_down(engine, SDL_SCANCODE_RSHIFT);
    const bool ctrl_now = engine_key_down(engine, SDL_SCANCODE_LCTRL) ||
                          engine_key_down(engine, SDL_SCANCODE_RCTRL);
    const bool alt_down = engine_key_down(engine, SDL_SCANCODE_LALT) ||
                          engine_key_down(engine, SDL_SCANCODE_RALT);

    const bool up_pressed = up_now && !game->prev_up;
    const bool down_pressed = down_now && !game->prev_down;
    const bool left_pressed = left_now && !game->prev_left;
    const bool right_pressed = right_now && !game->prev_right;
    const bool enter_pressed = enter_now && !game->prev_enter;
    const bool back_pressed = back_now && !game->prev_back;
    const bool backspace_pressed = backspace_now && !game->prev_backspace;
    const bool c_pressed = c_now && !game->prev_c;
    const bool b_pressed = b_now && !game->prev_b;
    const bool t_pressed = t_now && !game->prev_t;
    const bool page_up_pressed = page_up_now && !game->prev_page_up;
    const bool page_down_pressed = page_down_now && !game->prev_page_down;
    const bool home_pressed = home_now && !game->prev_home;
    const bool tab_pressed = tab_now && !game->prev_tab;
    const bool space_pressed = space_now && !game->prev_space;
    const bool e_pressed = e_now && !game->prev_e;
    const bool next_container_pressed = tab_pressed && !shift_now;
    const bool previous_container_pressed = tab_pressed && shift_now;

    game->prev_up = up_now;
    game->prev_down = down_now;
    game->prev_left = left_now;
    game->prev_right = right_now;
    game->prev_enter = enter_now;
    game->prev_back = back_now;
    game->prev_backspace = backspace_now;
    game->prev_c = c_now;
    game->prev_b = b_now;
    game->prev_t = t_now;
    game->prev_page_up = page_up_now;
    game->prev_page_down = page_down_now;
    game->prev_home = home_now;
    game->prev_tab = tab_now;
    game->prev_space = space_now;
    game->prev_e = e_now;

    UiAction action = UI_ACTION_NONE;
    ui_update(&game->ui, up_pressed, down_pressed, left_pressed, right_pressed,
              next_container_pressed, previous_container_pressed, enter_pressed,
              back_pressed, backspace_pressed, engine_text_input(engine), &action,
              game->speech_ready ? game_announce : NULL);
    music_player_update_volume();

    if (action == UI_ACTION_EXIT)
    {
        game_announce("Exiting.", true);
        engine_stop(engine);
        return;
    }

    if (action == UI_ACTION_START_WORLD_SURVIVAL ||
        action == UI_ACTION_START_WORLD_CREATIVE)
    {
        game_announce("Generating new world.", true);
        const GameMode mode = action == UI_ACTION_START_WORLD_CREATIVE
                                  ? GAME_MODE_CREATIVE
                                  : GAME_MODE_SURVIVAL;
        if (game_create_new_world(game, mode))
        {
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
            if (mode == GAME_MODE_SURVIVAL)
            {
                opening_scene_start(game->speech_ready ? game_announce : NULL);
            }
        }
        else
        {
            ui_init(&game->ui, game->speech_ready ? game_announce : NULL);
        }
    }

    if (action == UI_ACTION_SELECT_CREATIVE_TILE)
    {
        const char *label = ui_focused_widget_label(&game->ui);
        const TileId selected_tile = tiles_find_by_name(label);
        if (selected_tile != TILE_ID_COUNT)
        {
            game->inventory.selected_tile = selected_tile;
            char message[192];
            snprintf(message, sizeof(message), "%s selected.",
                     label != NULL ? label : "Tile");
            game_announce(message, true);
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
        }
    }

    if (e_pressed)
    {
        const UiScreen screen = ui_screen(&game->ui);
        if (screen == UI_SCREEN_WORLD && game->world_loaded)
        {
            if (game->game_mode == GAME_MODE_CREATIVE)
            {
                ui_show_screen(&game->ui, UI_SCREEN_CREATIVE_INVENTORY,
                               game->speech_ready ? game_announce : NULL);
            }
            else
            {
                game_announce("Inventory is only available in creative right now.", true);
            }
        }
        else if (screen == UI_SCREEN_CREATIVE_INVENTORY)
        {
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
        }
    }

    const bool in_world_screen = ui_screen(&game->ui) == UI_SCREEN_WORLD && game->world_loaded;
    if (!in_world_screen && opening_scene_is_active())
    {
        opening_scene_cancel();
    }

    const float delta_time = engine_delta_time(engine);
    if (in_world_screen)
    {
        opening_scene_update(delta_time, enter_pressed, game->speech_ready ? game_announce : NULL);
    }

    const bool opening_scene_active = in_world_screen && opening_scene_is_active();
    music_player_set_suspended(opening_scene_active);
    music_player_update(in_world_screen, delta_time);

    if (!in_world_screen)
    {
        water_biome_audio_update(NULL, delta_time);
        game->has_prev_tile = false;
        game->has_prev_blocked_tile = false;
        return;
    }

    if (opening_scene_active)
    {
        water_biome_audio_update(NULL, delta_time);
        game->has_prev_tile = false;
        game->has_prev_blocked_tile = false;
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

    const float player_x_before = game->world.player_x;
    const float player_y_before = game->world.player_y;
    world_update(&game->world, delta_time, move_x, move_y, space_pressed);
    water_biome_audio_update(&game->world, delta_time);

    if (move_x != 0.0f || move_y != 0.0f)
    {
        const bool blocked =
            SDL_fabsf(game->world.player_x - player_x_before) < 0.001f &&
            SDL_fabsf(game->world.player_y - player_y_before) < 0.001f;
        if (blocked)
        {
            const int move_dir_x = move_x > 0.0f ? 1 : (move_x < 0.0f ? -1 : 0);
            const int move_dir_y = move_y > 0.0f ? 1 : (move_y < 0.0f ? -1 : 0);
            game_announce_blocked_feature_ahead(game, move_dir_x, move_dir_y);
        }
        else
        {
            game->has_prev_blocked_tile = false;
        }
    }
    else
    {
        game->has_prev_blocked_tile = false;
    }

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

    if (ctrl_now && (page_up_pressed || page_down_pressed))
    {
        if (page_up_pressed)
        {
            game->tracker_category_index--;
            if (game->tracker_category_index < 0)
            {
                game->tracker_category_index = k_tracker_category_count - 1;
            }
        }
        else
        {
            game->tracker_category_index++;
            if (game->tracker_category_index >= k_tracker_category_count)
            {
                game->tracker_category_index = 0;
            }
        }

        game->tracker_object_index = 0;
        game_announce_tracker_focus(game, true);
    }
    else if (page_up_pressed || page_down_pressed)
    {
        const TileCategory category = k_tracker_categories[game->tracker_category_index];
        int total = 0;
        const bool has_object =
            game_find_object_in_category(&game->world, category, 0, &total, NULL, NULL, NULL);
        if (!has_object || total <= 0)
        {
            game->tracker_object_index = 0;
            game_announce_tracker_focus(game, true);
        }
        else
        {
            if (page_up_pressed)
            {
                game->tracker_object_index--;
                if (game->tracker_object_index < 0)
                {
                    game->tracker_object_index = total - 1;
                }
            }
            else
            {
                game->tracker_object_index++;
                if (game->tracker_object_index >= total)
                {
                    game->tracker_object_index = 0;
                }
            }
            game_announce_tracker_focus(game, true);
        }
    }

    if (home_pressed)
    {
        game_announce_tracker_coordinates(game, true);
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
    opening_scene_shutdown();
    water_biome_audio_shutdown();
    settings_save();
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
