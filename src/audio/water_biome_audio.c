#include "water_biome_audio.h"

#include "settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include "../world/world.h"

#ifdef _WIN32
#include <windows.h>

typedef struct BiomeAudioTrack
{
    const char *relative_path;
    const char *alias;
    bool (*matches_biome)(BiomeType biome_type);
    float peak_volume;
    bool apply_loop_smoothing;
    bool is_footstep_track;
    bool is_swim_track;
    bool footstep_is_cold;
    bool footstep_is_ship_piece;
    int left_volume;
    int right_volume;
    int speed_permille;
    unsigned int length_ms;
    float drift_current;
    float drift_target;
    float drift_timer;
    bool initialized;
} BiomeAudioTrack;

static bool g_audio_started = false;
static bool g_tracks_playing = false;
static float g_update_timer = 0.0f;
static float g_tundra_loop_fade = 1.0f;
static unsigned int g_rng_state = 0xA341316Cu;
static bool g_have_previous_player_position = false;
static float g_previous_player_x = 0.0f;
static float g_previous_player_y = 0.0f;

static bool is_water_biome(BiomeType biome_type);
static bool is_tundra_biome(BiomeType biome_type);
static bool is_cold_ground_tile(TileId tile_id);

static BiomeAudioTrack g_tracks[] = {
    {
        .relative_path = "assets/sfx/Water.mp3",
        .alias = "liryna_water_biome_ambience",
        .matches_biome = is_water_biome,
        .peak_volume = 950.0f,
        .apply_loop_smoothing = false,
        .is_footstep_track = false,
        .is_swim_track = false,
        .footstep_is_cold = false,
        .footstep_is_ship_piece = false,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
    {
        .relative_path = "assets/sfx/tundra.mp3",
        .alias = "liryna_tundra_biome_ambience",
        .matches_biome = is_tundra_biome,
        .peak_volume = 820.0f,
        .apply_loop_smoothing = true,
        .is_footstep_track = false,
        .is_swim_track = false,
        .footstep_is_cold = false,
        .footstep_is_ship_piece = false,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 0.92f,
        .drift_target = 0.92f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
    {
        .relative_path = "assets/sfx/tundrafootsteps.mp3.mp3",
        .alias = "liryna_tundra_footsteps",
        .matches_biome = is_tundra_biome,
        .peak_volume = 900.0f,
        .apply_loop_smoothing = false,
        .is_footstep_track = true,
        .is_swim_track = false,
        .footstep_is_cold = true,
        .footstep_is_ship_piece = false,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
    {
        .relative_path = "assets/sfx/forist.mp3",
        .alias = "liryna_forest_footsteps",
        .matches_biome = NULL,
        .peak_volume = 880.0f,
        .apply_loop_smoothing = false,
        .is_footstep_track = true,
        .is_swim_track = false,
        .footstep_is_cold = false,
        .footstep_is_ship_piece = false,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
    {
        .relative_path = "assets/sfx/swimmingsound.mp3",
        .alias = "liryna_swimming",
        .matches_biome = NULL,
        .peak_volume = 860.0f,
        .apply_loop_smoothing = false,
        .is_footstep_track = false,
        .is_swim_track = true,
        .footstep_is_cold = false,
        .footstep_is_ship_piece = false,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
    {
        .relative_path = "assets/sfx/shipfootstep.mp3",
        .alias = "liryna_ship_piece_footsteps",
        .matches_biome = NULL,
        .peak_volume = 900.0f,
        .apply_loop_smoothing = false,
        .is_footstep_track = true,
        .is_swim_track = false,
        .footstep_is_cold = false,
        .footstep_is_ship_piece = true,
        .left_volume = -1,
        .right_volume = -1,
        .speed_permille = -1,
        .length_ms = 0U,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
        .drift_timer = 0.0f,
        .initialized = false,
    },
};

static const size_t k_track_count = sizeof(g_tracks) / sizeof(g_tracks[0]);

static bool send_mci_command(const char *command)
{
    char error_text[256];
    const MCIERROR result = mciSendStringA(command, NULL, 0, NULL);
    if (result == 0)
    {
        return true;
    }

    if (!mciGetErrorStringA(result, error_text, sizeof(error_text)))
    {
        SDL_snprintf(error_text, sizeof(error_text), "Unknown error %u", (unsigned int)result);
    }

    fprintf(stderr, "biome_audio: MCI command failed: %s (%s)\n", command, error_text);
    return false;
}

static bool read_mci_unsigned(const char *command, unsigned int *out_value)
{
    if (command == NULL || out_value == NULL)
    {
        return false;
    }

    char result[128] = {0};
    const MCIERROR status = mciSendStringA(command, result, (UINT)sizeof(result), NULL);
    if (status != 0 || result[0] == '\0')
    {
        return false;
    }

    char *end = NULL;
    const unsigned long parsed = strtoul(result, &end, 10);
    if (end == result)
    {
        return false;
    }

    *out_value = (unsigned int)parsed;
    return true;
}

static bool try_path(char *out_path, size_t out_size, const char *candidate)
{
    if (out_path == NULL || out_size == 0 || candidate == NULL)
    {
        return false;
    }

    char normalized[1024];
    const char *full_path = _fullpath(normalized, candidate, sizeof(normalized));
    const char *path_to_test = candidate;
    if (full_path != NULL)
    {
        path_to_test = normalized;
    }

    if (SDL_strlcpy(out_path, path_to_test, out_size) >= out_size)
    {
        return false;
    }

    SDL_IOStream *stream = SDL_IOFromFile(out_path, "rb");
    if (stream == NULL)
    {
        return false;
    }

    SDL_CloseIO(stream);
    return true;
}

static bool resolve_track_path(const char *relative_path, char *out_path, size_t out_size)
{
    if (relative_path == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_path(out_path, out_size, relative_path))
    {
        return true;
    }

    const char *dot = strrchr(relative_path, '.');
    if (dot != NULL)
    {
        char alternate_relative_path[1024];
        if (SDL_strcasecmp(dot, ".wav") == 0)
        {
            SDL_snprintf(alternate_relative_path, sizeof(alternate_relative_path), "%.*s.mp3",
                         (int)(dot - relative_path), relative_path);
            if (try_path(out_path, out_size, alternate_relative_path))
            {
                return true;
            }

            SDL_snprintf(alternate_relative_path, sizeof(alternate_relative_path), "%.*s.mp3.mp3",
                         (int)(dot - relative_path), relative_path);
            if (try_path(out_path, out_size, alternate_relative_path))
            {
                return true;
            }
        }
        else if (SDL_strcasecmp(dot, ".mp3") == 0)
        {
            SDL_snprintf(alternate_relative_path, sizeof(alternate_relative_path), "%.*s.wav",
                         (int)(dot - relative_path), relative_path);
            if (try_path(out_path, out_size, alternate_relative_path))
            {
                return true;
            }
        }
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    static const char *k_prefixes[] = {
        "",
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
    };
    char candidate[1024];
    bool found = false;

    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     relative_path);
        if (try_path(out_path, out_size, candidate))
        {
            found = true;
            break;
        }
    }

    SDL_free((void *)base_path);
    return found;
}

static int clamp_volume(int volume)
{
    if (volume < 0)
    {
        return 0;
    }
    if (volume > 1000)
    {
        return 1000;
    }

    return volume;
}

static float random_unit(void)
{
    g_rng_state = (1664525u * g_rng_state) + 1013904223u;
    return ((float)(g_rng_state >> 8) / (float)0x00FFFFFFu);
}

static int clamp_speed_permille(int speed_permille)
{
    if (speed_permille < 500)
    {
        return 500;
    }
    if (speed_permille > 2200)
    {
        return 2200;
    }

    return speed_permille;
}

static void set_track_speed(BiomeAudioTrack *track, int speed_permille)
{
    if (track == NULL)
    {
        return;
    }

    speed_permille = clamp_speed_permille(speed_permille);

    if (!g_audio_started)
    {
        track->speed_permille = speed_permille;
        return;
    }

    if (track->speed_permille == speed_permille)
    {
        return;
    }

    char command[128];
    SDL_snprintf(command, sizeof(command), "set %s speed %d", track->alias, speed_permille);
    if (send_mci_command(command))
    {
        track->speed_permille = speed_permille;
    }
}

static void set_track_channel_volume(BiomeAudioTrack *track, int left, int right)
{
    if (track == NULL)
    {
        return;
    }

    left = clamp_volume(left);
    right = clamp_volume(right);

    if (!g_audio_started)
    {
        track->left_volume = left;
        track->right_volume = right;
        return;
    }

    if (left == track->left_volume && right == track->right_volume)
    {
        return;
    }

    char command[128];

    if (left != track->left_volume)
    {
        SDL_snprintf(command, sizeof(command), "setaudio %s left volume to %d", track->alias, left);
        send_mci_command(command);
        track->left_volume = left;
    }

    if (right != track->right_volume)
    {
        SDL_snprintf(command, sizeof(command), "setaudio %s right volume to %d", track->alias, right);
        send_mci_command(command);
        track->right_volume = right;
    }
}

static bool is_water_biome(BiomeType biome_type)
{
    return biome_type == BIOME_OCEAN || biome_type == BIOME_LAKE || biome_type == BIOME_RIVER ||
           biome_type == BIOME_COAST || biome_type == BIOME_SWAMP;
}

static bool is_tundra_biome(BiomeType biome_type)
{
    return biome_type == BIOME_TUNDRA || biome_type == BIOME_SNOWY_MOUNTAINS;
}

static bool is_cold_ground_tile(TileId tile_id)
{
    switch (tile_id)
    {
    case TILE_SNOW:
    case TILE_ICE:
    case TILE_FROZENGROUND:
    case TILE_PERMAFROST:
        return true;
    default:
        return false;
    }
}

static bool find_nearest_matching_tile(const World *world,
                                       int player_tile_x,
                                       int player_tile_y,
                                       int hearing_radius_tiles,
                                       bool (*matches_biome)(BiomeType biome_type),
                                       int *out_x,
                                       int *out_y,
                                       int *out_distance_sq)
{
    if (world == NULL || matches_biome == NULL || out_x == NULL || out_y == NULL ||
        out_distance_sq == NULL)
    {
        return false;
    }

    const int min_x = SDL_max(0, player_tile_x - hearing_radius_tiles);
    const int max_x = SDL_min(world->width - 1, player_tile_x + hearing_radius_tiles);
    const int min_y = SDL_max(0, player_tile_y - hearing_radius_tiles);
    const int max_y = SDL_min(world->height - 1, player_tile_y + hearing_radius_tiles);

    int best_x = 0;
    int best_y = 0;
    int best_distance_sq = (hearing_radius_tiles * hearing_radius_tiles) + 1;
    bool found_match = false;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            const int index = (y * world->width) + x;
            if (!matches_biome(world->biomes[index]))
            {
                continue;
            }

            const int dx = x - player_tile_x;
            const int dy = y - player_tile_y;
            const int distance_sq = (dx * dx) + (dy * dy);

            if (distance_sq > hearing_radius_tiles * hearing_radius_tiles)
            {
                continue;
            }

            if (!found_match || distance_sq < best_distance_sq)
            {
                found_match = true;
                best_distance_sq = distance_sq;
                best_x = x;
                best_y = y;
            }
        }
    }

    if (!found_match)
    {
        return false;
    }

    *out_x = best_x;
    *out_y = best_y;
    *out_distance_sq = best_distance_sq;
    return true;
}

static float compute_loop_smoothing_gain(BiomeAudioTrack *track, float delta_time)
{
    if (track == NULL || !track->apply_loop_smoothing)
    {
        return 1.0f;
    }

    if (track->drift_timer <= 0.0f)
    {
        track->drift_target = 0.86f + (0.14f * random_unit());
        track->drift_timer = 0.7f + (1.3f * random_unit());
    }

    track->drift_timer -= delta_time;
    const float drift_step = SDL_clamp(delta_time * 0.8f, 0.0f, 1.0f);
    track->drift_current += (track->drift_target - track->drift_current) * drift_step;

    if (track->length_ms < 5000U)
    {
        return track->drift_current;
    }

    char command[128];
    unsigned int position_ms = 0U;
    SDL_snprintf(command, sizeof(command), "status %s position", track->alias);
    if (!read_mci_unsigned(command, &position_ms))
    {
        return track->drift_current;
    }

    const float position = (float)position_ms;
    const float length = (float)track->length_ms;
    const float from_start = position;
    const float to_end = SDL_max(0.0f, length - position);
    const float edge_distance = SDL_min(from_start, to_end);
    const float fade_window_ms = 1800.0f;
    const float t = SDL_clamp(edge_distance / fade_window_ms, 0.0f, 1.0f);
    const float smooth = (t * t) * (3.0f - (2.0f * t));

    g_tundra_loop_fade += (smooth - g_tundra_loop_fade) * SDL_clamp(delta_time * 4.0f, 0.0f, 1.0f);
    return track->drift_current * g_tundra_loop_fade;
}

static bool init_track(BiomeAudioTrack *track)
{
    if (track == NULL)
    {
        return false;
    }

    char track_path[1024];
    if (!resolve_track_path(track->relative_path, track_path, sizeof(track_path)))
    {
        fprintf(stderr, "biome_audio: failed to locate %s\n", track->relative_path);
        return false;
    }

    char command[1300];
    SDL_snprintf(command, sizeof(command), "open \"%s\" type waveaudio alias %s", track_path,
                 track->alias);
    if (!send_mci_command(command))
    {
        SDL_snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s", track_path,
                     track->alias);
        if (!send_mci_command(command))
        {
            SDL_snprintf(command, sizeof(command), "open \"%s\" alias %s", track_path,
                         track->alias);
            if (!send_mci_command(command))
            {
                return false;
            }
        }
    }

    SDL_snprintf(command, sizeof(command), "set %s time format milliseconds", track->alias);
    send_mci_command(command);

    SDL_snprintf(command, sizeof(command), "setaudio %s left volume to 0", track->alias);
    send_mci_command(command);
    SDL_snprintf(command, sizeof(command), "setaudio %s right volume to 0", track->alias);
    send_mci_command(command);
    SDL_snprintf(command, sizeof(command), "set %s speed 1000", track->alias);
    send_mci_command(command);
    track->left_volume = 0;
    track->right_volume = 0;
    track->speed_permille = 1000;
    track->initialized = true;

    SDL_snprintf(command, sizeof(command), "status %s length", track->alias);
    (void)read_mci_unsigned(command, &track->length_ms);

    return true;
}

static void start_all_tracks(void)
{
    if (!g_audio_started || g_tracks_playing)
    {
        return;
    }

    for (size_t i = 0; i < k_track_count; i++)
    {
        if (!g_tracks[i].initialized)
        {
            continue;
        }
        char command[160];
        SDL_snprintf(command, sizeof(command), "seek %s to start", g_tracks[i].alias);
        send_mci_command(command);
        SDL_snprintf(command, sizeof(command), "play %s repeat", g_tracks[i].alias);
        send_mci_command(command);
    }

    g_tracks_playing = true;
}

static void stop_all_tracks(void)
{
    if (!g_audio_started || !g_tracks_playing)
    {
        return;
    }

    for (size_t i = 0; i < k_track_count; i++)
    {
        if (!g_tracks[i].initialized)
        {
            continue;
        }
        char command[160];
        SDL_snprintf(command, sizeof(command), "stop %s", g_tracks[i].alias);
        send_mci_command(command);
        SDL_snprintf(command, sizeof(command), "seek %s to start", g_tracks[i].alias);
        send_mci_command(command);
    }

    g_tracks_playing = false;
}

bool water_biome_audio_init(void)
{
    if (g_audio_started)
    {
        return true;
    }

    int initialized_track_count = 0;
    for (size_t i = 0; i < k_track_count; i++)
    {
        g_tracks[i].initialized = false;
        if (!init_track(&g_tracks[i]))
        {
            continue;
        }
        initialized_track_count++;
    }

    if (initialized_track_count == 0)
    {
        fprintf(stderr, "biome_audio: no audio tracks could be initialized\n");
        return false;
    }

    g_audio_started = true;
    g_tracks_playing = false;
    g_update_timer = 0.0f;
    g_tundra_loop_fade = 1.0f;
    return true;
}

void water_biome_audio_update(const World *world, float delta_time)
{
    g_update_timer += delta_time;
    if (g_update_timer < 0.08f)
    {
        return;
    }
    const float update_dt = g_update_timer;
    g_update_timer = 0.0f;

    if (!g_audio_started || world == NULL || world->tiles == NULL || world->biomes == NULL ||
        world->width <= 0 || world->height <= 0 || world->tile_size <= 0)
    {
        stop_all_tracks();
        for (size_t i = 0; i < k_track_count; i++)
        {
            set_track_channel_volume(&g_tracks[i], 0, 0);
            set_track_speed(&g_tracks[i], 1000);
        }
        g_have_previous_player_position = false;
        return;
    }

    start_all_tracks();

    const int player_tile_x = (int)(world->player_x / (float)world->tile_size);
    const int player_tile_y = (int)(world->player_y / (float)world->tile_size);
    const bool player_in_bounds = player_tile_x >= 0 && player_tile_y >= 0 &&
                                  player_tile_x < world->width && player_tile_y < world->height;
    bool player_on_ground_layer = false;
    bool player_on_cold_ground = false;
    bool player_on_ship_piece = false;
    bool player_swimming = false;
    if (player_in_bounds)
    {
        const int player_index = (player_tile_y * world->width) + player_tile_x;
        const TileId player_tile_id = world->tiles[player_index];
        const TileDefinition *player_tile = tiles_get_definition(player_tile_id);
        if (player_tile != NULL)
        {
            if (player_tile->walkable && player_tile->layer == TILE_LAYER_GROUND)
            {
                player_on_ground_layer = true;
                player_on_cold_ground = is_cold_ground_tile(player_tile_id);
                player_on_ship_piece = player_tile_id == TILE_SHIPPIECE;
            }
            player_swimming = player_tile->is_liquid && !player_tile->blocks_swimming;
        }
    }
    float movement_speed = 0.0f;
    if (g_have_previous_player_position && update_dt > 0.0001f)
    {
        const float delta_x = world->player_x - g_previous_player_x;
        const float delta_y = world->player_y - g_previous_player_y;
        movement_speed = sqrtf((delta_x * delta_x) + (delta_y * delta_y)) / update_dt;
    }
    g_previous_player_x = world->player_x;
    g_previous_player_y = world->player_y;
    g_have_previous_player_position = true;

    const int hearing_radius_tiles = 12;

    for (size_t i = 0; i < k_track_count; i++)
    {
        BiomeAudioTrack *track = &g_tracks[i];
        if (!track->initialized)
        {
            continue;
        }
        if (track->is_swim_track)
        {
            if (!player_swimming || movement_speed < 2.0f)
            {
                set_track_channel_volume(track, 0, 0);
                set_track_speed(track, 1000);
                continue;
            }

            const float swim_speed = world->player_swim_speed > 0.001f ? world->player_swim_speed
                                                                        : world->player_speed;
            const float normalized_speed =
                swim_speed > 0.001f ? SDL_clamp(movement_speed / swim_speed, 0.0f, 1.6f) : 0.0f;
            const float gain = SDL_clamp(0.28f + (normalized_speed * 0.52f), 0.0f, 1.0f);
            const int base_volume = (int)(gain * track->peak_volume);
            const int speed_permille = (int)(760.0f + (normalized_speed * 520.0f));
            set_track_speed(track, speed_permille);
            set_track_channel_volume(track, base_volume, base_volume);
            continue;
        }

        if (track->is_footstep_track)
        {
            bool track_matches_current_surface = false;
            if (player_on_ground_layer)
            {
                if (track->footstep_is_ship_piece)
                {
                    track_matches_current_surface = player_on_ship_piece;
                }
                else if (track->footstep_is_cold)
                {
                    track_matches_current_surface = player_on_cold_ground;
                }
                else
                {
                    track_matches_current_surface = !player_on_cold_ground && !player_on_ship_piece;
                }
            }
            if (!track_matches_current_surface || movement_speed < 2.0f)
            {
                set_track_channel_volume(track, 0, 0);
                set_track_speed(track, 1000);
                continue;
            }

            const float normalized_speed =
                world->player_speed > 0.001f ? SDL_clamp(movement_speed / world->player_speed, 0.0f, 1.7f)
                                             : 0.0f;
            const float gain = SDL_clamp(0.30f + (normalized_speed * 0.50f), 0.0f, 1.0f);
            int base_volume = (int)(gain * track->peak_volume);
            base_volume = settings_effective_footsteps_mci_volume(base_volume);
            const int speed_permille = (int)(700.0f + (normalized_speed * 600.0f));
            set_track_speed(track, speed_permille);
            set_track_channel_volume(track, base_volume, base_volume);
            continue;
        }

        int best_x = 0;
        int best_y = 0;
        int best_distance_sq = 0;
        const bool found = find_nearest_matching_tile(world, player_tile_x, player_tile_y,
                                                      hearing_radius_tiles, track->matches_biome,
                                                      &best_x, &best_y, &best_distance_sq);
        if (!found)
        {
            set_track_channel_volume(track, 0, 0);
            continue;
        }

        const float distance_tiles = sqrtf((float)best_distance_sq);
        const float proximity =
            SDL_clamp(1.0f - (distance_tiles / (float)hearing_radius_tiles), 0.0f, 1.0f);
        float base_gain = proximity * proximity;
        if (track->apply_loop_smoothing)
        {
            base_gain *= compute_loop_smoothing_gain(track, update_dt);
        }

        const float pan =
            SDL_clamp((float)(best_x - player_tile_x) / (float)hearing_radius_tiles, -1.0f, 1.0f);
        const float angle = (pan + 1.0f) * SDL_PI_F * 0.25f;
        const float left_gain = cosf(angle);
        const float right_gain = sinf(angle);

        const int base_volume = settings_effective_ambience_mci_volume(
            (int)(base_gain * track->peak_volume));
        const int left_volume = (int)(left_gain * (float)base_volume);
        const int right_volume = (int)(right_gain * (float)base_volume);
        set_track_channel_volume(track, left_volume, right_volume);
        set_track_speed(track, 1000);
    }
}

void water_biome_audio_shutdown(void)
{
    if (!g_audio_started)
    {
        return;
    }

    for (size_t i = 0; i < k_track_count; i++)
    {
        if (!g_tracks[i].initialized)
        {
            continue;
        }
        char command[128];
        SDL_snprintf(command, sizeof(command), "stop %s", g_tracks[i].alias);
        send_mci_command(command);
        SDL_snprintf(command, sizeof(command), "close %s", g_tracks[i].alias);
        send_mci_command(command);
        g_tracks[i].left_volume = -1;
        g_tracks[i].right_volume = -1;
        g_tracks[i].speed_permille = -1;
        g_tracks[i].drift_current = g_tracks[i].apply_loop_smoothing ? 0.92f : 1.0f;
        g_tracks[i].drift_target = g_tracks[i].drift_current;
        g_tracks[i].drift_timer = 0.0f;
        g_tracks[i].initialized = false;
    }

    g_audio_started = false;
    g_tracks_playing = false;
    g_update_timer = 0.0f;
    g_tundra_loop_fade = 1.0f;
    g_have_previous_player_position = false;
    g_previous_player_x = 0.0f;
    g_previous_player_y = 0.0f;
}

#else

bool water_biome_audio_init(void)
{
    return true;
}

void water_biome_audio_update(const World *world, float delta_time)
{
    (void)world;
    (void)delta_time;
}

void water_biome_audio_shutdown(void)
{
}

#endif


