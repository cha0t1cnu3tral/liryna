#include "speech.h"

#include <prism.h>

#include <chrono>
#include <cstdio>
#include <thread>

static PrismContext *speech_context = nullptr;
static PrismBackend *speech_backend = nullptr;

static void speech_reset(void)
{
    speech_backend = nullptr;
    speech_context = nullptr;
}

bool speech_init(void)
{
    if (speech_backend)
    {
        return true;
    }

    PrismConfig config = prism_config_init();
    speech_context = prism_init(&config);
    if (!speech_context)
    {
        std::fprintf(stderr, "speech: failed to initialize Prism\n");
        return false;
    }

    speech_backend = prism_registry_acquire_best(speech_context);
    if (!speech_backend)
    {
        std::fprintf(stderr, "speech: no Prism backend is available\n");
        prism_shutdown(speech_context);
        speech_reset();
        return false;
    }

    return true;
}

void speech_shutdown(void)
{
    if (speech_backend)
    {
        PrismError err = prism_backend_stop(speech_backend);
        (void)err;
        prism_backend_free(speech_backend);
        speech_backend = nullptr;
    }

    if (speech_context)
    {
        prism_shutdown(speech_context);
        speech_context = nullptr;
    }
}

bool speech_say(const char *text, bool interrupt)
{
    if (!speech_backend || !text)
    {
        return false;
    }

    PrismError err = prism_backend_speak(speech_backend, text, interrupt);
    if (err != PRISM_OK)
    {
        std::fprintf(stderr, "speech: speak failed: %s\n",
                     prism_error_string(err));
        return false;
    }

    return true;
}

bool speech_output(const char *text, bool interrupt)
{
    if (!speech_backend || !text)
    {
        return false;
    }

    PrismError err = prism_backend_output(speech_backend, text, interrupt);
    if (err != PRISM_OK)
    {
        std::fprintf(stderr, "speech: output failed: %s\n",
                     prism_error_string(err));
        return false;
    }

    return true;
}

void speech_wait(int timeout_ms)
{
    if (!speech_backend || timeout_ms <= 0)
    {
        return;
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    bool queried_state = false;

    while (std::chrono::steady_clock::now() < deadline)
    {
        bool speaking = false;
        PrismError err = prism_backend_is_speaking(speech_backend, &speaking);

        if (err == PRISM_OK)
        {
            queried_state = true;
            if (!speaking)
            {
                return;
            }
        }
        else if (err == PRISM_ERROR_NOT_IMPLEMENTED)
        {
            break;
        }
        else
        {
            std::fprintf(stderr, "speech: state query failed: %s\n",
                         prism_error_string(err));
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!queried_state)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
}

void speech_stop(void)
{
    if (speech_backend)
    {
        PrismError err = prism_backend_stop(speech_backend);
        (void)err;
    }
}

const char *speech_backend_name(void)
{
    if (!speech_backend)
    {
        return "none";
    }

    const char *name = prism_backend_name(speech_backend);
    return name ? name : "unknown";
}
