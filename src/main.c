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
#include "structure_builder.h"
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
    bool structure_builder_active;
    StructureBuilderConfig structure_builder_config;

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
    bool prev_left_bracket;
    bool prev_right_bracket;
    bool prev_mouse_right;
    bool prev_hotbar_keys[INVENTORY_HOTBAR_SLOT_COUNT];

    bool has_prev_tile;
    int prev_tile_x;
    int prev_tile_y;
    bool has_prev_blocked_tile;
    int prev_blocked_tile_x;
    int prev_blocked_tile_y;
    int facing_dir_x;
    int facing_dir_y;

    int tracker_category_index;
    int tracker_object_index;
    bool builder_fill_mode;
    bool builder_fill_anchor_set;
    int builder_fill_anchor_x;
    int builder_fill_anchor_y;
    int builder_shape_mode;
    bool auto_walk_active;
    int auto_walk_target_x;
    int auto_walk_target_y;
    float auto_walk_stuck_seconds;
    bool axe_chopping;
    int axe_target_x;
    int axe_target_y;
    float axe_chop_seconds;
    int pending_hotbar_tile;
    int pending_inventory_slot;
} Game;

typedef enum BuilderShapeMode
{
    BUILDER_SHAPE_RECT_FILL = 0,
    BUILDER_SHAPE_RECT_OUTLINE,
    BUILDER_SHAPE_LINE,
    BUILDER_SHAPE_COUNT
} BuilderShapeMode;

static const TileCategory k_tracker_categories[] = {
    TILE_CATEGORY_TREES,
    TILE_CATEGORY_PLANTS,
    TILE_CATEGORY_ROCKS,
    TILE_CATEGORY_FURNITURE,
    TILE_CATEGORY_TOOLS,
    TILE_CATEGORY_WATER,
    TILE_CATEGORY_STRUCTURES,
    TILE_CATEGORY_TERRAIN,
    TILE_CATEGORY_MISC,
};

static const int k_tracker_category_count =
    (int)(sizeof(k_tracker_categories) / sizeof(k_tracker_categories[0]));

static const char *const k_builder_shape_mode_names[BUILDER_SHAPE_COUNT] = {
    "filled rectangle",
    "rectangle outline",
    "line",
};

static const float k_axe_chop_required_seconds = 3.0f;
static const int k_tree_wood_min_yield = 15;
static const int k_tree_wood_max_yield = 20;
static const int k_tree_wood_auto_pickup_min = 12;
static const int k_tree_wood_auto_pickup_max = 15;

static void game_clear_pending_inventory_item(Game *game);
static void game_reset_builder_shape_state(Game *game);
static bool game_tile_is_wall_like(const TileDefinition *tile);
static void game_apply_structure_builder_biome(Game *game, BiomeType biome_type);

static void game_announce(const char *text, bool interrupt)
{
    if (!speech_say(text, interrupt))
    {
        speech_output(text, interrupt);
    }
}

static const TileDefinition *game_get_tile_at(const World *world, int tile_x, int tile_y)
{
    return world_get_top_tile_at(world, tile_x, tile_y);
}

static bool game_is_feature_blocking_tile(const TileDefinition *tile)
{
    return tile != NULL &&
           tile->blocks_land_movement &&
           (tile->layer == TILE_LAYER_OBJECT || tile->layer == TILE_LAYER_STRUCTURE);
}

static void game_set_world_tile_if_in_bounds(World *world, int tile_x, int tile_y, TileId tile_id)
{
    if (world == NULL || tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return;
    }

    if (!world_set_tile(world, tile_x, tile_y, tile_id))
    {
        return;
    }
}

static void game_clear_world_tile_if_in_bounds(World *world,
                                               int tile_x,
                                               int tile_y,
                                               const TileDefinition *tile)
{
    if (world == NULL || tile == NULL)
    {
        return;
    }

    world_clear_tile_at_layer(world, tile_x, tile_y, tile->layer);
}

static bool game_get_builder_target_tile(const Game *game, int *out_x, int *out_y)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0 ||
        out_x == NULL || out_y == NULL)
    {
        return false;
    }

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
    int facing_x = game->facing_dir_x;
    int facing_y = game->facing_dir_y;
    if (facing_x == 0 && facing_y == 0)
    {
        facing_y = 1;
    }

    *out_x = player_tile_x + facing_x;
    *out_y = player_tile_y + facing_y;
    return world_is_in_bounds(&game->world, *out_x, *out_y);
}

static void game_reset_builder_shape_state(Game *game)
{
    if (game == NULL)
    {
        return;
    }

    game->builder_fill_mode = false;
    game->builder_fill_anchor_set = false;
    game->builder_fill_anchor_x = -1;
    game->builder_fill_anchor_y = -1;
    game->builder_shape_mode = BUILDER_SHAPE_RECT_FILL;
}

static bool game_get_builder_selected_tile(const Game *game,
                                           TileId *out_tile_id,
                                           const TileDefinition **out_tile)
{
    if (game == NULL || out_tile_id == NULL || out_tile == NULL)
    {
        return false;
    }

    const TileId tile_id = (TileId)game->inventory.selected_tile;
    const TileDefinition *tile = tiles_get_definition(tile_id);
    if (tile == NULL || tile->layer == TILE_LAYER_UNKNOWN || tile->layer >= TILE_LAYER_COUNT)
    {
        return false;
    }

    *out_tile_id = tile_id;
    *out_tile = tile;
    return true;
}

