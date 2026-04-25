#include "opening_scene.h"

#include "audio_backend.h"
#include "settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <miniaudio.h>

typedef struct OpeningSceneTrack
{
    const char *relative_path;
    int base_volume;
    ma_sound sound;
    bool loaded;
} OpeningSceneTrack;

typedef struct OpeningSceneBeat
{
    const char *text;
    bool start_spaceship;
    bool stop_spaceship;
    bool start_alarm;
    bool stop_alarm;
    bool start_crash;
    bool stop_crash;
} OpeningSceneBeat;

static const OpeningSceneBeat g_beats[] = {
    {
        .text = "A ship flies through the sky. You're inside, piloting the ship toward your destination: the planet Niurka, where a few other ships have already been sent.",
        .start_spaceship = true,
    },
    {
        .text = "You're supposed to meet a space station to refuel on the way, but for some reason, you can't get in touch with Henry. You: Hello? Hello? Is there anyone here at all?!",
    },
    {
        .text = "You: This is bad. Like, really bad.",
    },
    {
        .text = "You know you need to find somewhere to land before you run out of fuel. You glance at your dashboard, where several small, planet like dots are blinking.",
    },
    {
        .text = "You: Nice. Planet roulette. Except instead of money or death... it's my new home. Could be the planet of kill-me-now, or something actually livable.",
    },
    {
        .text = "You: Hm. Well... let's pick this one.",
    },
    {
        .text = "You steer toward a random dot. As you do, the fuel indicator begins dropping faster and faster.",
    },
    {
        .start_alarm = true,
    },
    {
        .text = "You: Great. Just perfect. Looks like I'm about to learn what a crash landing feels like... the hard way.",
    },
    {
        .start_crash = true,
        .stop_spaceship = true,
        .stop_alarm = true,
    },
    {
        .text = "You wake up sometime later, your head pounding. The crash knocked you out.",
    },
    {
        .text = "Your ship is an utter wreck of twisted metal and smashed electronics. The only usable items you can spot are a small axe, a pickaxe, and your reader and radio.",
    },
    {
        .text = "You: I should probably grab those... and start figuring out where the hell I am.",
        .stop_crash = true,
    },
};

static const size_t k_beat_count = sizeof(g_beats) / sizeof(g_beats[0]);

static OpeningSceneTrack g_spaceship_track = {
    .relative_path = "assets/sfx/opening scene/spaceship.mp3",
    .base_volume = 330,
};
static OpeningSceneTrack g_alarm_track = {
    .relative_path = "assets/sfx/opening scene/alarm.mp3",
    .base_volume = 760,
};
static OpeningSceneTrack g_crash_track = {
    .relative_path = "assets/sfx/opening scene/crash.mp3",
    .base_volume = 920,
};

static OpeningSceneTrack *g_tracks[] = {
    &g_spaceship_track,
    &g_alarm_track,
    &g_crash_track,
};

static const size_t k_track_count = sizeof(g_tracks) / sizeof(g_tracks[0]);

static bool g_initialized = false;
static bool g_active = false;
static int g_current_beat_index = -1;
static float g_time_to_next_beat = 0.0f;

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

static bool try_alternate_extension(char *out_path,
                                    size_t out_size,
                                    const char *relative_path)
{
    const char *dot = relative_path != NULL ? strrchr(relative_path, '.') : NULL;
    if (dot == NULL)
    {
        return false;
    }

    char alternate_path[1024];
    if (SDL_strcasecmp(dot, ".wav") == 0)
    {
        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.mp3",
                     (int)(dot - relative_path), relative_path);
        if (try_path(out_path, out_size, alternate_path))
        {
            return true;
        }

        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.mp3.mp3",
                     (int)(dot - relative_path), relative_path);
        return try_path(out_path, out_size, alternate_path);
    }

    if (SDL_strcasecmp(dot, ".mp3") == 0)
    {
        SDL_snprintf(alternate_path, sizeof(alternate_path), "%.*s.wav",
                     (int)(dot - relative_path), relative_path);
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

static float track_volume(const OpeningSceneTrack *track)
{
    if (track == NULL)
    {
        return 0.0f;
    }

    const int clamped_base = SDL_clamp(track->base_volume, 0, 1000);
    const int effective_volume = settings_effective_ambience_mci_volume(clamped_base);
    return SDL_clamp((float)effective_volume / 1000.0f, 0.0f, 1.0f);
}

static bool load_track(OpeningSceneTrack *track)
{
    if (track == NULL)
    {
        return false;
    }

    if (track->loaded)
    {
        return true;
    }

    ma_engine *engine = audio_backend_engine();
    if (engine == NULL)
    {
        return false;
    }

    char track_path[1024];
    if (!resolve_track_path(track->relative_path, track_path, sizeof(track_path)))
    {
        fprintf(stderr, "opening_scene: failed to locate %s\n", track->relative_path);
        return false;
    }

    const ma_result result =
        ma_sound_init_from_file(engine, track_path, 0, NULL, NULL, &track->sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "opening_scene: failed to load %s: %s\n",
                track_path, ma_result_description(result));
        return false;
    }

    ma_sound_set_volume(&track->sound, track_volume(track));
    track->loaded = true;
    return true;
}

