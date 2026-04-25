#include "music_player.h"

#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef _WIN32

static const char *k_menu_music_relative_path = "assets/music/mm1 music opening.mp3";

typedef enum MusicMode
{
    MUSIC_MODE_MENU = 0,
    MUSIC_MODE_WORLD
} MusicMode;

typedef enum TransitionReason
{
    TRANSITION_NONE = 0,
    TRANSITION_MODE_SWITCH,
    TRANSITION_NEXT_WORLD_TRACK
} TransitionReason;

static const char *k_music_alias = "liryna_music";
static const float k_transition_duration_seconds = 1.2f;
static const int k_max_music_volume = 500;
#define MAX_WORLD_TRACKS 32

static bool g_player_initialized = false;
static bool g_track_open = false;
static bool g_world_tracks_loaded = false;
static bool g_suspended = false;

static char g_world_tracks[MAX_WORLD_TRACKS][1024];
static int g_world_track_count = 0;
static int g_world_track_index = 0;

static MusicMode g_current_mode = MUSIC_MODE_MENU;
static MusicMode g_target_mode = MUSIC_MODE_MENU;
static bool g_fading_out = false;
static bool g_fading_in = false;
static float g_transition_timer = 0.0f;
static TransitionReason g_transition_reason = TRANSITION_NONE;

static int g_current_base_volume = 0;
static int g_current_effective_volume = -1;
static bool g_volume_control_supported = true;

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

static bool directory_exists(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool try_path(char *out_path, size_t out_size, const char *candidate)
{
    if (candidate == NULL || out_path == NULL || out_size == 0)
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

    if (!file_exists(path_to_test))
    {
        return false;
    }

    SDL_strlcpy(out_path, path_to_test, out_size);
    return true;
}

static bool try_directory(char *out_path, size_t out_size, const char *candidate)
{
    if (candidate == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    char normalized[1024];
    const char *full_path = _fullpath(normalized, candidate, sizeof(normalized));
    const char *path_to_test = full_path != NULL ? normalized : candidate;

    if (!directory_exists(path_to_test))
    {
        return false;
    }

    SDL_strlcpy(out_path, path_to_test, out_size);
    return true;
}

static bool resolve_music_path(char *out_path, size_t out_size, const char *relative_path)
{
    if (out_path == NULL || out_size == 0 || relative_path == NULL)
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
    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     relative_path);
        if (try_path(out_path, out_size, candidate))
        {
            SDL_free((void *)base_path);
            return true;
        }
    }

    SDL_free((void *)base_path);
    return false;
}

static bool resolve_music_directory(char *out_path, size_t out_size)
{
    static const char *k_music_relative_directory = "assets/music";
    if (out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (try_directory(out_path, out_size, k_music_relative_directory))
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
        "..\\",
        "..\\..\\",
        "..\\..\\..\\",
    };
    char candidate[1024];
    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     k_music_relative_directory);
        if (try_directory(out_path, out_size, candidate))
        {
            SDL_free((void *)base_path);
            return true;
        }
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

static bool query_mci_string(const char *command, char *out_text, size_t out_size)
{
    if (command == NULL || out_text == NULL || out_size == 0)
    {
        return false;
    }

    MCIERROR error = mciSendStringA(command, out_text, (UINT)out_size, NULL);
    if (error == 0)
    {
        return true;
    }

    char error_text[256];
    if (!mciGetErrorStringA(error, error_text, sizeof(error_text)))
    {
        SDL_strlcpy(error_text, "unknown MCI error", sizeof(error_text));
    }

    fprintf(stderr, "music: MCI query failed: %s (%s)\n", command, error_text);
    return false;
}

static int case_insensitive_compare(const char *a, const char *b)
{
    return SDL_strcasecmp(a, b);
}