static int game_abs_int(int value)
{
    return value < 0 ? -value : value;
}

static int game_builder_line_steps(int start_x, int start_y, int end_x, int end_y)
{
    const int dx = game_abs_int(end_x - start_x);
    const int dy = game_abs_int(end_y - start_y);
    return dx > dy ? dx : dy;
}

static int game_apply_builder_shape(World *world,
                                    TileId tile_id,
                                    BuilderShapeMode shape_mode,
                                    int start_x,
                                    int start_y,
                                    int end_x,
                                    int end_y)
{
    if (world == NULL || tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return 0;
    }

    int placed_count = 0;
    const int min_x = start_x < end_x ? start_x : end_x;
    const int max_x = start_x > end_x ? start_x : end_x;
    const int min_y = start_y < end_y ? start_y : end_y;
    const int max_y = start_y > end_y ? start_y : end_y;

    switch (shape_mode)
    {
    case BUILDER_SHAPE_RECT_FILL:
        for (int y = min_y; y <= max_y; y++)
        {
            for (int x = min_x; x <= max_x; x++)
            {
                if (world_set_tile(world, x, y, tile_id))
                {
                    placed_count++;
                }
            }
        }
        break;
    case BUILDER_SHAPE_RECT_OUTLINE:
        for (int y = min_y; y <= max_y; y++)
        {
            for (int x = min_x; x <= max_x; x++)
            {
                if (x != min_x && x != max_x && y != min_y && y != max_y)
                {
                    continue;
                }

                if (world_set_tile(world, x, y, tile_id))
                {
                    placed_count++;
                }
            }
        }
        break;
    case BUILDER_SHAPE_LINE:
    {
        const int steps = game_builder_line_steps(start_x, start_y, end_x, end_y);
        if (steps == 0)
        {
            if (world_set_tile(world, start_x, start_y, tile_id))
            {
                placed_count++;
            }
            break;
        }

        for (int step = 0; step <= steps; step++)
        {
            const float progress = (float)step / (float)steps;
            const int x = (int)SDL_roundf(start_x + (end_x - start_x) * progress);
            const int y = (int)SDL_roundf(start_y + (end_y - start_y) * progress);
            if (world_set_tile(world, x, y, tile_id))
            {
                placed_count++;
            }
        }
        break;
    }
    case BUILDER_SHAPE_COUNT:
    default:
        break;
    }

    return placed_count;
}

static bool game_find_walkable_builder_tile(const World *world,
                                            int min_x,
                                            int min_y,
                                            int max_x,
                                            int max_y,
                                            bool interior_only,
                                            int *out_x,
                                            int *out_y)
{
    if (world == NULL || out_x == NULL || out_y == NULL)
    {
        return false;
    }

    int search_min_x = min_x;
    int search_min_y = min_y;
    int search_max_x = max_x;
    int search_max_y = max_y;
    if (interior_only)
    {
        search_min_x++;
        search_min_y++;
        search_max_x--;
        search_max_y--;
    }

    if (search_min_x > search_max_x || search_min_y > search_max_y)
    {
        return false;
    }

    const int center_x = (search_min_x + search_max_x) / 2;
    const int center_y = (search_min_y + search_max_y) / 2;
    const int radius_x = search_max_x - search_min_x;
    const int radius_y = search_max_y - search_min_y;
    const int max_radius = radius_x > radius_y ? radius_x : radius_y;

    for (int radius = 0; radius <= max_radius; radius++)
    {
        for (int y = center_y - radius; y <= center_y + radius; y++)
        {
            for (int x = center_x - radius; x <= center_x + radius; x++)
            {
                if (x < search_min_x || x > search_max_x || y < search_min_y || y > search_max_y)
                {
                    continue;
                }

                bool can_swim = false;
                if (!world_can_occupy_tile(world, x, y, &can_swim, NULL, NULL) || can_swim)
                {
                    continue;
                }

                *out_x = x;
                *out_y = y;
                return true;
            }
        }
    }

    return false;
}

static void game_move_builder_player_to_tile(Game *game, int tile_x, int tile_y)
{
    if (game == NULL || game->world.tile_size <= 0)
    {
        return;
    }

    game->world.player_x = (float)(tile_x * game->world.tile_size);
    game->world.player_y = (float)(tile_y * game->world.tile_size);
}

static void game_place_builder_player_inside_wall(Game *game,
                                                  BuilderShapeMode shape_mode,
                                                  int start_x,
                                                  int start_y,
                                                  int end_x,
                                                  int end_y)
{
    if (game == NULL)
    {
        return;
    }

    const int min_x = start_x < end_x ? start_x : end_x;
    const int max_x = start_x > end_x ? start_x : end_x;
    const int min_y = start_y < end_y ? start_y : end_y;
    const int max_y = start_y > end_y ? start_y : end_y;

    int destination_x = 0;
    int destination_y = 0;
    if (shape_mode != BUILDER_SHAPE_LINE &&
        game_find_walkable_builder_tile(&game->world, min_x, min_y, max_x, max_y, true,
                                        &destination_x, &destination_y))
    {
        game_move_builder_player_to_tile(game, destination_x, destination_y);
        return;
    }

    if (game_find_walkable_builder_tile(&game->world, min_x, min_y, max_x, max_y, false,
                                        &destination_x, &destination_y))
    {
        game_move_builder_player_to_tile(game, destination_x, destination_y);
    }
}

