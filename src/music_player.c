#include "music_player.h"

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

static const char *k_music_relative_path = "assets/music/mm1 music opening.mp3";

#ifdef _WIN32
static const char *k_music_alias = "liryna_main_menu_music";
static bool g_music_started = false;

static bool file_exists(const char *path)
{
    SDL_IOStream *stream = SDL_IOFromFile(path, "rb");
    if (stream == NULL)
    {
        return false;
    }

    SDL_CloseIO(stream);
    return true;
}

static bool try_path(char *out_path, size_t out_size, const char *candidate)
{
    if (candidate == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (!file_exists(candidate))
    {
        return false;
    }

    SDL_strlcpy(out_path, candidate, out_size);
    return true;
}

static bool resolve_music_path(char *out_path, size_t out_size)
{
    if (out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_path(out_path, out_size, k_music_relative_path))
    {
        return true;
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    char candidate[1024];
    SDL_snprintf(candidate, sizeof(candidate), "%s%s", base_path, k_music_relative_path);
    if (try_path(out_path, out_size, candidate))
    {
        SDL_free((void *)base_path);
        return true;
    }

    SDL_snprintf(candidate, sizeof(candidate), "%s..\\%s", base_path, k_music_relative_path);
    if (try_path(out_path, out_size, candidate))
    {
        SDL_free((void *)base_path);
        return true;
    }

    SDL_snprintf(candidate, sizeof(candidate), "%s..\\..\\%s", base_path, k_music_relative_path);
    if (try_path(out_path, out_size, candidate))
    {
        SDL_free((void *)base_path);
        return true;
    }

    SDL_free((void *)base_path);
    return false;
}

static bool send_mci_command(const char *command)
{
    MCIERROR error = mciSendStringA(command, NULL, 0, NULL);
    if (error == 0)
    {
        return true;
    }

    char error_text[256];
    if (!mciGetErrorStringA(error, error_text, sizeof(error_text)))
    {
        SDL_strlcpy(error_text, "unknown MCI error", sizeof(error_text));
    }

    fprintf(stderr, "music: MCI command failed: %s (%s)\n", command, error_text);
    return false;
}

bool music_player_start_main_menu_music(void)
{
    if (g_music_started)
    {
        return true;
    }

    char music_path[1024];
    if (!resolve_music_path(music_path, sizeof(music_path)))
    {
        fprintf(stderr, "music: failed to locate %s\n", k_music_relative_path);
        return false;
    }

    char command[1400];
    SDL_snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s",
                 music_path, k_music_alias);
    if (!send_mci_command(command))
    {
        return false;
    }

    SDL_snprintf(command, sizeof(command), "setaudio %s volume to 500", k_music_alias);
    if (!send_mci_command(command))
    {
        send_mci_command("close liryna_main_menu_music");
        return false;
    }

    SDL_snprintf(command, sizeof(command), "play %s repeat", k_music_alias);
    if (!send_mci_command(command))
    {
        send_mci_command("close liryna_main_menu_music");
        return false;
    }

    g_music_started = true;
    return true;
}

void music_player_shutdown(void)
{
    if (!g_music_started)
    {
        return;
    }

    send_mci_command("stop liryna_main_menu_music");
    send_mci_command("close liryna_main_menu_music");
    g_music_started = false;
}

#else

bool music_player_start_main_menu_music(void)
{
    return false;
}

void music_player_shutdown(void)
{
}

#endif
