#include "music_player.h"

#include "audio_backend.h"
#include "settings.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <miniaudio.h>

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

enum
{
    MAX_WORLD_TRACKS = 32,
};

static const char *k_menu_music_relative_path = "assets/music/mm1 music opening.mp3";
static const char *k_music_relative_directory = "assets/music";
static const float k_transition_duration_seconds = 1.2f;
static const int k_max_music_volume = 500;

static bool g_player_initialized = false;
static bool g_track_open = false;
static bool g_world_tracks_loaded = false;
static bool g_suspended = false;

static ma_sound g_current_sound;
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
    SDL_PathInfo info;
    return path != NULL && path[0] != '\0' &&
           SDL_GetPathInfo(path, &info) &&
           info.type == SDL_PATHTYPE_DIRECTORY;
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

    return SDL_strlcpy(out_path, candidate, out_size) < out_size;
}

static bool try_directory(char *out_path, size_t out_size, const char *candidate)
{
    if (candidate == NULL || out_path == NULL || out_size == 0)
    {
        return false;
    }

    if (!directory_exists(candidate))
    {
        return false;
    }

    return SDL_strlcpy(out_path, candidate, out_size) < out_size;
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

static bool resolve_music_path(char *out_path, size_t out_size, const char *relative_path)
{
    if (out_path == NULL || out_size == 0 || relative_path == NULL)
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

static bool resolve_music_directory(char *out_path, size_t out_size)
{
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
        "../",
        "../../",
        "../../../",
    };

    char candidate[1024];
    bool found = false;
    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     k_music_relative_directory);
        if (try_directory(out_path, out_size, candidate))
        {
            found = true;
            break;
        }
    }

    SDL_free((void *)base_path);
    return found;
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
        if (SDL_strncasecmp(haystack + i, needle, needle_length) == 0)
        {
            return true;
        }
    }

    return false;
}

static bool is_music_file(const char *path)
{
    const char *extension = path != NULL ? strrchr(path, '.') : NULL;
    return extension != NULL &&
           (SDL_strcasecmp(extension, ".wav") == 0 ||
            SDL_strcasecmp(extension, ".mp3") == 0);
}

static bool should_include_world_track(const char *entry)
{
    if (entry == NULL || !is_music_file(entry))
    {
        return false;
    }

    const char *extension = strrchr(entry, '.');
    if (extension == NULL)
    {
        return false;
    }

    if (SDL_strcasecmp(extension, ".mp3") == 0)
    {
        return true;
    }

    if (SDL_strcasecmp(extension, ".wav") == 0)
    {
        char alternate_path[1024];
        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.mp3",
                     (int)(extension - entry), entry);
        return !file_exists(alternate_path);
    }

    return false;
}

static int compare_track_paths(const void *a, const void *b)
{
    const char *left = (const char *)a;
    const char *right = (const char *)b;
    return SDL_strcasecmp(left, right);
}

static bool append_world_track(const char *music_directory, const char *entry)
{
    if (music_directory == NULL || entry == NULL ||
        g_world_track_count >= MAX_WORLD_TRACKS ||
        contains_case_insensitive(entry, "mm1") ||
        !should_include_world_track(entry))
    {
        return false;
    }

    char full_path[1024];
    SDL_snprintf(full_path, sizeof(full_path), "%s/%s", music_directory, entry);

    if (!file_exists(full_path))
    {
        return false;
    }

    SDL_strlcpy(g_world_tracks[g_world_track_count],
                full_path,
                sizeof(g_world_tracks[g_world_track_count]));
    g_world_track_count++;
    return true;
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

    int entry_count = 0;
    char **entries = SDL_GlobDirectory(music_directory, "*", SDL_GLOB_CASEINSENSITIVE,
                                       &entry_count);
    if (entries == NULL)
    {
        fprintf(stderr, "music: failed to enumerate %s: %s\n", music_directory, SDL_GetError());
        return false;
    }

    for (int i = 0; i < entry_count; i++)
    {
        append_world_track(music_directory, entries[i]);
    }
    SDL_free(entries);

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

    ma_sound_stop(&g_current_sound);
    ma_sound_uninit(&g_current_sound);
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
    if (effective_volume == g_current_effective_volume)
    {
        return true;
    }

    ma_sound_set_volume(&g_current_sound,
                        SDL_clamp((float)effective_volume / 1000.0f, 0.0f, 1.0f));
    g_current_effective_volume = effective_volume;
    return true;
}

static bool open_and_play_track(const char *path, bool repeat)
{
    if (path == NULL || path[0] == '\0')
    {
        return false;
    }

    ma_engine *engine = audio_backend_engine();
    if (engine == NULL)
    {
        return false;
    }

    close_current_track();

    const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
    const ma_result result =
        ma_sound_init_from_file(engine, path, flags, NULL, NULL, &g_current_sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "music: failed to load %s: %s\n", path, ma_result_description(result));
        return false;
    }

    g_track_open = true;
    g_current_effective_volume = -1;
    ma_sound_set_looping(&g_current_sound, repeat ? MA_TRUE : MA_FALSE);

    if (!set_music_volume(0))
    {
        close_current_track();
        return false;
    }

    const ma_result start_result = ma_sound_start(&g_current_sound);
    if (start_result != MA_SUCCESS)
    {
        fprintf(stderr, "music: failed to play %s: %s\n", path,
                ma_result_description(start_result));
        close_current_track();
        return false;
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

    float cursor_seconds = 0.0f;
    float length_seconds = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&g_current_sound, &cursor_seconds) != MA_SUCCESS ||
        ma_sound_get_length_in_seconds(&g_current_sound, &length_seconds) != MA_SUCCESS ||
        length_seconds <= 0.0f)
    {
        return false;
    }

    *out_position_ms = (int)(cursor_seconds * 1000.0f);
    *out_length_ms = (int)(length_seconds * 1000.0f);
    return true;
}

static bool is_track_playing(void)
{
    return g_track_open && ma_sound_is_playing(&g_current_sound);
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
        return;
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
        else if (!is_track_playing() || ma_sound_at_end(&g_current_sound))
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
}