static bool game_tile_is_wall_like(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return false;
    }

    switch (tile->id)
    {
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
        return true;
    case TILE_ID_COUNT:
    default:
        return false;
    }
}

static void game_apply_structure_builder_biome(Game *game, BiomeType biome_type)
{
    if (game == NULL || !game->structure_builder_active || !game->world_loaded)
    {
        return;
    }

    const BiomeType previous_biome = game->structure_builder_config.builder_biome;
    const TileId previous_tile = structure_builder_biome_primary_tile(previous_biome);
    const TileId next_tile = structure_builder_biome_primary_tile(biome_type);
    const BiomeDefinition *biome = biome_get_definition(biome_type);
    const float next_temperature_c =
        biome != NULL ? (biome->min_temperature_c + biome->max_temperature_c) * 0.5f : 18.0f;

    game->structure_builder_config.builder_biome = biome_type;
    for (int biome_index = 0; biome_index < BIOME_TYPE_COUNT; biome_index++)
    {
        game->structure_builder_config.allowed_biomes[biome_index] =
            biome_index == (int)biome_type;
    }
    structure_builder_set_allowed_supports_for_biome(&game->structure_builder_config, biome_type);

    for (int y = 0; y < game->world.height; y++)
    {
        for (int x = 0; x < game->world.width; x++)
        {
            const int index = (y * game->world.width) + x;
            game->world.biomes[index] = biome_type;
            game->world.temperatures_c[index] = next_temperature_c;

            if (world_get_tile_at_layer(&game->world, x, y, TILE_LAYER_FLOOR) != NULL ||
                world_get_tile_at_layer(&game->world, x, y, TILE_LAYER_OBJECT) != NULL ||
                world_get_tile_at_layer(&game->world, x, y, TILE_LAYER_STRUCTURE) != NULL)
            {
                continue;
            }

            if (world_get_tile_id_at_layer(&game->world, x, y, TILE_LAYER_GROUND) == previous_tile)
            {
                world_set_tile_at_layer(&game->world, x, y, TILE_LAYER_GROUND, next_tile);
            }
        }
    }
}

static void game_cycle_structure_builder_biome(Game *game, int direction)
{
    if (game == NULL || !game->structure_builder_active)
    {
        return;
    }

    int next_biome = (int)game->structure_builder_config.builder_biome + direction;
    while (next_biome < 0)
    {
        next_biome += BIOME_TYPE_COUNT;
    }
    while (next_biome >= BIOME_TYPE_COUNT)
    {
        next_biome -= BIOME_TYPE_COUNT;
    }

    game_apply_structure_builder_biome(game, (BiomeType)next_biome);

    const BiomeDefinition *biome = biome_get_definition((BiomeType)next_biome);
    char message[192];
    snprintf(message, sizeof(message),
             "Builder biome set to %s. Allowed biomes and support terrain updated for this biome.",
             biome != NULL && biome->name != NULL ? biome->name : "Biome");
    game_announce(message, true);
}

static void game_cycle_builder_shape_mode(Game *game, int direction)
{
    if (game == NULL)
    {
        return;
    }

    int next_mode = game->builder_shape_mode + direction;
    while (next_mode < 0)
    {
        next_mode += BUILDER_SHAPE_COUNT;
    }
    while (next_mode >= BUILDER_SHAPE_COUNT)
    {
        next_mode -= BUILDER_SHAPE_COUNT;
    }
    game->builder_shape_mode = next_mode;

    char message[192];
    snprintf(message, sizeof(message), "Builder shape set to %s.",
             k_builder_shape_mode_names[game->builder_shape_mode]);
    game_announce(message, true);
}

static void game_toggle_builder_fill_mode(Game *game)
{
    if (game == NULL || !game->structure_builder_active)
    {
        return;
    }

    game->builder_fill_mode = !game->builder_fill_mode;
    game->builder_fill_anchor_set = false;
    game->builder_fill_anchor_x = -1;
    game->builder_fill_anchor_y = -1;

    if (game->builder_fill_mode)
    {
        char message[256];
        snprintf(message, sizeof(message),
                 "Builder fill mode on. Shape: %s. Press Enter to set the first corner, then Enter again to apply. Use left and right bracket to change shape.",
                 k_builder_shape_mode_names[game->builder_shape_mode]);
        game_announce(message, true);
    }
    else
    {
        game_announce("Builder fill mode off. Enter places one tile again.", true);
    }
}

