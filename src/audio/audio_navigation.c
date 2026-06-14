#include "audio_navigation.h"

#include "audio_backend.h"
#include "settings.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_iostream.h>
#include <miniaudio.h>

static const char *const k_ping_path = "assets/sfx/audio navigation/ping.wav";
static const char *const k_start_path = "assets/sfx/audio navigation/start.wav";
static const char *const k_goal_path = "assets/sfx/audio navigation/goal_reached.wav";

/* Directional tuning. */
static const float k_pan_full_tiles = 8.0f;     /* offset for full left/right pan */
static const float k_pitch_full_tiles = 10.0f;  /* offset for full pitch shift */
static const float k_pitch_amount = 0.35f;      /* north raises, south lowers */
static const float k_hearing_tiles = 40.0f;     /* distance over which volume fades */
static const float k_ping_peak_volume = 850.0f; /* pre-settings peak (0..1000) */
static const int k_reached_tile_radius = 1;     /* Chebyshev distance counted as arrival */

static bool g_initialized = false;
static bool g_active = false;

static ma_sound g_ping_sound;
static ma_sound g_start_sound;
static ma_sound g_goal_sound;
static bool g_ping_ready = false;
static bool g_start_ready = false;
static bool g_goal_ready = false;
static bool g_ping_playing = false;

static float g_ping_volume = 0.0f;
static float g_ping_pan = 0.0f;
static float g_ping_pitch = 1.0f;

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

