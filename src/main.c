#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "music_player.h"
#include "speech.h"
#include "ui/menu_ui.h"
#include "water_biome_audio.h"
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
    bool prev_page_up;
    bool prev_page_down;
    bool prev_home;

    bool has_prev_tile;
    int prev_tile_x;
    int prev_tile_y;

    int tracker_category_index;
    int tracker_object_index;
} Game;

typedef enum TrackerCategory
{
    TRACKER_CATEGORY_TREES = 0,
    TRACKER_CATEGORY_PLANTS,
    TRACKER_CATEGORY_ROCKS,
    TRACKER_CATEGORY_FURNITURE,
    TRACKER_CATEGORY_WATER,
    TRACKER_CATEGORY_STRUCTURES,
    TRACKER_CATEGORY_TERRAIN,
    TRACKER_CATEGORY_MISC
} TrackerCategory;

static const TrackerCategory k_tracker_categories[] = {
    TRACKER_CATEGORY_TREES,
    TRACKER_CATEGORY_PLANTS,
    TRACKER_CATEGORY_ROCKS,
    TRACKER_CATEGORY_FURNITURE,
    TRACKER_CATEGORY_WATER,
    TRACKER_CATEGORY_STRUCTURES,
    TRACKER_CATEGORY_TERRAIN,
    TRACKER_CATEGORY_MISC,
};

static const int k_tracker_category_count =
    (int)(sizeof(k_tracker_categories) / sizeof(k_tracker_categories[0]));

static const char *tracker_category_name(TrackerCategory category)
{
    switch (category)
    {
    case TRACKER_CATEGORY_TREES:
        return "Trees";
    case TRACKER_CATEGORY_PLANTS:
        return "Plants";
    case TRACKER_CATEGORY_ROCKS:
        return "Rocks";
    case TRACKER_CATEGORY_FURNITURE:
        return "Furniture";
    case TRACKER_CATEGORY_WATER:
        return "Water";
    case TRACKER_CATEGORY_STRUCTURES:
        return "Structures";
    case TRACKER_CATEGORY_TERRAIN:
        return "Terrain";
    case TRACKER_CATEGORY_MISC:
    default:
        return "Misc";
    }
}