static bool game_place_selected_tile_in_builder(Game *game)
{
    if (game == NULL || !game->structure_builder_active)
    {
        return false;
    }

    TileId tile_id = TILE_ID_COUNT;
    const TileDefinition *tile = NULL;
    if (!game_get_builder_selected_tile(game, &tile_id, &tile))
    {
        game_announce("That tile cannot be placed in the builder.", true);
        return false;
    }

    int tile_x = 0;
    int tile_y = 0;
    if (!game_get_builder_target_tile(game, &tile_x, &tile_y))
    {
        game_announce("Builder target is out of bounds.", true);
        return false;
    }

    if (game->builder_fill_mode)
    {
        if (!game->builder_fill_anchor_set)
        {
            game->builder_fill_anchor_set = true;
            game->builder_fill_anchor_x = tile_x;
            game->builder_fill_anchor_y = tile_y;

            char message[224];
            snprintf(message, sizeof(message),
                     "First corner set at X %d Y %d. Move to the second point and press Enter to apply the %s.",
                     tile_x, tile_y, k_builder_shape_mode_names[game->builder_shape_mode]);
            game_announce(message, true);
            return true;
        }

        const int placed_count =
            game_apply_builder_shape(&game->world,
                                     tile_id,
                                     (BuilderShapeMode)game->builder_shape_mode,
                                     game->builder_fill_anchor_x,
                                     game->builder_fill_anchor_y,
                                     tile_x,
                                     tile_y);
        if (placed_count <= 0)
        {
            game_announce("Builder fill failed.", true);
            return false;
        }

        char message[256];
        snprintf(message, sizeof(message),
                 "%s applied as a %s from X %d Y %d to X %d Y %d.",
                 tile->name != NULL ? tile->name : "Tile",
                 k_builder_shape_mode_names[game->builder_shape_mode],
                 game->builder_fill_anchor_x,
                 game->builder_fill_anchor_y,
                 tile_x,
                 tile_y);
        game_announce(message, true);
        if (game_tile_is_wall_like(tile))
        {
            game_place_builder_player_inside_wall(game,
                                                  (BuilderShapeMode)game->builder_shape_mode,
                                                  game->builder_fill_anchor_x,
                                                  game->builder_fill_anchor_y,
                                                  tile_x,
                                                  tile_y);
        }
        game->builder_fill_anchor_set = false;
        game->builder_fill_anchor_x = -1;
        game->builder_fill_anchor_y = -1;
        return true;
    }

    if (!world_set_tile(&game->world, tile_x, tile_y, tile_id))
    {
        game_announce("Builder placement failed.", true);
        return false;
    }

    char message[192];
    snprintf(message, sizeof(message), "%s placed at X %d Y %d.",
             tile->name != NULL ? tile->name : "Tile", tile_x, tile_y);
    game_announce(message, true);
    return true;
}

static bool game_clear_builder_target_tile(Game *game)
{
    if (game == NULL || !game->structure_builder_active)
    {
        return false;
    }

    int tile_x = 0;
    int tile_y = 0;
    if (!game_get_builder_target_tile(game, &tile_x, &tile_y))
    {
        game_announce("Builder target is out of bounds.", true);
        return false;
    }

    const TileDefinition *tile = world_get_top_tile_at(&game->world, tile_x, tile_y);
    if (tile == NULL)
    {
        game_announce("Nothing to remove there.", true);
        return false;
    }

    if (!world_clear_tile_at_layer(&game->world, tile_x, tile_y, tile->layer))
    {
        game_announce("Removal failed.", true);
        return false;
    }

    char message[192];
    snprintf(message, sizeof(message), "%s removed from X %d Y %d.",
             tile->name != NULL ? tile->name : "Tile", tile_x, tile_y);
    game_announce(message, true);
    return true;
}

static bool game_tile_pickup_allowed_without_tools(const TileDefinition *tile)
{
    if (tile == NULL)
    {
        return false;
    }

    const TileCategory category = tile_category_for_definition(tile);
    return category == TILE_CATEGORY_FURNITURE ||
           category == TILE_CATEGORY_TOOLS ||
           tile->id == TILE_WOOD;
}

static bool game_tile_is_tree(const TileDefinition *tile)
{
    return tile != NULL && tile_category_for_definition(tile) == TILE_CATEGORY_TREES;
}

static bool game_inventory_has_tile(const Inventory *inventory, TileId tile_id)
{
    if (inventory == NULL || tile_id < 0 || tile_id >= TILE_ID_COUNT)
    {
        return false;
    }

    if (inventory->mode == GAME_MODE_CREATIVE)
    {
        return true;
    }

    return inventory_tile_count(inventory, tile_id) > 0;
}

