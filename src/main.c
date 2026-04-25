#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "engine.h"
#include "audio_backend.h"
#include "inventory.h"
#include "music_player.h"
#include "opening_scene.h"
#include "settings.h"
#include "speech.h"
#include "tile_distance.h"
#include "tile_categories.h"
#include "water_biome_audio.h"
#include "ui/ui.h"
#include "ui/screens/screen_registry.h"
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
    bool prev_j;
    bool prev_k;
    bool prev_page_up;
    bool prev_page_down;
    bool prev_home;
    bool prev_tab;
    bool prev_space;
    bool prev_e;
    bool prev_right_bracket;
    bool prev_mouse_right;
    bool prev_hotbar_keys[INVENTORY_HOTBAR_SLOT_COUNT];

    bool has_prev_tile;
    int prev_tile_x;
    int prev_tile_y;
    bool cursor_locked_to_player;
    int cursor_tile_x;
    int cursor_tile_y;
    bool has_prev_blocked_tile;
    int prev_blocked_tile_x;
    int prev_blocked_tile_y;

    int tracker_category_index;
    int tracker_object_index;
    int pending_hotbar_tile;
    int pending_inventory_slot;
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

static void game_clear_pending_inventory_item(Game *game);

static bool game_get_player_tile_position(const Game *game, int *out_x, int *out_y)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0 ||
        out_x == NULL || out_y == NULL)
    {
        return false;
    }

    *out_x = (int)(game->world.player_x / (float)game->world.tile_size);
    *out_y = (int)(game->world.player_y / (float)game->world.tile_size);
    return true;
}

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

static bool game_move_cursor(Game *game, int delta_x, int delta_y)
{
    if (game == NULL || !game->world_loaded)
    {
        return false;
    }

    if (game->cursor_locked_to_player)
    {
        int player_tile_x = 0;
        int player_tile_y = 0;
        if (!game_get_player_tile_position(game, &player_tile_x, &player_tile_y))
        {
            return false;
        }

        game->cursor_tile_x = player_tile_x;
        game->cursor_tile_y = player_tile_y;
    }

    const int next_x = game->cursor_tile_x + delta_x;
    const int next_y = game->cursor_tile_y + delta_y;
    if (game_get_tile_at(&game->world, next_x, next_y) == NULL)
    {
        return false;
    }

    game->cursor_locked_to_player = false;
    game->cursor_tile_x = next_x;
    game->cursor_tile_y = next_y;
    return true;
}

static void game_recenter_cursor(Game *game)
{
    if (game == NULL)
    {
        return;
    }

    int player_tile_x = 0;
    int player_tile_y = 0;
    if (!game_get_player_tile_position(game, &player_tile_x, &player_tile_y))
    {
        return;
    }

    game->cursor_locked_to_player = true;
    game->cursor_tile_x = player_tile_x;
    game->cursor_tile_y = player_tile_y;
}

static void game_announce_cursor_tile(Game *game, bool interrupt)
{
    if (game == NULL || !game->world_loaded)
    {
        return;
    }

    int player_tile_x = 0;
    int player_tile_y = 0;
    if (game_get_player_tile_position(game, &player_tile_x, &player_tile_y) &&
        game->cursor_tile_x == player_tile_x &&
        game->cursor_tile_y == player_tile_y)
    {
        game_announce("Player.", interrupt);
        return;
    }

    const TileDefinition *tile =
        game_get_tile_at(&game->world, game->cursor_tile_x, game->cursor_tile_y);
    if (tile == NULL)
    {
        return;
    }

    char message[160];
    snprintf(message, sizeof(message), "%s.",
             tile->name != NULL ? tile->name : "Unknown");
    game_announce(message, interrupt);
}

static void game_sync_cursor_to_player_if_locked(Game *game)
{
    if (game == NULL || !game->cursor_locked_to_player)
    {
        return;
    }

    game_recenter_cursor(game);
}

