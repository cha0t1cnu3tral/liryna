#include "water_biome_audio.h"

#include "audio_backend.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <miniaudio.h>

#include "../world/world.h"

typedef struct BiomeAudioTrack
{
    const char *relative_path;
    bool (*matches_biome)(BiomeType biome_type);
    float peak_volume;
    bool apply_loop_smoothing;
    bool is_footstep_track;
    bool is_swim_track;
    bool footstep_is_cold;
    bool footstep_is_ship_piece;
    ma_sound sound;
    float volume;
    float pan;
    float pitch;
    float length_seconds;
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
        .matches_biome = is_water_biome,
        .peak_volume = 950.0f,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
    },
    {
        .relative_path = "assets/sfx/tundra.mp3",
        .matches_biome = is_tundra_biome,
        .peak_volume = 820.0f,
        .apply_loop_smoothing = true,
        .drift_current = 0.92f,
        .drift_target = 0.92f,
    },
    {
        .relative_path = "assets/sfx/tundrafootsteps.mp3.mp3",
        .matches_biome = is_tundra_biome,
        .peak_volume = 900.0f,
        .is_footstep_track = true,
        .footstep_is_cold = true,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
    },
    {
        .relative_path = "assets/sfx/forist.mp3",
        .peak_volume = 880.0f,
        .is_footstep_track = true,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
    },
    {
        .relative_path = "assets/sfx/swimmingsound.mp3",
        .peak_volume = 860.0f,
        .is_swim_track = true,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
    },
    {
        .relative_path = "assets/sfx/shipfootstep.mp3",
        .peak_volume = 900.0f,
        .is_footstep_track = true,
        .footstep_is_ship_piece = true,
        .drift_current = 1.0f,
        .drift_target = 1.0f,
    },
};

static const size_t k_track_count = sizeof(g_tracks) / sizeof(g_tracks[0]);

static bool try_path(char *out_path, size_t out_size, const char *candidate)
{
    if (out_path == NULL || out_size == 0 || candidate == NULL)
    {
        return false;
    }

    if (SDL_strlcpy(out_path, candidate, out_size) >= out_size)
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

static bool try_alternate_extension(char *out_path, size_t out_size, const char *path)
{
    const char *dot = path != NULL ? strrchr(path, '.') : NULL;
    if (dot == NULL)
    {
        return false;
    }

    char alternate_path[1024];
    if (SDL_strcasecmp(dot, ".wav") == 0)
    {
        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.mp3",
                     (int)(dot - path), path);
        if (try_path(out_path, out_size, alternate_path))
        {
            return true;
        }

        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.mp3.mp3",
                     (int)(dot - path), path);
        return try_path(out_path, out_size, alternate_path);
    }

    if (SDL_strcasecmp(dot, ".mp3") == 0)
    {
        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.wav",
                     (int)(dot - path), path);
        return try_path(out_path, out_size, alternate_path);
    }

    return false;
}

static bool resolve_track_path(const char *relative_path, char *out_path, size_t out_size)
{
    if (relative_path == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_path(out_path, out_size, relative_path) ||
        try_alternate_extension(out_path, out_size, relative_path))
    {
        return true;
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    static const char *k_prefixes[] = {
        "",
        "../",
        "../../",
        "../../../",
    };
    char candidate[1024];
    bool found = false;

    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     relative_path);
        if (try_path(out_path, out_size, candidate) ||
            try_alternate_extension(out_path, out_size, candidate))
        {
            found = true;
            break;
        }
    }

    SDL_free((void *)base_path);
    return found;
}

static float random_unit(void)
{
    g_rng_state = (1664525u * g_rng_state) + 1013904223u;
    return ((float)(g_rng_state >> 8) / (float)0x00FFFFFFu);
}

static float clamp_pitch(float pitch)
{
    if (pitch < 0.5f)
    {
        return 0.5f;
    }
    if (pitch > 2.2f)
    {
        return 2.2f;
    }

    return pitch;
}

static void set_track_pitch(BiomeAudioTrack *track, float pitch)
{
    if (track == NULL)
    {
        return;
    }

    pitch = clamp_pitch(pitch);
    if (fabsf(track->pitch - pitch) < 0.001f)
    {
        return;
    }

    if (track->initialized)
    {
        ma_sound_set_pitch(&track->sound, pitch);
    }
    track->pitch = pitch;
}