static bool game_selected_tool_is_axe(const Game *game)
{
    if (game == NULL)
    {
        return false;
    }

    return game->inventory.selected_tile == TILE_SMALLAXE &&
           game_inventory_has_tile(&game->inventory, TILE_SMALLAXE);
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
        game_announce("Select the SmallAxe and hold space beside the tree to cut it.", true);
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

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
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
        const TileDefinition *tile = world_get_top_tile_at(&game->world, tile_x, tile_y);
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

        game_clear_world_tile_if_in_bounds(&game->world, tile_x, tile_y, tile);
        char message[160];
        snprintf(message, sizeof(message), "Picked up %s. Added to inventory.",
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

static bool game_try_auto_pickup_wood_near_player(Game *game)
{
    if (game == NULL || !game->world_loaded || game->game_mode != GAME_MODE_SURVIVAL ||
        game->world.tile_size <= 0)
    {
        return false;
    }

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
    int picked_up = 0;

    for (int offset_y = -1; offset_y <= 1; offset_y++)
    {
        for (int offset_x = -1; offset_x <= 1; offset_x++)
        {
            const int tile_x = player_tile_x + offset_x;
            const int tile_y = player_tile_y + offset_y;
            const TileDefinition *tile =
                world_get_tile_at_layer(&game->world, tile_x, tile_y, TILE_LAYER_OBJECT);
            if (tile == NULL || tile->id != TILE_WOOD)
            {
                continue;
            }

            if (!inventory_add_survival(&game->inventory, TILE_WOOD, 1))
            {
                if (picked_up == 0)
                {
                    game_announce("Inventory full.", true);
                }
                return picked_up > 0;
            }

            game_clear_world_tile_if_in_bounds(&game->world, tile_x, tile_y, tile);
            picked_up++;
        }
    }

    if (picked_up > 0)
    {
        char message[128];
        snprintf(message, sizeof(message), "Picked up %d wood.", picked_up);
        game_announce(message, true);
        return true;
    }

    return false;
}

static bool game_find_axe_tree_target(const Game *game, int *out_x, int *out_y)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return false;
    }

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);

    const int facing_x = game->facing_dir_x;
    const int facing_y = game->facing_dir_y;
    if (facing_x != 0 || facing_y != 0)
    {
        const int target_x = player_tile_x + facing_x;
        const int target_y = player_tile_y + facing_y;
        if (game_tile_is_tree(game_get_tile_at(&game->world, target_x, target_y)))
        {
            if (out_x != NULL)
            {
                *out_x = target_x;
            }
            if (out_y != NULL)
            {
                *out_y = target_y;
            }
            return true;
        }
    }

    static const int k_offsets[][2] = {
        {0, -1},
        {1, 0},
        {0, 1},
        {-1, 0},
        {1, -1},
        {1, 1},
        {-1, 1},
        {-1, -1},
    };

    for (int i = 0; i < (int)(sizeof(k_offsets) / sizeof(k_offsets[0])); i++)
    {
        const int target_x = player_tile_x + k_offsets[i][0];
        const int target_y = player_tile_y + k_offsets[i][1];
        if (!game_tile_is_tree(game_get_tile_at(&game->world, target_x, target_y)))
        {
            continue;
        }

        if (out_x != NULL)
        {
            *out_x = target_x;
        }
        if (out_y != NULL)
        {
            *out_y = target_y;
        }
        return true;
    }

    return false;
}

static bool game_can_drop_wood_at(const Game *game, int tile_x, int tile_y)
{
    bool can_swim = false;
    const TileDefinition *support_tile = NULL;
    if (!world_can_occupy_tile(&game->world, tile_x, tile_y, &can_swim, &support_tile, NULL))
    {
        return false;
    }

    if (can_swim || support_tile == NULL ||
        world_get_tile_at_layer(&game->world, tile_x, tile_y, TILE_LAYER_OBJECT) != NULL ||
        world_get_tile_at_layer(&game->world, tile_x, tile_y, TILE_LAYER_STRUCTURE) != NULL)
    {
        return false;
    }

    const TileCategory category = tile_category_for_definition(support_tile);
    return category == TILE_CATEGORY_TERRAIN || support_tile->layer == TILE_LAYER_GROUND ||
           support_tile->layer == TILE_LAYER_FLOOR;
}

static int game_scatter_wood_drops(Game *game, int center_x, int center_y, int count)
{
    if (game == NULL || count <= 0)
    {
        return 0;
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
        {2, 0},
        {-2, 0},
        {0, 2},
        {0, -2},
    };

    int dropped = 0;
    for (int i = 0; i < (int)(sizeof(k_offsets) / sizeof(k_offsets[0])) && dropped < count; i++)
    {
        const int tile_x = center_x + k_offsets[i][0];
        const int tile_y = center_y + k_offsets[i][1];
        if (!game_can_drop_wood_at(game, tile_x, tile_y))
        {
            continue;
        }

        game_set_world_tile_if_in_bounds(&game->world, tile_x, tile_y, TILE_WOOD);
        dropped++;
    }

    return dropped;
}

static void game_reset_axe_chop(Game *game)
{
    if (game == NULL)
    {
        return;
    }

    game->axe_chopping = false;
    game->axe_target_x = -1;
    game->axe_target_y = -1;
    game->axe_chop_seconds = 0.0f;
}

static void game_complete_tree_chop(Game *game, int tree_x, int tree_y)
{
    const int total_yield =
        k_tree_wood_min_yield + (rand() % (k_tree_wood_max_yield - k_tree_wood_min_yield + 1));
    int auto_pickup =
        k_tree_wood_auto_pickup_min +
        (rand() % (k_tree_wood_auto_pickup_max - k_tree_wood_auto_pickup_min + 1));
    if (auto_pickup > total_yield)
    {
        auto_pickup = total_yield;
    }

    int added = 0;
    if (inventory_add_survival(&game->inventory, TILE_WOOD, auto_pickup))
    {
        added = auto_pickup;
    }

    const int intended_drops = total_yield - added;
    world_clear_tile_at_layer(&game->world, tree_x, tree_y, TILE_LAYER_OBJECT);
    const int dropped = game_scatter_wood_drops(game, tree_x, tree_y, intended_drops);

    if (dropped < intended_drops)
    {
        inventory_add_survival(&game->inventory, TILE_WOOD, intended_drops - dropped);
        added += intended_drops - dropped;
    }

    char message[192];
    snprintf(message, sizeof(message),
             "Tree cut down. %d wood added, %d wood dropped nearby.",
             added, dropped);
    game_announce(message, true);
    game_reset_axe_chop(game);
}