static TrackerCategory tracker_category_for_tile(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return TRACKER_CATEGORY_MISC;
    }

    switch (tile->id)
    {
    case TILE_TREEOAK:
    case TILE_TREEPINE:
    case TILE_TREEBIRCH:
    case TILE_TREEPALM:
    case TILE_TREEDEAD:
    case TILE_TREEBURNT:
    case TILE_TREESAPLING:
        return TRACKER_CATEGORY_TREES;
    case TILE_BUSH:
    case TILE_BERRYBUSH:
    case TILE_FLOWERPATCH:
    case TILE_TALLWEEDS:
    case TILE_VINEPATCH:
    case TILE_VINES:
    case TILE_MUSHROOMPATCH:
    case TILE_GIANTMUSHROOM:
    case TILE_CACTUS:
    case TILE_DESERT_SHRUB:
        return TRACKER_CATEGORY_PLANTS;
    case TILE_BOULDER:
    case TILE_ROCKCLUSTER:
    case TILE_LARGEROCK:
    case TILE_CAVEENTRANCE:
        return TRACKER_CATEGORY_ROCKS;
    case TILE_TABLE:
    case TILE_TABLEROUND:
    case TILE_TABLELONG:
    case TILE_CHAIRWOOD:
    case TILE_CHAIRMETAL:
    case TILE_SOFA:
    case TILE_BED:
    case TILE_BUNKBED:
    case TILE_CABINET:
    case TILE_SHELF:
    case TILE_BOOKSHELF:
    case TILE_CHEST:
    case TILE_LOCKER:
    case TILE_DESK:
    case TILE_OFFICEDESK:
    case TILE_LAMP:
    case TILE_CEILINGLIGHT:
    case TILE_FLOORLAMP:
    case TILE_STOVE:
    case TILE_OVEN:
    case TILE_SINK:
    case TILE_BATHTUB:
    case TILE_TOILET:
    case TILE_SHOWER:
    case TILE_FRIDGE:
    case TILE_WASHINGMACHINE:
    case TILE_DRYER:
        return TRACKER_CATEGORY_FURNITURE;
    case TILE_WOODFOUNDATION:
    case TILE_STONEFOUNDATION:
    case TILE_CONCRETEFOUNDATION:
    case TILE_BRICKFOUNDATION:
    case TILE_STEELFOUNDATION:
    case TILE_REINFORCEDFOUNDATION:
    case TILE_PIER:
    case TILE_CARPET:
    case TILE_LOGWALL:
    case TILE_PLANKWALL:
    case TILE_STONEWALL:
    case TILE_BRICKWALL:
    case TILE_CONCRETEWALL:
    case TILE_REINFORCEDWALL:
    case TILE_STEELWALL:
    case TILE_GLASSWALL:
    case TILE_METALWALL:
    case TILE_FENCEWOOD:
    case TILE_FENCEMETAL:
    case TILE_FENCESTONE:
    case TILE_FENCECHAIN:
    case TILE_FENCEBARBED:
    case TILE_BARRICADEWOOD:
    case TILE_BARRICADESANDBAG:
    case TILE_RUINEDWALL:
    case TILE_CRACKEDWALL:
        return TRACKER_CATEGORY_STRUCTURES;
    default:
        break;
    }

    if (tile->is_liquid)
    {
        return TRACKER_CATEGORY_WATER;
    }

    if (tile->layer == TILE_LAYER_GROUND || tile->layer == TILE_LAYER_FLOOR)
    {
        return TRACKER_CATEGORY_TERRAIN;
    }

    return TRACKER_CATEGORY_MISC;
}

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static bool game_find_object_in_category(const World *world,
                                         TrackerCategory category,
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
            if (tile == NULL || tracker_category_for_tile(tile) != category)
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

    const TrackerCategory category = k_tracker_categories[game->tracker_category_index];
    const char *category_name = tracker_category_name(category);
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

    const TrackerCategory category = k_tracker_categories[game->tracker_category_index];
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
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
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
    game->prev_page_up = false;
    game->prev_page_down = false;
    game->prev_home = false;
    game->has_prev_tile = false;
    game->prev_tile_x = -1;
    game->prev_tile_y = -1;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;

    game->speech_ready = speech_init();
    if (!music_player_start_main_menu_music())
    {
        fprintf(stderr, "game: main menu music failed to start\n");
    }
    if (!water_biome_audio_init())
    {
        fprintf(stderr, "game: water biome audio failed to start\n");
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
    const bool page_up_now = engine_key_down(engine, SDL_SCANCODE_PAGEUP);
    const bool page_down_now = engine_key_down(engine, SDL_SCANCODE_PAGEDOWN);
    const bool home_now = engine_key_down(engine, SDL_SCANCODE_HOME);
    const bool ctrl_now = engine_key_down(engine, SDL_SCANCODE_LCTRL) ||
                          engine_key_down(engine, SDL_SCANCODE_RCTRL);

    const bool up_pressed = up_now && !game->prev_up;
    const bool down_pressed = down_now && !game->prev_down;
    const bool enter_pressed = enter_now && !game->prev_enter;
    const bool back_pressed = back_now && !game->prev_back;
    const bool c_pressed = c_now && !game->prev_c;
    const bool b_pressed = b_now && !game->prev_b;
    const bool t_pressed = t_now && !game->prev_t;
    const bool page_up_pressed = page_up_now && !game->prev_page_up;
    const bool page_down_pressed = page_down_now && !game->prev_page_down;
    const bool home_pressed = home_now && !game->prev_home;

    game->prev_up = up_now;
    game->prev_down = down_now;
    game->prev_enter = enter_now;
    game->prev_back = back_now;
    game->prev_c = c_now;
    game->prev_b = b_now;
    game->prev_t = t_now;
    game->prev_page_up = page_up_now;
    game->prev_page_down = page_down_now;
    game->prev_home = home_now;

    UiAction action = UI_ACTION_NONE;
    ui_update(&game->ui, up_pressed, down_pressed, enter_pressed, back_pressed,
              &action, game->speech_ready ? game_announce : NULL);

    if (action == UI_ACTION_EXIT)
    {
        engine_stop(engine);
        return;
    }

    if (action == UI_ACTION_NEW_WORLD)
    {
        if (!game_create_new_world(game))
        {
            ui_init(&game->ui, game->speech_ready ? game_announce : NULL);
        }
    }

    if (ui_screen(&game->ui) != UI_SCREEN_WORLD || !game->world_loaded)
    {
        water_biome_audio_update(NULL, engine_delta_time(engine));
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
    water_biome_audio_update(&game->world, engine_delta_time(engine));

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
            char message[192];
            if (tile != NULL && tile->primary_resource != NULL && tile->resource_yield > 0)
            {
                snprintf(message, sizeof(message),
                         "Tile %s. Resource %s %u. X %d Y %d.",
                         tile->name != NULL ? tile->name : "Unknown", tile->primary_resource,
                         (unsigned int)tile->resource_yield, tile_x, tile_y);
            }
            else
            {
                snprintf(message, sizeof(message), "Tile %s. X %d Y %d.",
                         tile != NULL && tile->name != NULL ? tile->name : "Unknown",
                         tile_x, tile_y);
            }
            game_announce(message, true);

            game->prev_tile_x = tile_x;
            game->prev_tile_y = tile_y;
            game->has_prev_tile = true;
        }
    }

    if (c_pressed)
    {
        char message[160];
        if (has_tile)
        {
            snprintf(message, sizeof(message), "Coordinates X %d Y %d. Tile %s.",
                     tile_x, tile_y,
                     tile != NULL && tile->name != NULL ? tile->name : "Unknown");
        }
        else
        {
            snprintf(message, sizeof(message), "Coordinates unavailable.");
        }

        game_announce(message, true);
    }

    if (b_pressed)
    {
        char message[196];
        if (has_tile && biome != NULL)
        {
            snprintf(message, sizeof(message), "Biome %s. Range %.0f to %.0f Celsius.",
                     biome->name, biome->min_temperature_c, biome->max_temperature_c);
        }
        else
        {
            snprintf(message, sizeof(message), "Biome unavailable.");
        }

        game_announce(message, true);
    }

    if (t_pressed)
    {
        char message[196];
        if (has_tile && biome != NULL)
        {
            snprintf(message, sizeof(message),
                     "Temperature %.1f Celsius in %s. Expected range %.0f to %.0f Celsius.",
                     temperature_c, biome->name, biome->min_temperature_c,
                     biome->max_temperature_c);
        }
        else
        {
            snprintf(message, sizeof(message), "Temperature unavailable.");
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
        const TrackerCategory category = k_tracker_categories[game->tracker_category_index];
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

    ui_render(&game->ui, renderer);
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
    water_biome_audio_shutdown();
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