static bool contains_case_insensitive(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL || needle[0] == '\0')
    {
        return false;
    }

    const size_t haystack_length = strlen(haystack);
    const size_t needle_length = strlen(needle);
    if (needle_length > haystack_length)
    {
        return false;
    }

    for (size_t i = 0; i <= haystack_length - needle_length; i++)
    {
        if (_strnicmp(haystack + i, needle, needle_length) == 0)
        {
            return true;
        }
    }

    return false;
}

static int compare_track_paths(const void *a, const void *b)
{
    const char *left = (const char *)a;
    const char *right = (const char *)b;
    return case_insensitive_compare(left, right);
}

static bool load_world_tracks(void)
{
    g_world_tracks_loaded = false;
    g_world_track_count = 0;
    g_world_track_index = 0;

    char music_directory[1024];
    if (!resolve_music_directory(music_directory, sizeof(music_directory)))
    {
        fprintf(stderr, "music: failed to locate assets/music directory\n");
        return false;
    }

    WIN32_FIND_DATAA find_data;
    char pattern[1200];
    SDL_snprintf(pattern, sizeof(pattern), "%s\\*.*", music_directory);
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    do
    {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        if (contains_case_insensitive(find_data.cFileName, "mm1"))
        {
            continue;
        }

        const char *extension = strrchr(find_data.cFileName, '.');
        if (extension == NULL ||
            (SDL_strcasecmp(extension, ".wav") != 0 &&
             SDL_strcasecmp(extension, ".mp3") != 0))
        {
            continue;
        }

        if (g_world_track_count >= MAX_WORLD_TRACKS)
        {
            break;
        }

        SDL_snprintf(g_world_tracks[g_world_track_count], sizeof(g_world_tracks[g_world_track_count]),
                     "%s\\%s", music_directory, find_data.cFileName);
        g_world_track_count++;
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);

    if (g_world_track_count > 1)
    {
        qsort(g_world_tracks, (size_t)g_world_track_count, sizeof(g_world_tracks[0]),
              compare_track_paths);
    }

    if (g_world_track_count == 0)
    {
        fprintf(stderr, "music: no non-mm1 world tracks found in assets/music\n");
        return false;
    }

    g_world_tracks_loaded = true;
    return true;
}

static void close_current_track(void)
{
    if (!g_track_open)
    {
        return;
    }

    send_mci_command("stop liryna_music");
    send_mci_command("close liryna_music");
    g_track_open = false;
    g_current_effective_volume = -1;
}

static bool set_music_volume(int volume)
{
    const int clamped_base_volume = SDL_clamp(volume, 0, k_max_music_volume);
    g_current_base_volume = clamped_base_volume;

    if (!g_track_open)
    {
        return true;
    }

    const int effective_volume = settings_effective_music_mci_volume(clamped_base_volume);
    if (!g_volume_control_supported)
    {
        g_current_effective_volume = effective_volume;
        return true;
    }

    if (effective_volume == g_current_effective_volume)
    {
        return true;
    }

    char command[128];
    SDL_snprintf(command, sizeof(command), "setaudio %s volume to %d", k_music_alias, effective_volume);
    if (!send_mci_command(command))
    {
        g_volume_control_supported = false;
        g_current_effective_volume = effective_volume;
        return true;
    }

    g_current_effective_volume = effective_volume;
    return true;
}

static bool open_and_play_track(const char *path, bool repeat)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    close_current_track();

    char command[1400];
    SDL_snprintf(command, sizeof(command), "open \"%s\" type waveaudio alias %s", path,
                 k_music_alias);
    if (!send_mci_command(command))
    {
        SDL_snprintf(command, sizeof(command), "open \"%s\" type mpegvideo alias %s", path,
                     k_music_alias);
        if (!send_mci_command(command))
        {
            SDL_snprintf(command, sizeof(command), "open \"%s\" alias %s", path, k_music_alias);
            if (!send_mci_command(command))
            {
                return false;
            }
        }
    }

    g_track_open = true;
    g_current_effective_volume = -1;

    SDL_snprintf(command, sizeof(command), "set %s time format milliseconds", k_music_alias);
    if (!send_mci_command(command))
    {
        close_current_track();
        return false;
    }

    if (!set_music_volume(0))
    {
        close_current_track();
        return false;
    }

    SDL_snprintf(command, sizeof(command), repeat ? "play %s repeat" : "play %s", k_music_alias);
    if (!send_mci_command(command))
    {
        SDL_snprintf(command, sizeof(command), "play %s", k_music_alias);
        if (!send_mci_command(command))
        {
            close_current_track();
            return false;
        }
    }

    g_fading_in = true;
    g_transition_timer = 0.0f;
    return true;
}

