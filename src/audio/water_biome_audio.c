#include "water_biome_audio.h"

#include <math.h>
#include <stdio.h>

#include <SDL3/SDL.h>
#include "../world/world.h"

#ifdef _WIN32
#include <windows.h>

static const char *k_water_relative_path = "assets/sfx/Water.mp3";
static const char *k_water_alias = "liryna_water_biome_ambience";

static bool g_audio_started = false;
static int g_left_volume = -1;
static int g_right_volume = -1;
static float g_update_timer = 0.0f;

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

    fprintf(stderr, "water_audio: MCI command failed: %s (%s)\n", command, error_text);
    return false;
}

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

static bool resolve_water_path(char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_path(out_path, out_size, k_water_relative_path))
    {
        return true;
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    char candidate[1024];
    bool found = false;

    SDL_snprintf(candidate, sizeof(candidate), "%s%s", base_path, k_water_relative_path);
    if (try_path(out_path, out_size, candidate))
    {
        found = true;
    }

    if (!found)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s..\\%s", base_path, k_water_relative_path);
        if (try_path(out_path, out_size, candidate))
        {
            found = true;
        }
    }

    if (!found)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s..\\..\\%s", base_path, k_water_relative_path);
        if (try_path(out_path, out_size, candidate))
        {
            found = true;
        }
    }

    SDL_free((void *)base_path);
    return found;
}

static bool is_water_biome(BiomeType biome_type)
{
    return biome_type == BIOME_OCEAN || biome_type == BIOME_LAKE || biome_type == BIOME_RIVER ||
           biome_type == BIOME_COAST || biome_type == BIOME_SWAMP;
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

static void set_channel_volume(int left, int right)
{
    left = clamp_volume(left);
    right = clamp_volume(right);

    if (!g_audio_started)
    {
        g_left_volume = left;
        g_right_volume = right;
        return;
    }

    if (left == g_left_volume && right == g_right_volume)
    {
        return;
    }

    char command[128];

    if (left != g_left_volume)
    {
        SDL_snprintf(command, sizeof(command), "setaudio %s left volume to %d", k_water_alias, left);
        send_mci_command(command);
        g_left_volume = left;
    }

    if (right != g_right_volume)
    {
        SDL_snprintf(command, sizeof(command), "setaudio %s right volume to %d", k_water_alias, right);
        send_mci_command(command);
        g_right_volume = right;
    }
}

bool water_biome_audio_init(void)
{
    if (g_audio_started)
    {
        return true;
    }

    char water_path[1024];
    if (!resolve_water_path(water_path, sizeof(water_path)))
    {
        fprintf(stderr, "water_audio: failed to locate %s\n", k_water_relative_path);
        return false;
    }

    char command[1300];
    SDL_snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s", water_path,
                 k_water_alias);
    if (!send_mci_command(command))
    {
        return false;
    }

    SDL_snprintf(command, sizeof(command), "play %s repeat", k_water_alias);
    if (!send_mci_command(command))
    {
        send_mci_command("close liryna_water_biome_ambience");
        return false;
    }

    g_audio_started = true;
    g_update_timer = 0.0f;
    set_channel_volume(0, 0);
    return true;
}

void water_biome_audio_update(const World *world, float delta_time)
{
    g_update_timer += delta_time;
    if (g_update_timer < 0.08f)
    {
        return;
    }
    g_update_timer = 0.0f;

    if (!g_audio_started || world == NULL || world->tiles == NULL || world->biomes == NULL ||
        world->width <= 0 || world->height <= 0 || world->tile_size <= 0)
    {
        set_channel_volume(0, 0);
        return;
    }

    const int player_tile_x = (int)(world->player_x / (float)world->tile_size);
    const int player_tile_y = (int)(world->player_y / (float)world->tile_size);

    const int hearing_radius_tiles = 12;
    const int min_x = SDL_max(0, player_tile_x - hearing_radius_tiles);
    const int max_x = SDL_min(world->width - 1, player_tile_x + hearing_radius_tiles);
    const int min_y = SDL_max(0, player_tile_y - hearing_radius_tiles);
    const int max_y = SDL_min(world->height - 1, player_tile_y + hearing_radius_tiles);

    int best_x = 0;
    int best_y = 0;
    int best_distance_sq = (hearing_radius_tiles * hearing_radius_tiles) + 1;
    bool found_water = false;

    for (int y = min_y; y <= max_y; y++)
    {
        for (int x = min_x; x <= max_x; x++)
        {
            const int index = (y * world->width) + x;
            if (!is_water_biome(world->biomes[index]))
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

            if (!found_water || distance_sq < best_distance_sq)
            {
                found_water = true;
                best_distance_sq = distance_sq;
                best_x = x;
                best_y = y;
            }
        }
    }

    if (!found_water)
    {
        set_channel_volume(0, 0);
        return;
    }

    const float distance_tiles = sqrtf((float)best_distance_sq);
    const float proximity = SDL_clamp(1.0f - (distance_tiles / (float)hearing_radius_tiles), 0.0f, 1.0f);
    const float base_gain = proximity * proximity;

    const float pan = SDL_clamp((float)(best_x - player_tile_x) / (float)hearing_radius_tiles, -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * SDL_PI_F * 0.25f;
    const float left_gain = cosf(angle);
    const float right_gain = sinf(angle);

    const int base_volume = (int)(base_gain * 950.0f);
    const int left_volume = (int)(left_gain * (float)base_volume);
    const int right_volume = (int)(right_gain * (float)base_volume);

    set_channel_volume(left_volume, right_volume);
}

void water_biome_audio_shutdown(void)
{
    if (!g_audio_started)
    {
        return;
    }

    send_mci_command("stop liryna_water_biome_ambience");
    send_mci_command("close liryna_water_biome_ambience");

    g_audio_started = false;
    g_left_volume = -1;
    g_right_volume = -1;
    g_update_timer = 0.0f;
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