static bool game_update_axe_use(Game *game, bool use_tool_down, bool use_tool_pressed, float delta_time)
{
    if (game == NULL || !use_tool_down || game->game_mode != GAME_MODE_SURVIVAL)
    {
        game_reset_axe_chop(game);
        return false;
    }

    if (!game_selected_tool_is_axe(game))
    {
        if (use_tool_pressed)
        {
            game_announce("Select the SmallAxe to chop trees.", true);
        }
        game_reset_axe_chop(game);
        return false;
    }

    int target_x = 0;
    int target_y = 0;
    if (!game_find_axe_tree_target(game, &target_x, &target_y))
    {
        if (use_tool_pressed)
        {
            game_announce("No tree in axe range.", true);
        }
        game_reset_axe_chop(game);
        return true;
    }

    if (!game->axe_chopping || game->axe_target_x != target_x || game->axe_target_y != target_y)
    {
        game->axe_chopping = true;
        game->axe_target_x = target_x;
        game->axe_target_y = target_y;
        game->axe_chop_seconds = 0.0f;
        game_announce("Swinging axe. Hold space for 3 seconds.", true);
    }

    game->axe_chop_seconds += delta_time;
    if (game->axe_chop_seconds >= k_axe_chop_required_seconds)
    {
        game_complete_tree_chop(game, target_x, target_y);
    }

    return true;
}