static void set_track_mix(BiomeAudioTrack *track, float volume, float pan)
{
    if (track == NULL)
    {
        return;
    }

    volume = SDL_clamp(volume, 0.0f, 1.0f);
    pan = SDL_clamp(pan, -1.0f, 1.0f);

    if (fabsf(track->volume - volume) < 0.001f &&
        fabsf(track->pan - pan) < 0.001f)
    {
        return;
    }

    if (track->initialized)
    {
        ma_sound_set_volume(&track->sound, volume);
        ma_sound_set_pan(&track->sound, pan);
    }

    track->volume = volume;
    track->pan = pan;
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

    if (track->length_seconds < 5.0f)
    {
        return track->drift_current;
    }

    float position = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&track->sound, &position) != MA_SUCCESS)
    {
        return track->drift_current;
    }

    const float from_start = position;
    const float to_end = SDL_max(0.0f, track->length_seconds - position);
    const float edge_distance = SDL_min(from_start, to_end);
    const float fade_window_seconds = 1.8f;
    const float t = SDL_clamp(edge_distance / fade_window_seconds, 0.0f, 1.0f);
    const float smooth = (t * t) * (3.0f - (2.0f * t));

    g_tundra_loop_fade +=
        (smooth - g_tundra_loop_fade) * SDL_clamp(delta_time * 4.0f, 0.0f, 1.0f);
    return track->drift_current * g_tundra_loop_fade;
}

static bool init_track(BiomeAudioTrack *track)
{
    if (track == NULL)
    {
        return false;
    }

    ma_engine *engine = audio_backend_engine();
    if (engine == NULL)
    {
        return false;
    }

    char track_path[1024];
    if (!resolve_track_path(track->relative_path, track_path, sizeof(track_path)))
    {
        fprintf(stderr, "biome_audio: failed to locate %s\n", track->relative_path);
        return false;
    }

    const ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    const ma_result result =
        ma_sound_init_from_file(engine, track_path, flags, NULL, NULL, &track->sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "biome_audio: failed to load %s: %s\n",
                track_path, ma_result_description(result));
        return false;
    }

    ma_sound_set_looping(&track->sound, MA_TRUE);
    ma_sound_set_volume(&track->sound, 0.0f);
    ma_sound_set_pan(&track->sound, 0.0f);
    ma_sound_set_pitch(&track->sound, 1.0f);

    track->volume = 0.0f;
    track->pan = 0.0f;
    track->pitch = 1.0f;
    track->length_seconds = 0.0f;
    track->initialized = true;

    (void)ma_sound_get_length_in_seconds(&track->sound, &track->length_seconds);
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

        ma_sound_seek_to_pcm_frame(&g_tracks[i].sound, 0);
        if (ma_sound_start(&g_tracks[i].sound) != MA_SUCCESS)
        {
            fprintf(stderr, "biome_audio: failed to start %s\n", g_tracks[i].relative_path);
        }
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

        ma_sound_stop(&g_tracks[i].sound);
        ma_sound_seek_to_pcm_frame(&g_tracks[i].sound, 0);
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
            set_track_mix(&g_tracks[i], 0.0f, 0.0f);
            set_track_pitch(&g_tracks[i], 1.0f);
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
                set_track_mix(track, 0.0f, 0.0f);
                set_track_pitch(track, 1.0f);
                continue;
            }

            const float swim_speed = world->player_swim_speed > 0.001f ? world->player_swim_speed
                                                                        : world->player_speed;
            const float normalized_speed =
                swim_speed > 0.001f ? SDL_clamp(movement_speed / swim_speed, 0.0f, 1.6f) : 0.0f;
            const float gain = SDL_clamp(0.28f + (normalized_speed * 0.52f), 0.0f, 1.0f);
            const float volume =
                SDL_clamp(gain * (track->peak_volume / 1000.0f), 0.0f, 1.0f);
            const float pitch = 0.76f + (normalized_speed * 0.52f);
            set_track_pitch(track, pitch);
            set_track_mix(track, volume, 0.0f);
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
                set_track_mix(track, 0.0f, 0.0f);
                set_track_pitch(track, 1.0f);
                continue;
            }

            const float normalized_speed =
                world->player_speed > 0.001f
                    ? SDL_clamp(movement_speed / world->player_speed, 0.0f, 1.7f)
                    : 0.0f;
            const float gain = SDL_clamp(0.30f + (normalized_speed * 0.50f), 0.0f, 1.0f);
            int base_volume = (int)(gain * track->peak_volume);
            base_volume = settings_effective_footsteps_mci_volume(base_volume);
            const float volume = SDL_clamp((float)base_volume / 1000.0f, 0.0f, 1.0f);
            const float pitch = 0.70f + (normalized_speed * 0.60f);
            set_track_pitch(track, pitch);
            set_track_mix(track, volume, 0.0f);
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
            set_track_mix(track, 0.0f, 0.0f);
            set_track_pitch(track, 1.0f);
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
        const int base_volume = settings_effective_ambience_mci_volume(
            (int)(base_gain * track->peak_volume));
        const float volume = SDL_clamp((float)base_volume / 1000.0f, 0.0f, 1.0f);
        set_track_mix(track, volume, pan);
        set_track_pitch(track, 1.0f);
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

        ma_sound_stop(&g_tracks[i].sound);
        ma_sound_uninit(&g_tracks[i].sound);
        g_tracks[i].volume = 0.0f;
        g_tracks[i].pan = 0.0f;
        g_tracks[i].pitch = 1.0f;
        g_tracks[i].length_seconds = 0.0f;
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