static bool start_menu_track(void)
{
    char music_path[1024];
    if (!resolve_music_path(music_path, sizeof(music_path), k_menu_music_relative_path))
    {
        fprintf(stderr, "music: failed to locate %s\n", k_menu_music_relative_path);
        return false;
    }

    if (!open_and_play_track(music_path, true))
    {
        return false;
    }

    g_current_mode = MUSIC_MODE_MENU;
    return true;
}

static bool start_world_track(void)
{
    if (!g_world_tracks_loaded)
    {
        load_world_tracks();
    }

    if (g_world_track_count <= 0)
    {
        close_current_track();
        return false;
    }

    if (g_world_track_index < 0 || g_world_track_index >= g_world_track_count)
    {
        g_world_track_index = 0;
    }

    if (!open_and_play_track(g_world_tracks[g_world_track_index], false))
    {
        return false;
    }

    g_current_mode = MUSIC_MODE_WORLD;
    return true;
}

static bool get_current_track_progress_ms(int *out_position_ms, int *out_length_ms)
{
    if (!g_track_open || out_position_ms == NULL || out_length_ms == NULL)
    {
        return false;
    }

    char command[128];
    char result[64];

    SDL_snprintf(command, sizeof(command), "status %s position", k_music_alias);
    if (!query_mci_string(command, result, sizeof(result)))
    {
        return false;
    }

    const int position_ms = atoi(result);

    SDL_snprintf(command, sizeof(command), "status %s length", k_music_alias);
    if (!query_mci_string(command, result, sizeof(result)))
    {
        return false;
    }

    const int length_ms = atoi(result);
    if (length_ms <= 0)
    {
        return false;
    }

    *out_position_ms = position_ms;
    *out_length_ms = length_ms;
    return true;
}

static bool is_track_playing(void)
{
    if (!g_track_open)
    {
        return false;
    }

    char command[128];
    char mode[32];

    SDL_snprintf(command, sizeof(command), "status %s mode", k_music_alias);
    if (!query_mci_string(command, mode, sizeof(mode)))
    {
        return false;
    }

    return SDL_strcasecmp(mode, "playing") == 0;
}

static void begin_transition(TransitionReason reason, MusicMode target_mode)
{
    g_transition_reason = reason;
    g_target_mode = target_mode;
    g_fading_out = g_track_open;
    g_fading_in = false;
    g_transition_timer = 0.0f;

    if (!g_track_open)
    {
        g_fading_out = false;
        if (g_target_mode == MUSIC_MODE_MENU)
        {
            start_menu_track();
        }
        else
        {
            start_world_track();
        }
        g_transition_reason = TRANSITION_NONE;
    }
}

static void update_transition(float delta_time)
{
    const float safe_delta_time = delta_time > 0.0f ? delta_time : 0.0f;

    if (g_fading_out)
    {
        g_transition_timer += safe_delta_time;
        const float progress =
            SDL_clamp(g_transition_timer / k_transition_duration_seconds, 0.0f, 1.0f);
        const int volume = (int)((1.0f - progress) * (float)k_max_music_volume);
        set_music_volume(volume);

        if (progress >= 1.0f)
        {
            g_fading_out = false;
            g_transition_timer = 0.0f;

            if (g_transition_reason == TRANSITION_NEXT_WORLD_TRACK && g_world_track_count > 0)
            {
                g_world_track_index = (g_world_track_index + 1) % g_world_track_count;
            }

            if (g_target_mode == MUSIC_MODE_MENU)
            {
                start_menu_track();
            }
            else
            {
                start_world_track();
            }

            g_transition_reason = TRANSITION_NONE;
        }

        return;
    }

    if (g_fading_in && g_track_open)
    {
        g_transition_timer += safe_delta_time;
        const float progress =
            SDL_clamp(g_transition_timer / k_transition_duration_seconds, 0.0f, 1.0f);
        const int volume = (int)(progress * (float)k_max_music_volume);
        set_music_volume(volume);

        if (progress >= 1.0f)
        {
            g_fading_in = false;
            g_transition_timer = 0.0f;
            set_music_volume(k_max_music_volume);
        }
    }
}