static void game_announce_cursor_distance(Game *game)
{
    if (game == NULL || !game->world_loaded)
    {
        return;
    }

    int player_tile_x = 0;
    int player_tile_y = 0;
    if (!game_get_player_tile_position(game, &player_tile_x, &player_tile_y))
    {
        game_announce("Unavailable.", true);
        return;
    }

    char message[96];
    if (!tile_distance_format_cardinal(
            tile_distance_offset(player_tile_x, player_tile_y,
                                 game->cursor_tile_x, game->cursor_tile_y),
            message, sizeof(message)))
    {
        game_announce("Unavailable.", true);
        return;
    }

    game_announce(message, true);
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

static bool game_tile_pickup_allowed_without_tools(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return false;
    }

    const TileCategory category = tile_category_for_definition(tile);
    return category == TILE_CATEGORY_FURNITURE;
}

static void game_announce_pickup_requirement(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        game_announce("Nothing to pick up.", true);
        return;
    }

    const TileCategory category = tile_category_for_definition(tile);
    if (category == TILE_CATEGORY_TREES)
    {
        game_announce("Trees need tools before pickup. Not implemented yet.", true);
        return;
    }

    if (category == TILE_CATEGORY_ROCKS)
    {
        game_announce("Rocks need tools before pickup. Not implemented yet.", true);
        return;
    }

    if (category == TILE_CATEGORY_TERRAIN)
    {
        game_announce("Ground pieces need tools before pickup. Not implemented yet.", true);
        return;
    }

    game_announce("This tile cannot be picked up right now.", true);
}

static bool game_try_pickup_near_player(Game *game)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return false;
    }

    int player_tile_x = 0;
    int player_tile_y = 0;
    if (!game_get_player_tile_position(game, &player_tile_x, &player_tile_y))
    {
        return false;
    }
    static const int k_offsets[][2] = {
        {0, 0},
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
        {1, 1},
        {-1, 1},
        {1, -1},
        {-1, -1},
    };

    const TileDefinition *first_unavailable_tile = NULL;

    for (int i = 0; i < (int)(sizeof(k_offsets) / sizeof(k_offsets[0])); i++)
    {
        const int tile_x = player_tile_x + k_offsets[i][0];
        const int tile_y = player_tile_y + k_offsets[i][1];
        const TileDefinition *tile = game_get_tile_at(&game->world, tile_x, tile_y);
        if (tile == NULL)
        {
            continue;
        }

        if (!game_tile_pickup_allowed_without_tools(tile))
        {
            if (first_unavailable_tile == NULL)
            {
                first_unavailable_tile = tile;
            }
            continue;
        }

        if (!inventory_add_survival(&game->inventory, tile->id, 1))
        {
            game_announce("Inventory full.", true);
            return false;
        }

        game_set_world_tile_if_in_bounds(&game->world, tile_x, tile_y, TILE_GRASS);
        char message[160];
        snprintf(message, sizeof(message), "Picked up %s.",
                 tile->name != NULL ? tile->name : "item");
        game_announce(message, true);
        return true;
    }

    if (first_unavailable_tile != NULL)
    {
        game_announce_pickup_requirement(first_unavailable_tile);
    }
    else
    {
        game_announce("No pickup item nearby.", true);
    }

    return false;
}

static void game_place_opening_wreckage(Game *game)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return;
    }

    int spawn_x = 0;
    int spawn_y = 0;
    if (!game_get_player_tile_position(game, &spawn_x, &spawn_y))
    {
        return;
    }

    for (int offset_y = -2; offset_y <= 2; offset_y++)
    {
        for (int offset_x = -2; offset_x <= 2; offset_x++)
        {
            game_set_world_tile_if_in_bounds(&game->world, spawn_x + offset_x, spawn_y + offset_y,
                                             TILE_SHIPPIECE);
        }
    }

    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 1, spawn_y, TILE_SMALLAXE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 2, spawn_y, TILE_PICKAXE);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x + 1, spawn_y + 1, TILE_READER);
    game_set_world_tile_if_in_bounds(&game->world, spawn_x, spawn_y + 1, TILE_RADIO);
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