static void game_place_opening_wreckage(Game *game)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return;
    }

    const int spawn_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int spawn_y = (int)(game->world.player_y / (float)game->world.tile_size);

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
    if (world == NULL || world->ground_tiles == NULL || world->width <= 0 ||
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
            static const TileLayer k_layers[] = {
                TILE_LAYER_GROUND,
                TILE_LAYER_FLOOR,
                TILE_LAYER_OBJECT,
                TILE_LAYER_STRUCTURE,
            };

            for (int layer_index = 0; layer_index < (int)(sizeof(k_layers) / sizeof(k_layers[0]));
                 layer_index++)
            {
                const TileDefinition *tile =
                    world_get_tile_at_layer(world, x, y, k_layers[layer_index]);
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

static bool game_get_tracker_target(const Game *game,
                                    int *out_total,
                                    int *out_x,
                                    int *out_y,
                                    const TileDefinition **out_tile)
{
    if (game == NULL || !game->world_loaded ||
        game->tracker_category_index < 0 ||
        game->tracker_category_index >= k_tracker_category_count)
    {
        return false;
    }

    const TileCategory category = k_tracker_categories[game->tracker_category_index];
    return game_find_object_in_category(&game->world, category, game->tracker_object_index,
                                        out_total, out_x, out_y, out_tile);
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

    const bool has_object =
        game_get_tracker_target(game, &total, &object_x, &object_y, &object_tile);

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
    if (game == NULL)
    {
        return;
    }

    int total = 0;
    int object_x = 0;
    int object_y = 0;
    const TileDefinition *object_tile = NULL;
    const bool has_object =
        game_get_tracker_target(game, &total, &object_x, &object_y, &object_tile);

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

static void game_cancel_auto_walk(Game *game, const char *message, bool interrupt)
{
    if (game == NULL)
    {
        return;
    }

    game->auto_walk_active = false;
    game->auto_walk_target_x = -1;
    game->auto_walk_target_y = -1;
    game->auto_walk_stuck_seconds = 0.0f;
    if (message != NULL && message[0] != '\0')
    {
        game_announce(message, interrupt);
    }
}

static bool game_start_auto_walk_to_tracker_target(Game *game)
{
    if (game == NULL || !game->world_loaded || game->world.tile_size <= 0)
    {
        return false;
    }

    int total = 0;
    int object_x = 0;
    int object_y = 0;
    const TileDefinition *object_tile = NULL;
    const bool has_object =
        game_get_tracker_target(game, &total, &object_x, &object_y, &object_tile);
    if (!has_object || total <= 0)
    {
        game_announce("Auto walk unavailable. No tracked object selected.", true);
        return false;
    }

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
    if (player_tile_x == object_x && player_tile_y == object_y)
    {
        game_announce("Already at tracked object.", true);
        return true;
    }

    game->auto_walk_active = true;
    game->auto_walk_target_x = object_x;
    game->auto_walk_target_y = object_y;
    game->auto_walk_stuck_seconds = 0.0f;

    char message[224];
    snprintf(message, sizeof(message), "Auto walk started to %s at X %d Y %d.",
             object_tile != NULL && object_tile->name != NULL ? object_tile->name : "object",
             object_x, object_y);
    game_announce(message, true);
    return true;
}

static void game_apply_auto_walk(Game *game,
                                 bool manual_move_input,
                                 float *move_x,
                                 float *move_y,
                                 bool *jump_pressed)
{
    if (game == NULL || !game->auto_walk_active || move_x == NULL || move_y == NULL ||
        jump_pressed == NULL)
    {
        return;
    }

    if (manual_move_input)
    {
        game_cancel_auto_walk(game, "Auto walk canceled.", true);
        return;
    }

    if (!game->world_loaded || game->world.tile_size <= 0)
    {
        game_cancel_auto_walk(game, NULL, false);
        return;
    }

    int total = 0;
    int object_x = 0;
    int object_y = 0;
    const TileDefinition *object_tile = NULL;
    const bool has_object =
        game_get_tracker_target(game, &total, &object_x, &object_y, &object_tile);
    if (!has_object || total <= 0)
    {
        game_cancel_auto_walk(game, "Auto walk target unavailable.", true);
        return;
    }

    game->auto_walk_target_x = object_x;
    game->auto_walk_target_y = object_y;

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
    if (player_tile_x == game->auto_walk_target_x &&
        player_tile_y == game->auto_walk_target_y)
    {
        game_cancel_auto_walk(game, "Reached tracked object.", true);
        return;
    }

    const float target_world_x = (float)(game->auto_walk_target_x * game->world.tile_size);
    const float target_world_y = (float)(game->auto_walk_target_y * game->world.tile_size);
    const float delta_x = target_world_x - game->world.player_x;
    const float delta_y = target_world_y - game->world.player_y;
    const float move_length = SDL_sqrtf((delta_x * delta_x) + (delta_y * delta_y));
    if (move_length <= 0.001f)
    {
        game_cancel_auto_walk(game, "Reached tracked object.", true);
        return;
    }

    *move_x = delta_x / move_length;
    *move_y = delta_y / move_length;
    *jump_pressed = true;
}

static void game_update_auto_walk_progress(Game *game,
                                           float player_x_before,
                                           float player_y_before,
                                           float delta_time)
{
    if (game == NULL || !game->auto_walk_active)
    {
        return;
    }

    if (game->world.tile_size <= 0)
    {
        game_cancel_auto_walk(game, NULL, false);
        return;
    }

    const int player_tile_x = (int)(game->world.player_x / (float)game->world.tile_size);
    const int player_tile_y = (int)(game->world.player_y / (float)game->world.tile_size);
    if (player_tile_x == game->auto_walk_target_x &&
        player_tile_y == game->auto_walk_target_y)
    {
        game_cancel_auto_walk(game, "Reached tracked object.", true);
        return;
    }

    const float moved =
        SDL_fabsf(game->world.player_x - player_x_before) +
        SDL_fabsf(game->world.player_y - player_y_before);
    if (moved < 0.01f)
    {
        game->auto_walk_stuck_seconds += delta_time;
        if (game->auto_walk_stuck_seconds >= 0.6f)
        {
            game_cancel_auto_walk(game, "Auto walk blocked before target.", true);
        }
    }
    else
    {
        game->auto_walk_stuck_seconds = 0.0f;
    }
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
    game->structure_builder_active = false;
    game->game_mode = mode;
    inventory_init(&game->inventory, mode);
    if (mode == GAME_MODE_SURVIVAL)
    {
        game_place_opening_wreckage(game);
    }
    game->has_prev_tile = false;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->facing_dir_x = 0;
    game->facing_dir_y = 1;
    game->auto_walk_active = false;
    game->auto_walk_target_x = -1;
    game->auto_walk_target_y = -1;
    game->auto_walk_stuck_seconds = 0.0f;
    game_reset_axe_chop(game);
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game_reset_builder_shape_state(game);
    game_announce(mode == GAME_MODE_CREATIVE ? "Creative world ready."
                                             : "Survival world ready.",
                  false);
    return true;
}

static bool game_create_structure_builder_world(Game *game)
{
    enum
    {
        BUILDER_WORLD_WIDTH_TILES = 96,
        BUILDER_WORLD_HEIGHT_TILES = 64,
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

    if (!world_init_flat(&game->world,
                         BUILDER_WORLD_WIDTH_TILES,
                         BUILDER_WORLD_HEIGHT_TILES,
                         32,
                         TILE_GRASS,
                         BIOME_PLAINS,
                         18.0f))
    {
        fprintf(stderr, "game: structure builder world initialization failed\n");
        game_announce("Structure builder failed to start.", true);
        return false;
    }

    game->world_loaded = true;
    game->structure_builder_active = true;
    game->game_mode = GAME_MODE_CREATIVE;
    inventory_init(&game->inventory, GAME_MODE_CREATIVE);
    structure_builder_config_reset(&game->structure_builder_config);
    ui_structure_save_bind_config(&game->structure_builder_config);
    game_apply_structure_builder_biome(game, game->structure_builder_config.builder_biome);

    game->has_prev_tile = false;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->facing_dir_x = 0;
    game->facing_dir_y = 1;
    game->auto_walk_active = false;
    game->auto_walk_target_x = -1;
    game->auto_walk_target_y = -1;
    game->auto_walk_stuck_seconds = 0.0f;
    game_reset_axe_chop(game);
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game_reset_builder_shape_state(game);
    game_announce("Structure builder ready. E opens inventory. Enter places. Shift Enter toggles fill mode. Left and right bracket change shape. Backspace removes. Control Tab changes biome. Tab opens save.",
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
    game->prev_left_bracket = false;
    game->prev_right_bracket = false;
    game->prev_mouse_right = false;
    for (int i = 0; i < INVENTORY_HOTBAR_SLOT_COUNT; i++)
    {
        game->prev_hotbar_keys[i] = false;
    }
    game->has_prev_tile = false;
    game->prev_tile_x = -1;
    game->prev_tile_y = -1;
    game->has_prev_blocked_tile = false;
    game->prev_blocked_tile_x = -1;
    game->prev_blocked_tile_y = -1;
    game->facing_dir_x = 0;
    game->facing_dir_y = 1;
    game->tracker_category_index = 0;
    game->tracker_object_index = 0;
    game->auto_walk_active = false;
    game->auto_walk_target_x = -1;
    game->auto_walk_target_y = -1;
    game->auto_walk_stuck_seconds = 0.0f;
    game_reset_axe_chop(game);
    game->pending_hotbar_tile = TILE_ID_COUNT;
    game->pending_inventory_slot = -1;
    game_reset_builder_shape_state(game);
    game->game_mode = GAME_MODE_SURVIVAL;
    game->structure_builder_active = false;
    inventory_init(&game->inventory, GAME_MODE_SURVIVAL);
    structure_builder_config_reset(&game->structure_builder_config);
    ui_structure_save_bind_config(&game->structure_builder_config);

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
    const bool page_up_now = engine_key_down(engine, SDL_SCANCODE_PAGEUP);
    const bool page_down_now = engine_key_down(engine, SDL_SCANCODE_PAGEDOWN);
    const bool home_now = engine_key_down(engine, SDL_SCANCODE_HOME);
    const bool tab_now = engine_key_down(engine, SDL_SCANCODE_TAB);
    const bool space_now = engine_key_down(engine, SDL_SCANCODE_SPACE);
    const bool e_now = engine_key_down(engine, SDL_SCANCODE_E);
    const bool left_bracket_now = engine_key_down(engine, SDL_SCANCODE_LEFTBRACKET);
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
    const bool page_up_pressed = page_up_now && !game->prev_page_up;
    const bool page_down_pressed = page_down_now && !game->prev_page_down;
    const bool home_pressed = home_now && !game->prev_home;
    const bool tab_pressed = tab_now && !game->prev_tab;
    const bool space_pressed = space_now && !game->prev_space;
    const bool e_pressed = e_now && !game->prev_e;
    const bool left_bracket_pressed =
        left_bracket_now && !game->prev_left_bracket;
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
    game->prev_page_up = page_up_now;
    game->prev_page_down = page_down_now;
    game->prev_home = home_now;
    game->prev_tab = tab_now;
    game->prev_space = space_now;
    game->prev_e = e_now;
    game->prev_left_bracket = left_bracket_now;
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

    if (action == UI_ACTION_START_STRUCTURE_BUILDER)
    {
        game_announce("Opening structure builder.", true);
        if (game_create_structure_builder_world(game))
        {
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
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

    if (action == UI_ACTION_SAVE_STRUCTURE)
    {
        char saved_path[256];
        char error_message[160];
        if (structure_builder_save(&game->world, &game->structure_builder_config,
                                   saved_path, sizeof(saved_path),
                                   error_message, sizeof(error_message)))
        {
            char message[320];
            snprintf(message, sizeof(message), "%s saved to %s.",
                     structure_builder_save_kind_name(game->structure_builder_config.save_kind),
                     saved_path);
            game_announce(message, true);
            ui_show_screen(&game->ui, UI_SCREEN_WORLD,
                           game->speech_ready ? game_announce : NULL);
        }
        else
        {
            game_announce(error_message[0] != '\0' ? error_message
                                                   : "Structure save failed.",
                          true);
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

    if (tab_pressed && !ctrl_now &&
        ui_screen(&game->ui) == UI_SCREEN_WORLD &&
        game->world_loaded &&
        game->structure_builder_active)
    {
        ui_show_screen(&game->ui, UI_SCREEN_STRUCTURE_SAVE,
                       game->speech_ready ? game_announce : NULL);
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
        game->auto_walk_active = false;
        game->auto_walk_stuck_seconds = 0.0f;
        game_reset_axe_chop(game);
        return;
    }

    if (opening_scene_active)
    {
        water_biome_audio_update(NULL, delta_time);
        game->has_prev_tile = false;
        game->has_prev_blocked_tile = false;
        game->auto_walk_active = false;
        game->auto_walk_stuck_seconds = 0.0f;
        game_reset_axe_chop(game);
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

    if (move_x != 0.0f || move_y != 0.0f)
    {
        game->facing_dir_x = move_x > 0.0f ? 1 : (move_x < 0.0f ? -1 : 0);
        game->facing_dir_y = move_y > 0.0f ? 1 : (move_y < 0.0f ? -1 : 0);
    }

    if (game->structure_builder_active)
    {
        if (tab_pressed && ctrl_now)
        {
            game_cycle_structure_builder_biome(game, shift_now ? -1 : 1);
        }
        if (left_bracket_pressed)
        {
            game_cycle_builder_shape_mode(game, -1);
        }
        if (right_bracket_pressed)
        {
            game_cycle_builder_shape_mode(game, 1);
        }
        if (enter_pressed)
        {
            if (shift_now)
            {
                game_toggle_builder_fill_mode(game);
            }
            else
            {
                game_place_selected_tile_in_builder(game);
            }
        }
        if (backspace_pressed)
        {
            game_clear_builder_target_tile(game);
        }
    }

    const bool tool_use_consumed =
        game->structure_builder_active ? false
                                       : game_update_axe_use(game, space_now, space_pressed,
                                                             delta_time);
    const bool manual_move_input = move_x != 0.0f || move_y != 0.0f;
    bool jump_pressed = !game->structure_builder_active && space_pressed && !tool_use_consumed;
    game_apply_auto_walk(game, manual_move_input, &move_x, &move_y, &jump_pressed);

    const float player_x_before = game->world.player_x;
    const float player_y_before = game->world.player_y;
    world_update(&game->world, delta_time, move_x, move_y, jump_pressed);
    game_update_auto_walk_progress(game, player_x_before, player_y_before, delta_time);
    if (!game->structure_builder_active)
    {
        game_try_auto_pickup_wood_near_player(game);
    }
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
        if (ctrl_now)
        {
            game_start_auto_walk_to_tracker_target(game);
        }
        else
        {
            game_announce_tracker_coordinates(game, true);
        }
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
