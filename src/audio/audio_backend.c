#include "audio_backend.h"

#include <stdio.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

static ma_engine g_audio_engine;
static bool g_audio_engine_ready = false;

bool audio_backend_init(void)
{
    if (g_audio_engine_ready)
    {
        return true;
    }

    const ma_result result = ma_engine_init(NULL, &g_audio_engine);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "audio_backend: miniaudio initialization failed: %s\n",
                ma_result_description(result));
        return false;
    }

    g_audio_engine_ready = true;
    return true;
}

void audio_backend_shutdown(void)
{
    if (!g_audio_engine_ready)
    {
        return;
    }

    ma_engine_uninit(&g_audio_engine);
    g_audio_engine_ready = false;
}

bool audio_backend_is_ready(void)
{
    return g_audio_engine_ready;
}

const char *audio_backend_name(void)
{
    return "miniaudio";
}

ma_engine *audio_backend_engine(void)
{
    return g_audio_engine_ready ? &g_audio_engine : NULL;
}