static int game_find_survival_slot_for_tile(const Inventory *inventory, TileId tile_id)
{
    if (inventory == NULL || inventory->mode != GAME_MODE_SURVIVAL ||
        tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return -1;
    }

    for (int slot_index = 0; slot_index < INVENTORY_SURVIVAL_SLOT_COUNT; slot_index++)
    {
        const InventorySlot *slot = &inventory->slots[slot_index];
        if (slot->occupied && slot->tile_id == (int)tile_id)
        {
            return slot_index;
        }
    }

    return -1;
}

static void game_handle_survival_inventory_selection_slot(Game *game, int selected_slot)
{
    if (game == NULL || selected_slot < 0 || selected_slot >= INVENTORY_SURVIVAL_SLOT_COUNT)
    {
        return;
    }

    InventorySlot *slot = &game->inventory.slots[selected_slot];
    if (!slot->occupied || slot->tile_id < 0 || slot->tile_id >= TILE_ID_COUNT)
    {
        return;
    }

    const TileId selected_tile = (TileId)slot->tile_id;
    game->inventory.selected_tile = selected_tile;
    game->pending_hotbar_tile = selected_tile;

    if (game->pending_inventory_slot < 0)
    {
        game->pending_inventory_slot = selected_slot;
        const TileDefinition *tile = tiles_get_definition(selected_tile);
        char message[256];
        snprintf(message, sizeof(message),
                 "%s selected. Enter on another item to swap, or press 1 through 9 for hotbar.",
                 tile != NULL && tile->name != NULL ? tile->name : "Item");
        game_announce(message, true);
        return;
    }

    if (game->pending_inventory_slot == selected_slot)
    {
        game_clear_pending_inventory_item(game);
        game_announce("Selection cancelled.", true);
        return;
    }

    InventorySlot temp = game->inventory.slots[game->pending_inventory_slot];
    game->inventory.slots[game->pending_inventory_slot] = game->inventory.slots[selected_slot];
    game->inventory.slots[selected_slot] = temp;

    game->pending_inventory_slot = -1;
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game_announce("Items swapped.", true);
}

static void game_clear_pending_inventory_item(Game *game)
{
    if (game == NULL)
    {
        return;
    }

    game->pending_inventory_slot = -1;
    game->pending_hotbar_tile = TILE_ID_COUNT;
}

static void game_handle_survival_inventory_selection(Game *game, TileId selected_tile)
{
    if (game == NULL || selected_tile < 0 || selected_tile >= TILE_ID_COUNT)
    {
        return;
    }

    const int selected_slot = game_find_survival_slot_for_tile(&game->inventory, selected_tile);
    if (selected_slot < 0)
    {
        return;
    }

    game->inventory.selected_tile = selected_tile;
    game->pending_hotbar_tile = selected_tile;

    if (game->pending_inventory_slot < 0)
    {
        game->pending_inventory_slot = selected_slot;
        const TileDefinition *tile = tiles_get_definition(selected_tile);
        char message[256];
        snprintf(message, sizeof(message),
                 "%s selected. Enter on another item to swap, or press 1 through 9 for hotbar.",
                 tile != NULL && tile->name != NULL ? tile->name : "Item");
        game_announce(message, true);
        return;
    }

    if (game->pending_inventory_slot == selected_slot)
    {
        game_clear_pending_inventory_item(game);
        game_announce("Selection cancelled.", true);
        return;
    }

    InventorySlot temp = game->inventory.slots[game->pending_inventory_slot];
    game->inventory.slots[game->pending_inventory_slot] = game->inventory.slots[selected_slot];
    game->inventory.slots[selected_slot] = temp;

    game->pending_inventory_slot = -1;
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game_announce("Items swapped.", true);
}