bool music_player_start_main_menu_music(void)
{
    g_player_initialized = true;
    g_suspended = false;
    g_current_mode = MUSIC_MODE_MENU;
    g_target_mode = MUSIC_MODE_MENU;
    g_fading_out = false;
    g_fading_in = false;
    g_transition_reason = TRANSITION_NONE;
    g_transition_timer = 0.0f;
    g_current_base_volume = k_max_music_volume;
    g_current_effective_volume = -1;
    g_volume_control_supported = true;
    return start_menu_track();
}

void music_player_update(bool in_world, float delta_time)
{
    if (!g_player_initialized)
    {
        if (!music_player_start_main_menu_music())
        {
            return;
        }
    }

    if (g_suspended)
    {
        set_music_volume(0);
    }

    const MusicMode desired_mode = in_world ? MUSIC_MODE_WORLD : MUSIC_MODE_MENU;
    if (!g_fading_out && !g_fading_in && desired_mode != g_current_mode)
    {
        begin_transition(TRANSITION_MODE_SWITCH, desired_mode);
    }

    if (!g_fading_out && !g_fading_in && g_current_mode == MUSIC_MODE_WORLD && g_track_open &&
        g_world_track_count > 0)
    {
        int position_ms = 0;
        int length_ms = 0;
        if (get_current_track_progress_ms(&position_ms, &length_ms))
        {
            const int remaining_ms = length_ms - position_ms;
            if (remaining_ms <= (int)(k_transition_duration_seconds * 1000.0f))
            {
                begin_transition(TRANSITION_NEXT_WORLD_TRACK, MUSIC_MODE_WORLD);
            }
        }
        else if (!is_track_playing())
        {
            begin_transition(TRANSITION_NEXT_WORLD_TRACK, MUSIC_MODE_WORLD);
        }
    }

    if (!g_fading_out && !g_fading_in && g_track_open && !is_track_playing())
    {
        if (g_current_mode == MUSIC_MODE_MENU)
        {
            start_menu_track();
        }
        else
        {
            begin_transition(TRANSITION_NEXT_WORLD_TRACK, MUSIC_MODE_WORLD);
        }
    }

    update_transition(delta_time);
}

void music_player_set_suspended(bool suspended)
{
    g_suspended = suspended;
    if (g_suspended)
    {
        set_music_volume(0);
    }
}

void music_player_update_volume(void)
{
    if (!g_player_initialized || !g_track_open)
    {
        return;
    }

    set_music_volume(g_current_base_volume);
}

void music_player_shutdown(void)
{
    close_current_track();
    g_player_initialized = false;
    g_suspended = false;
    g_fading_out = false;
    g_fading_in = false;
    g_transition_reason = TRANSITION_NONE;
    g_transition_timer = 0.0f;
    g_current_base_volume = 0;
    g_current_effective_volume = -1;
    g_volume_control_supported = true;
}

#else

bool music_player_start_main_menu_music(void)
{
    return false;
}

void music_player_update(bool in_world, float delta_time)
{
    (void)in_world;
    (void)delta_time;
}

void music_player_set_suspended(bool suspended)
{
    (void)suspended;
}

void music_player_update_volume(void)
{
}

void music_player_shutdown(void)
{
}

#endif