static bool resolve_path(const char *relative_path, char *out_path, size_t out_size)
{
    if (relative_path == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_path(out_path, out_size, relative_path))
    {
        return true;
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    static const char *const k_prefixes[] = {
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
        if (try_path(out_path, out_size, candidate))
        {
            found = true;
            break;
        }
    }

    SDL_free((void *)base_path);
    return found;
}

static bool load_sound(const char *relative_path, bool looping, ma_sound *out_sound)
{
    ma_engine *engine = audio_backend_engine();
    if (engine == NULL || out_sound == NULL)
    {
        return false;
    }

    char resolved_path[1024];
    if (!resolve_path(relative_path, resolved_path, sizeof(resolved_path)))
    {
        fprintf(stderr, "audio_navigation: failed to locate %s\n", relative_path);
        return false;
    }

    const ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    const ma_result result =
        ma_sound_init_from_file(engine, resolved_path, flags, NULL, NULL, out_sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "audio_navigation: failed to load %s: %s\n",
                resolved_path, ma_result_description(result));
        return false;
    }

    ma_sound_set_looping(out_sound, looping ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(out_sound, looping ? 0.0f : 1.0f);
    ma_sound_set_pan(out_sound, 0.0f);
    ma_sound_set_pitch(out_sound, 1.0f);
    return true;
}

static void play_one_shot(ma_sound *sound, bool ready, const char *relative_path)
{
    if (!ready || sound == NULL)
    {
        return;
    }

    const int base_volume = settings_effective_ambience_mci_volume((int)k_ping_peak_volume);
    ma_sound_set_volume(sound, SDL_clamp((float)base_volume / 1000.0f, 0.0f, 1.0f));
    ma_sound_seek_to_pcm_frame(sound, 0);
    if (ma_sound_start(sound) != MA_SUCCESS)
    {
        fprintf(stderr, "audio_navigation: failed to start %s\n", relative_path);
    }
}

bool audio_navigation_init(void)
{
    if (g_initialized)
    {
        return true;
    }

    g_ping_ready = load_sound(k_ping_path, true, &g_ping_sound);
    g_start_ready = load_sound(k_start_path, false, &g_start_sound);
    g_goal_ready = load_sound(k_goal_path, false, &g_goal_sound);

    g_active = false;
    g_ping_playing = false;
    g_ping_volume = 0.0f;
    g_ping_pan = 0.0f;
    g_ping_pitch = 1.0f;

    g_initialized = g_ping_ready || g_start_ready || g_goal_ready;
    return g_initialized;
}

bool audio_navigation_is_active(void)
{
    return g_active;
}

void audio_navigation_start(void)
{
    if (g_active)
    {
        return;
    }

    g_active = true;
    g_ping_volume = 0.0f;
    g_ping_pan = 0.0f;
    g_ping_pitch = 1.0f;

    play_one_shot(&g_start_sound, g_start_ready, k_start_path);

    if (g_ping_ready)
    {
        ma_sound_set_volume(&g_ping_sound, 0.0f);
        ma_sound_set_pan(&g_ping_sound, 0.0f);
        ma_sound_set_pitch(&g_ping_sound, 1.0f);
        ma_sound_seek_to_pcm_frame(&g_ping_sound, 0);
        if (ma_sound_start(&g_ping_sound) == MA_SUCCESS)
        {
            g_ping_playing = true;
        }
        else
        {
            fprintf(stderr, "audio_navigation: failed to start %s\n", k_ping_path);
        }
    }
}

void audio_navigation_stop(void)
{
    if (!g_active && !g_ping_playing)
    {
        return;
    }

    g_active = false;

    if (g_ping_ready && g_ping_playing)
    {
        ma_sound_stop(&g_ping_sound);
        ma_sound_seek_to_pcm_frame(&g_ping_sound, 0);
        ma_sound_set_volume(&g_ping_sound, 0.0f);
    }

    g_ping_playing = false;
    g_ping_volume = 0.0f;
    g_ping_pan = 0.0f;
    g_ping_pitch = 1.0f;
}

static void approach(float *current, float target, float step)
{
    if (current == NULL)
    {
        return;
    }

    *current += (target - *current) * step;
}

void audio_navigation_update(bool has_target,
                             int player_tile_x,
                             int player_tile_y,
                             int target_tile_x,
                             int target_tile_y,
                             float delta_time)
{
    if (!g_active || !g_ping_ready || !g_ping_playing)
    {
        return;
    }

    const float smoothing = SDL_clamp(delta_time * 10.0f, 0.0f, 1.0f);

    if (!has_target)
    {
        approach(&g_ping_volume, 0.0f, smoothing);
        ma_sound_set_volume(&g_ping_sound, g_ping_volume);
        return;
    }

    const int dx = target_tile_x - player_tile_x;
    const int dy = target_tile_y - player_tile_y;
    const int abs_dx = dx < 0 ? -dx : dx;
    const int abs_dy = dy < 0 ? -dy : dy;
    const int chebyshev = abs_dx > abs_dy ? abs_dx : abs_dy;

    if (chebyshev <= k_reached_tile_radius)
    {
        play_one_shot(&g_goal_sound, g_goal_ready, k_goal_path);
        audio_navigation_stop();
        return;
    }

    const float pan_target = SDL_clamp((float)dx / k_pan_full_tiles, -1.0f, 1.0f);
    const float norm_y = SDL_clamp((float)dy / k_pitch_full_tiles, -1.0f, 1.0f);
    const float pitch_target = SDL_clamp(1.0f - (norm_y * k_pitch_amount), 0.6f, 1.5f);

    const float distance_tiles = sqrtf((float)((dx * dx) + (dy * dy)));
    const float proximity = SDL_clamp(1.0f - (distance_tiles / k_hearing_tiles), 0.0f, 1.0f);
    const float gain = SDL_clamp(0.45f + (0.55f * proximity), 0.0f, 1.0f);
    const int base_volume = settings_effective_ambience_mci_volume((int)(gain * k_ping_peak_volume));
    const float volume_target = SDL_clamp((float)base_volume / 1000.0f, 0.0f, 1.0f);

    approach(&g_ping_volume, volume_target, smoothing);
    approach(&g_ping_pan, pan_target, smoothing);
    approach(&g_ping_pitch, pitch_target, smoothing);

    ma_sound_set_volume(&g_ping_sound, g_ping_volume);
    ma_sound_set_pan(&g_ping_sound, g_ping_pan);
    ma_sound_set_pitch(&g_ping_sound, g_ping_pitch);
}

void audio_navigation_shutdown(void)
{
    if (!g_initialized)
    {
        return;
    }

    if (g_ping_ready)
    {
        ma_sound_stop(&g_ping_sound);
        ma_sound_uninit(&g_ping_sound);
        g_ping_ready = false;
    }
    if (g_start_ready)
    {
        ma_sound_stop(&g_start_sound);
        ma_sound_uninit(&g_start_sound);
        g_start_ready = false;
    }
    if (g_goal_ready)
    {
        ma_sound_stop(&g_goal_sound);
        ma_sound_uninit(&g_goal_sound);
        g_goal_ready = false;
    }

    g_initialized = false;
    g_active = false;
    g_ping_playing = false;
    g_ping_volume = 0.0f;
    g_ping_pan = 0.0f;
    g_ping_pitch = 1.0f;
}