static void game_assign_pending_hotbar_tile(Game *game, int slot_index)
{
    if (game == NULL ||
        slot_index < 0 || slot_index >= INVENTORY_HOTBAR_SLOT_COUNT ||
        game->pending_hotbar_tile < 0 || game->pending_hotbar_tile >= TILE_ID_COUNT)
    {
        return;
    }

    if (!inventory_assign_hotbar_slot(&game->inventory, slot_index, game->pending_hotbar_tile))
    {
        return;
    }

    const TileDefinition *tile = tiles_get_definition((TileId)game->pending_hotbar_tile);
    char message[192];
    snprintf(message, sizeof(message), "%s assigned to hotbar slot %d.",
             tile != NULL && tile->name != NULL ? tile->name : "Item",
             slot_index + 1);
    game_announce(message, true);
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
}

static void game_select_hotbar_slot(Game *game, int slot_index, bool announce_if_empty)
{
    if (game == NULL ||
        slot_index < 0 || slot_index >= INVENTORY_HOTBAR_SLOT_COUNT)
    {
        return;
    }

    if (!inventory_select_hotbar_slot(&game->inventory, slot_index))
    {
        return;
    }

    const int tile_id = inventory_hotbar_tile(&game->inventory, slot_index);
    if (tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        if (announce_if_empty)
        {
            char message[128];
            snprintf(message, sizeof(message), "Hotbar slot %d is empty.", slot_index + 1);
            game_announce(message, true);
        }
        return;
    }

    const TileDefinition *tile = tiles_get_definition((TileId)tile_id);
    char message[192];
    snprintf(message, sizeof(message), "%s, hotbar slot %d selected.",
             tile != NULL && tile->name != NULL ? tile->name : "Unknown",
             slot_index + 1);
    game_announce(message, true);
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
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
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
    game->prev_j = false;
    game->prev_k = false;
    game->prev_page_up = false;
    game->prev_page_down = false;
    game->prev_home = false;
    game->prev_tab = false;
    game->prev_space = false;
    game->prev_e = false;
    game->prev_right_bracket = false;
    game->prev_mouse_right = false;
    for (int i = 0; i < INVENTORY_HOTBAR_SLOT_COUNT; i++)
    {
        game->prev_hotbar_keys[i] = false;
    }
    game->has_prev_tile = false;
    game->prev_tile_x = -1;
    game->prev_tile_y = -1;
    game->cursor_locked_to_player = true;
    game->cursor_tile_x = -1;
    game->cursor_tile_y = -1;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
    game->game_mode = GAME_MODE_SURVIVAL;
    inventory_init(&game->inventory, GAME_MODE_SURVIVAL);

    game->speech_ready = speech_init();
    if (!settings_load())
    {
        fprintf(stderr, "game: settings load failed\n");
    }

    if (!audio_backend_init())
    {
        fprintf(stderr, "game: miniaudio backend failed to start\n");
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
    const bool j_now = engine_key_down(engine, SDL_SCANCODE_J);
    const bool k_now = engine_key_down(engine, SDL_SCANCODE_K);
    const bool page_up_now = engine_key_down(engine, SDL_SCANCODE_PAGEUP);
    const bool page_down_now = engine_key_down(engine, SDL_SCANCODE_PAGEDOWN);
    const bool home_now = engine_key_down(engine, SDL_SCANCODE_HOME);
    const bool tab_now = engine_key_down(engine, SDL_SCANCODE_TAB);
    const bool space_now = engine_key_down(engine, SDL_SCANCODE_SPACE);
    const bool e_now = engine_key_down(engine, SDL_SCANCODE_E);
    const bool right_bracket_now = engine_key_down(engine, SDL_SCANCODE_RIGHTBRACKET);
    bool hotbar_keys_now[INVENTORY_HOTBAR_SLOT_COUNT] = {
        engine_key_down(engine, SDL_SCANCODE_1),
        engine_key_down(engine, SDL_SCANCODE_2),
        engine_key_down(engine, SDL_SCANCODE_3),
        engine_key_down(engine, SDL_SCANCODE_4),
        engine_key_down(engine, SDL_SCANCODE_5),
        engine_key_down(engine, SDL_SCANCODE_6),
        engine_key_down(engine, SDL_SCANCODE_7),
        engine_key_down(engine, SDL_SCANCODE_8),
        engine_key_down(engine, SDL_SCANCODE_9),
    };
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    (void)mouse_x;
    (void)mouse_y;
    const bool mouse_right_now =
        (mouse_buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
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
    const bool j_pressed = j_now && !game->prev_j;
    const bool k_pressed = k_now && !game->prev_k;
    const bool page_up_pressed = page_up_now && !game->prev_page_up;
    const bool page_down_pressed = page_down_now && !game->prev_page_down;
    const bool home_pressed = home_now && !game->prev_home;
    const bool tab_pressed = tab_now && !game->prev_tab;
    const bool space_pressed = space_now && !game->prev_space;
    const bool e_pressed = e_now && !game->prev_e;
    const bool right_bracket_pressed =
        right_bracket_now && !game->prev_right_bracket;
    const bool mouse_right_pressed = mouse_right_now && !game->prev_mouse_right;
    const bool next_container_pressed = tab_pressed && !shift_now;
    const bool previous_container_pressed = tab_pressed && shift_now;
    int pressed_hotbar_index = -1;
    for (int i = 0; i < INVENTORY_HOTBAR_SLOT_COUNT; i++)
    {
        if (hotbar_keys_now[i] && !game->prev_hotbar_keys[i])
        {
            pressed_hotbar_index = i;
            break;
        }
    }

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
    game->prev_j = j_now;
    game->prev_k = k_now;
    game->prev_page_up = page_up_now;
    game->prev_page_down = page_down_now;
    game->prev_home = home_now;
    game->prev_tab = tab_now;
    game->prev_space = space_now;
    game->prev_e = e_now;
    game->prev_right_bracket = right_bracket_now;
    game->prev_mouse_right = mouse_right_now;
    for (int i = 0; i < INVENTORY_HOTBAR_SLOT_COUNT; i++)
    {
        game->prev_hotbar_keys[i] = hotbar_keys_now[i];
    }

    UiAction action = UI_ACTION_NONE;
    ui_survival_inventory_set_inventory(&game->inventory);
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
            game_recenter_cursor(game);
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

    if (action == UI_ACTION_SELECT_CREATIVE_TILE ||
        action == UI_ACTION_SELECT_SURVIVAL_TILE)
    {
        const char *label = ui_focused_widget_label(&game->ui);
        const TileId selected_tile = tiles_find_by_name(label);
        if (selected_tile != TILE_ID_COUNT)
        {
            if (action == UI_ACTION_SELECT_SURVIVAL_TILE)
            {
                const int focused_slot = ui_focused_widget_user_data(&game->ui);
                if (focused_slot >= 0)
                {
                    game_handle_survival_inventory_selection_slot(game, focused_slot);
                }
                else
                {
                    game_handle_survival_inventory_selection(game, selected_tile);
                }
            }
            else
            {
                game->inventory.selected_tile = selected_tile;
                game->pending_hotbar_tile = selected_tile;
                game->pending_inventory_slot = -1;
                char message[256];
                snprintf(message, sizeof(message),
                         "%s selected. Press 1 through 9 to assign hotbar.",
                         label != NULL ? label : "Tile");
                game_announce(message, true);
            }
        }
        else if (action == UI_ACTION_SELECT_SURVIVAL_TILE)
        {
            game_clear_pending_inventory_item(game);
            game_announce("Selection cancelled.", true);
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
                ui_show_screen(&game->ui, UI_SCREEN_SURVIVAL_INVENTORY,
                               game->speech_ready ? game_announce : NULL);
            }
        }
        else if (screen == UI_SCREEN_CREATIVE_INVENTORY ||
                 screen == UI_SCREEN_SURVIVAL_INVENTORY)
        {
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
            game_clear_pending_inventory_item(game);
        }
    }

    const UiScreen current_screen = ui_screen(&game->ui);
    const bool in_inventory_screen =
        current_screen == UI_SCREEN_CREATIVE_INVENTORY ||
        current_screen == UI_SCREEN_SURVIVAL_INVENTORY;
    if (!in_inventory_screen && game->pending_inventory_slot >= 0)
    {
        game_clear_pending_inventory_item(game);
    }

    if (pressed_hotbar_index >= 0)
    {
        if (in_inventory_screen && game->pending_hotbar_tile >= 0 &&
            game->pending_hotbar_tile < TILE_ID_COUNT)
        {
            game_assign_pending_hotbar_tile(game, pressed_hotbar_index);
        }
        else if (current_screen == UI_SCREEN_WORLD && game->world_loaded)
        {
            game_select_hotbar_slot(game, pressed_hotbar_index, true);
        }
    }

    if ((right_bracket_pressed || mouse_right_pressed) &&
        current_screen == UI_SCREEN_WORLD && game->world_loaded &&
        game->game_mode == GAME_MODE_SURVIVAL && !opening_scene_is_active())
    {
        game_try_pickup_near_player(game);
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
        game_sync_cursor_to_player_if_locked(game);
        game->has_prev_blocked_tile = false;
        return;
    }

    if (up_pressed)
    {
        game_move_cursor(game, 0, -1);
    }
    if (down_pressed)
    {
        game_move_cursor(game, 0, 1);
    }
    if (left_pressed)
    {
        game_move_cursor(game, -1, 0);
    }
    if (right_pressed)
    {
        game_move_cursor(game, 1, 0);
    }

    if (j_pressed)
    {
        game_recenter_cursor(game);
    }

    float move_x = 0.0f;
    float move_y = 0.0f;

    if (engine_key_down(engine, SDL_SCANCODE_W))
    {
        move_y -= 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_S))
    {
        move_y += 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_A))
    {
        move_x -= 1.0f;
    }

    if (engine_key_down(engine, SDL_SCANCODE_D))
    {
        move_x += 1.0f;
    }

    const float player_x_before = game->world.player_x;
    const float player_y_before = game->world.player_y;
    world_update(&game->world, delta_time, move_x, move_y, space_pressed);
    water_biome_audio_update(&game->world, delta_time);
    game_sync_cursor_to_player_if_locked(game);

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

    int player_tile_x = 0;
    int player_tile_y = 0;
    const TileDefinition *player_tile = NULL;
    const BiomeDefinition *biome = NULL;
    float temperature_c = 0.0f;
    const bool has_tile = world_get_player_environment(&game->world, &player_tile_x, &player_tile_y,
                                                       &player_tile, &biome, &temperature_c);
    if (game_get_tile_at(&game->world, game->cursor_tile_x, game->cursor_tile_y) != NULL)
    {
        const bool tile_changed = !game->has_prev_tile ||
                                  game->cursor_tile_x != game->prev_tile_x ||
                                  game->cursor_tile_y != game->prev_tile_y;
        if (tile_changed)
        {
            game_announce_cursor_tile(game, true);
            game->prev_tile_x = game->cursor_tile_x;
            game->prev_tile_y = game->cursor_tile_y;
            game->has_prev_tile = true;
        }
    }

    if (c_pressed)
    {
        char message[160];
        if (has_tile && alt_down)
        {
            snprintf(message, sizeof(message), "%s.",
                     player_tile != NULL && player_tile->name != NULL ? player_tile->name : "Unknown");
        }
        else if (has_tile)
        {
            snprintf(message, sizeof(message), "X %d Y %d.", player_tile_x, player_tile_y);
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

    if (k_pressed)
    {
        game_announce_cursor_distance(game);
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
    audio_backend_shutdown();
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