static void stop_track(OpeningSceneTrack *track)
{
    if (track == NULL || !track->loaded)
    {
        return;
    }

    ma_sound_stop(&track->sound);
    ma_sound_seek_to_pcm_frame(&track->sound, 0);
}

static void play_track(OpeningSceneTrack *track, bool repeat)
{
    if (track == NULL || !track->loaded)
    {
        return;
    }

    stop_track(track);
    ma_sound_set_looping(&track->sound, repeat ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&track->sound, track_volume(track));

    const ma_result result = ma_sound_start(&track->sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "opening_scene: failed to play %s: %s\n",
                track->relative_path, ma_result_description(result));
    }
}

static void unload_track(OpeningSceneTrack *track)
{
    if (track == NULL || !track->loaded)
    {
        return;
    }

    ma_sound_uninit(&track->sound);
    track->loaded = false;
}

static void stop_all_tracks(void)
{
    for (size_t i = 0; i < k_track_count; i++)
    {
        stop_track(g_tracks[i]);
    }
}

static bool beat_has_text(const OpeningSceneBeat *beat)
{
    return beat != NULL && beat->text != NULL && beat->text[0] != '\0';
}

static float beat_duration_seconds(const OpeningSceneBeat *beat)
{
    if (!beat_has_text(beat))
    {
        return 3.0f;
    }

    const float characters = (float)strlen(beat->text);
    const float estimated = 1.8f + (characters * 0.043f);
    return SDL_clamp(estimated, 3.0f, 9.0f);
}

static void apply_beat_audio(const OpeningSceneBeat *beat)
{
    if (beat == NULL)
    {
        return;
    }

    if (beat->stop_alarm)
    {
        stop_track(&g_alarm_track);
    }
    if (beat->stop_spaceship)
    {
        stop_track(&g_spaceship_track);
    }
    if (beat->stop_crash)
    {
        stop_track(&g_crash_track);
    }
    if (beat->start_spaceship)
    {
        play_track(&g_spaceship_track, true);
    }
    if (beat->start_alarm)
    {
        play_track(&g_alarm_track, true);
    }
    if (beat->start_crash)
    {
        play_track(&g_crash_track, false);
    }
}

static void finish_scene(OpeningSceneAnnounceFn announce)
{
    stop_all_tracks();
    g_active = false;
    g_current_beat_index = -1;
    g_time_to_next_beat = 0.0f;

    if (announce != NULL)
    {
        announce("Crash sequence complete. You are in control.", true);
    }
}

static void advance_scene(OpeningSceneAnnounceFn announce, bool skip_silent_beats)
{
    bool keep_advancing = true;
    while (keep_advancing)
    {
        g_current_beat_index++;
        if (g_current_beat_index < 0 || (size_t)g_current_beat_index >= k_beat_count)
        {
            finish_scene(announce);
            return;
        }

        const OpeningSceneBeat *beat = &g_beats[g_current_beat_index];
        apply_beat_audio(beat);
        g_time_to_next_beat = beat_duration_seconds(beat);

        if (announce != NULL && beat_has_text(beat))
        {
            announce(beat->text, true);
        }

        keep_advancing = skip_silent_beats && !beat_has_text(beat);
    }
}

bool opening_scene_init(void)
{
    if (g_initialized)
    {
        return true;
    }

    bool have_track = false;
    for (size_t i = 0; i < k_track_count; i++)
    {
        if (load_track(g_tracks[i]))
        {
            have_track = true;
        }
    }

    g_initialized = true;
    g_active = false;
    g_current_beat_index = -1;
    g_time_to_next_beat = 0.0f;
    return have_track;
}

void opening_scene_start(OpeningSceneAnnounceFn announce)
{
    if (!g_initialized)
    {
        opening_scene_init();
    }

    stop_all_tracks();
    g_active = true;
    g_current_beat_index = -1;
    g_time_to_next_beat = 0.0f;

    if (announce != NULL)
    {
        announce("Opening scene. Press Enter to skip ahead.", true);
    }

    advance_scene(announce, false);
}

void opening_scene_update(float delta_time, bool enter_pressed, OpeningSceneAnnounceFn announce)
{
    if (!g_active)
    {
        return;
    }

    if (enter_pressed)
    {
        advance_scene(announce, true);
        return;
    }

    float remaining_time = delta_time > 0.0f ? delta_time : 0.0f;
    while (g_active && remaining_time > 0.0f)
    {
        if (g_time_to_next_beat > remaining_time)
        {
            g_time_to_next_beat -= remaining_time;
            remaining_time = 0.0f;
            continue;
        }

        remaining_time -= SDL_max(g_time_to_next_beat, 0.0f);
        advance_scene(announce, false);
    }
}

bool opening_scene_is_active(void)
{
    return g_active;
}

void opening_scene_cancel(void)
{
    if (!g_active)
    {
        return;
    }

    stop_all_tracks();
    g_active = false;
    g_current_beat_index = -1;
    g_time_to_next_beat = 0.0f;
}

void opening_scene_shutdown(void)
{
    stop_all_tracks();
    for (size_t i = 0; i < k_track_count; i++)
    {
        unload_track(g_tracks[i]);
    }

    g_active = false;
    g_current_beat_index = -1;
    g_time_to_next_beat = 0.0f;
    g_initialized = false;
}
