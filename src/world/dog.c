#include "dog.h"

#include "audio_backend.h"
#include "tiles.h"
#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL_iostream.h>
#include <miniaudio.h>

enum
{
    DOG_CAPACITY = 24
};

typedef struct Dog
{
    bool alive;
    float x;
    float y;
    float health;
    float hunger;
    float age_seconds;
    float move_target_x;
    float move_target_y;
    float decision_timer;
    float bark_timer;
    float fear_timer;
    unsigned int personality;
} Dog;

static Dog g_dogs[DOG_CAPACITY];
static ma_sound g_bark_sound;
static bool g_bark_initialized = false;
static unsigned int g_rng_state = 0xD06B4A5u;

static const float k_dog_max_health = 40.0f;
static const float k_dog_max_hunger = 100.0f;
static const char *const k_bark_path = "assets/sfx/mpcs/dog_bark.wav";

static float dog_random_unit(void)
{
    g_rng_state = (1664525u * g_rng_state) + 1013904223u;
    return (float)(g_rng_state >> 8) / (float)0x00FFFFFFu;
}

static float dog_distance(float x1, float y1, float x2, float y2)
{
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    return sqrtf((dx * dx) + (dy * dy));
}

static bool dog_try_path(char *out_path, size_t out_size, const char *candidate)
{
    if (out_path == NULL || out_size == 0 || candidate == NULL ||
        SDL_strlcpy(out_path, candidate, out_size) >= out_size)
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

static bool dog_resolve_asset_path(const char *relative_path, char *out_path, size_t out_size)
{
    if (dog_try_path(out_path, out_size, relative_path))
    {
        return true;
    }

    const char *base_path = SDL_GetBasePath();
    if (base_path == NULL)
    {
        return false;
    }

    static const char *const k_prefixes[] = {"", "../", "../../", "../../../"};
    bool found = false;
    char candidate[1024];
    for (size_t i = 0; i < sizeof(k_prefixes) / sizeof(k_prefixes[0]); i++)
    {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base_path, k_prefixes[i],
                     relative_path);
        if (dog_try_path(out_path, out_size, candidate))
        {
            found = true;
            break;
        }
    }

    SDL_free((void *)base_path);
    return found;
}

static void dog_play_bark(const World *world, const Dog *dog)
{
    if (!g_bark_initialized || world == NULL || dog == NULL || world->tile_size <= 0)
    {
        return;
    }

    const float hearing_range = (float)world->tile_size * 6.0f;
    const float distance = dog_distance(dog->x, dog->y, world->player_x, world->player_y);
    if (distance > hearing_range)
    {
        return;
    }

    const float volume = SDL_clamp(1.0f - (distance / hearing_range), 0.12f, 0.9f);
    const float pan = SDL_clamp((dog->x - world->player_x) / hearing_range, -0.9f, 0.9f);
    ma_sound_stop(&g_bark_sound);
    ma_sound_seek_to_pcm_frame(&g_bark_sound, 0);
    ma_sound_set_volume(&g_bark_sound, volume);
    ma_sound_set_pan(&g_bark_sound, pan);
    ma_sound_set_pitch(&g_bark_sound, 0.92f + (dog_random_unit() * 0.18f));
    (void)ma_sound_start(&g_bark_sound);
}

static bool dog_can_move_to(const World *world, float x, float y)
{
    if (world == NULL || world->tile_size <= 0)
    {
        return false;
    }

    const int tile_x = (int)(x / (float)world->tile_size);
    const int tile_y = (int)(y / (float)world->tile_size);
    return world_can_occupy_tile(world, tile_x, tile_y, NULL, NULL, NULL);
}

static void dog_choose_wander_target(const World *world, Dog *dog)
{
    const float radius = (float)world->tile_size * (1.0f + (dog_random_unit() * 3.0f));
    const float angle = dog_random_unit() * 6.2831853f;
    const float target_x = dog->x + (cosf(angle) * radius);
    const float target_y = dog->y + (sinf(angle) * radius);
    if (dog_can_move_to(world, target_x, target_y))
    {
        dog->move_target_x = target_x;
        dog->move_target_y = target_y;
    }
    dog->decision_timer = 1.5f + (dog_random_unit() * 3.5f);
}

static void dog_update_movement(const World *world, Dog *dog, float delta_time)
{
    const float player_distance =
        dog_distance(dog->x, dog->y, world->player_x, world->player_y);
    const float tile_size = (float)world->tile_size;
    float speed = tile_size * 2.2f;

    dog->decision_timer -= delta_time;
    if (dog->fear_timer > 0.0f)
    {
        dog->fear_timer -= delta_time;
        const float away_x = dog->x - world->player_x;
        const float away_y = dog->y - world->player_y;
        const float length = sqrtf((away_x * away_x) + (away_y * away_y));
        if (length > 0.001f)
        {
            dog->move_target_x = dog->x + ((away_x / length) * tile_size * 4.0f);
            dog->move_target_y = dog->y + ((away_y / length) * tile_size * 4.0f);
        }
        speed = tile_size * 3.5f;
    }
    else if (player_distance > tile_size * 3.0f && player_distance < tile_size * 10.0f)
    {
        dog->move_target_x = world->player_x;
        dog->move_target_y = world->player_y;
        speed = tile_size * 3.0f;
    }
    else if (dog->decision_timer <= 0.0f ||
             dog_distance(dog->x, dog->y, dog->move_target_x, dog->move_target_y) <
                 tile_size * 0.25f)
    {
        dog_choose_wander_target(world, dog);
    }

    const float dx = dog->move_target_x - dog->x;
    const float dy = dog->move_target_y - dog->y;
    const float length = sqrtf((dx * dx) + (dy * dy));
    if (length <= 0.001f)
    {
        return;
    }

    const float step = SDL_min(speed * delta_time, length);
    const float next_x = dog->x + ((dx / length) * step);
    const float next_y = dog->y + ((dy / length) * step);
    if (dog_can_move_to(world, next_x, dog->y))
    {
        dog->x = next_x;
    }
    if (dog_can_move_to(world, dog->x, next_y))
    {
        dog->y = next_y;
    }
}

static bool dog_biome_allows_natural_spawn(BiomeType biome)
{
    return biome == BIOME_PLAINS ||
           biome == BIOME_FOREST ||
           biome == BIOME_DRY_PLAINS_STEPPE ||
           biome == BIOME_HILLS ||
           biome == BIOME_COAST;
}

static bool dog_has_neighbor(float x, float y, float minimum_distance)
{
    for (int i = 0; i < DOG_CAPACITY; i++)
    {
        if (g_dogs[i].alive &&
            dog_distance(x, y, g_dogs[i].x, g_dogs[i].y) < minimum_distance)
        {
            return true;
        }
    }

    return false;
}

static bool dog_spawn(float x, float y, float hunger, float bark_delay)
{
    for (int i = 0; i < DOG_CAPACITY; i++)
    {
        if (g_dogs[i].alive)
        {
            continue;
        }

        g_dogs[i] = (Dog){
            .alive = true,
            .x = x,
            .y = y,
            .health = k_dog_max_health,
            .hunger = SDL_clamp(hunger, 0.0f, k_dog_max_hunger),
            .move_target_x = x,
            .move_target_y = y,
            .decision_timer = 0.5f + (dog_random_unit() * 2.0f),
            .bark_timer = bark_delay,
            .personality = (unsigned int)(dog_random_unit() * 10000.0f),
        };
        return true;
    }

    return false;
}

bool dog_system_init(void)
{
    dog_system_shutdown();
    dog_system_reset();

    ma_engine *engine = audio_backend_engine();
    char path[1024];
    if (engine == NULL || !dog_resolve_asset_path(k_bark_path, path, sizeof(path)))
    {
        fprintf(stderr, "dog: failed to locate %s\n", k_bark_path);
        return false;
    }

    const ma_result result =
        ma_sound_init_from_file(engine, path, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                NULL, NULL, &g_bark_sound);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr, "dog: failed to load bark sound: %s\n", ma_result_description(result));
        return false;
    }

    g_bark_initialized = true;
    return true;
}

void dog_system_reset(void)
{
    memset(g_dogs, 0, sizeof(g_dogs));
}

void dog_system_shutdown(void)
{
    dog_system_reset();
    if (g_bark_initialized)
    {
        ma_sound_stop(&g_bark_sound);
        ma_sound_uninit(&g_bark_sound);
        g_bark_initialized = false;
    }
}

int dog_system_populate_world(const World *world)
{
    if (world == NULL || world->tile_size <= 0 || world->width <= 0 ||
        world->height <= 0 || world->biomes == NULL)
    {
        return 0;
    }

    dog_system_reset();
    const int target_count = 8 + (int)(dog_random_unit() * 7.0f);
    const float tile_size = (float)world->tile_size;
    const float spawn_exclusion = tile_size * 10.0f;
    const float dog_spacing = tile_size * 5.0f;
    int spawned = 0;

    for (int attempt = 0; attempt < 6000 && spawned < target_count; attempt++)
    {
        const int tile_x = 2 + (int)(dog_random_unit() * (float)SDL_max(1, world->width - 4));
        const int tile_y = 2 + (int)(dog_random_unit() * (float)SDL_max(1, world->height - 4));
        const int index = (tile_y * world->width) + tile_x;
        if (!world_is_in_bounds(world, tile_x, tile_y) ||
            !dog_biome_allows_natural_spawn(world->biomes[index]))
        {
            continue;
        }

        bool swimming = false;
        if (!world_can_occupy_tile(world, tile_x, tile_y, &swimming, NULL, NULL) || swimming)
        {
            continue;
        }

        const float x = (float)(tile_x * world->tile_size);
        const float y = (float)(tile_y * world->tile_size);
        if (dog_distance(x, y, world->player_x, world->player_y) < spawn_exclusion ||
            dog_has_neighbor(x, y, dog_spacing))
        {
            continue;
        }

        if (dog_spawn(x, y,
                      55.0f + (dog_random_unit() * 45.0f),
                      3.0f + (dog_random_unit() * 15.0f)))
        {
            spawned++;
        }
    }

    return spawned;
}

void dog_system_update(const World *world, float delta_time)
{
    if (world == NULL || world->tile_size <= 0 || delta_time <= 0.0f)
    {
        return;
    }

    const float update_dt = SDL_min(delta_time, 0.1f);
    for (int i = 0; i < DOG_CAPACITY; i++)
    {
        Dog *dog = &g_dogs[i];
        if (!dog->alive)
        {
            continue;
        }

        dog->age_seconds += update_dt;
        dog->hunger = SDL_max(0.0f, dog->hunger - (update_dt * 0.25f));
        if (dog->hunger <= 0.0f)
        {
            dog->health -= update_dt * 2.0f;
            if (dog->health <= 0.0f)
            {
                dog->alive = false;
                continue;
            }
        }

        dog_update_movement(world, dog, update_dt);

        dog->bark_timer -= update_dt;
        const float player_distance =
            dog_distance(dog->x, dog->y, world->player_x, world->player_y);
        if (dog->bark_timer <= 0.0f && player_distance <= world->tile_size * 5.0f)
        {
            dog_play_bark(world, dog);
            dog->bark_timer = 7.0f + (dog_random_unit() * 11.0f);
        }
    }
}

static void dog_render_one(const World *world, SDL_Renderer *renderer, const Dog *dog)
{
    const float size = (float)world->tile_size;
    const float x = dog->x;
    const float y = dog->y;
    const Uint8 body_red = (Uint8)(120 + (dog->personality % 55u));

    SDL_FRect body = {x + (size * 0.18f), y + (size * 0.35f), size * 0.58f, size * 0.38f};
    SDL_FRect head = {x + (size * 0.62f), y + (size * 0.20f), size * 0.30f, size * 0.34f};
    SDL_FRect ear1 = {x + (size * 0.64f), y + (size * 0.10f), size * 0.09f, size * 0.16f};
    SDL_FRect ear2 = {x + (size * 0.80f), y + (size * 0.10f), size * 0.09f, size * 0.16f};
    SDL_FRect leg1 = {x + (size * 0.26f), y + (size * 0.68f), size * 0.10f, size * 0.22f};
    SDL_FRect leg2 = {x + (size * 0.62f), y + (size * 0.68f), size * 0.10f, size * 0.22f};

    SDL_SetRenderDrawColor(renderer, body_red, 78, 38, 255);
    SDL_RenderFillRect(renderer, &body);
    SDL_RenderFillRect(renderer, &head);
    SDL_RenderFillRect(renderer, &leg1);
    SDL_RenderFillRect(renderer, &leg2);
    SDL_SetRenderDrawColor(renderer, 72, 42, 24, 255);
    SDL_RenderFillRect(renderer, &ear1);
    SDL_RenderFillRect(renderer, &ear2);
    SDL_RenderLine(renderer, x + (size * 0.18f), y + (size * 0.42f),
                  x + (size * 0.02f), y + (size * 0.24f));

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_FRect eye = {x + (size * 0.83f), y + (size * 0.29f), size * 0.04f, size * 0.04f};
    SDL_RenderFillRect(renderer, &eye);

    const float health_width = size * SDL_clamp(dog->health / k_dog_max_health, 0.0f, 1.0f);
    const float hunger_width = size * SDL_clamp(dog->hunger / k_dog_max_hunger, 0.0f, 1.0f);
    SDL_FRect health_bar = {x, y - 5.0f, health_width, 2.0f};
    SDL_FRect hunger_bar = {x, y - 2.0f, hunger_width, 2.0f};
    SDL_SetRenderDrawColor(renderer, 205, 45, 45, 255);
    SDL_RenderFillRect(renderer, &health_bar);
    SDL_SetRenderDrawColor(renderer, 225, 175, 35, 255);
    SDL_RenderFillRect(renderer, &hunger_bar);
}

void dog_system_render(const World *world, SDL_Renderer *renderer)
{
    if (world == NULL || renderer == NULL)
    {
        return;
    }

    for (int i = 0; i < DOG_CAPACITY; i++)
    {
        if (g_dogs[i].alive)
        {
            dog_render_one(world, renderer, &g_dogs[i]);
        }
    }
}

bool dog_system_item_has_action(int item)
{
    return item == TILE_DOG_WHISTLE || item == TILE_DOG_FOOD ||
           item == TILE_SMALLAXE || item == TILE_PICKAXE;
}

static Dog *dog_find_near(float x, float y, float range)
{
    Dog *nearest = NULL;
    float nearest_distance = range;
    for (int i = 0; i < DOG_CAPACITY; i++)
    {
        if (!g_dogs[i].alive)
        {
            continue;
        }

        const float distance = dog_distance(x, y, g_dogs[i].x, g_dogs[i].y);
        if (distance <= nearest_distance)
        {
            nearest = &g_dogs[i];
            nearest_distance = distance;
        }
    }
    return nearest;
}

bool dog_system_use_item(const World *world,
                         int item,
                         int player_tile_x,
                         int player_tile_y,
                         int facing_x,
                         int facing_y,
                         char *message,
                         size_t message_size,
                         bool *out_consumed)
{
    if (world == NULL || message == NULL || message_size == 0 || world->tile_size <= 0)
    {
        return false;
    }

    if (out_consumed != NULL)
    {
        *out_consumed = false;
    }

    if (facing_x == 0 && facing_y == 0)
    {
        facing_y = 1;
    }

    if (item == TILE_DOG_WHISTLE)
    {
        const int tile_x = player_tile_x + facing_x;
        const int tile_y = player_tile_y + facing_y;
        bool swimming = false;
        if (!world_can_occupy_tile(world, tile_x, tile_y, &swimming, NULL, NULL) || swimming)
        {
            SDL_snprintf(message, message_size, "The dog needs a clear dry tile.");
            return true;
        }

        const float x = (float)(tile_x * world->tile_size);
        const float y = (float)(tile_y * world->tile_size);
        if (dog_find_near(x, y, world->tile_size * 0.7f) != NULL)
        {
            SDL_snprintf(message, message_size, "There is already a dog there.");
            return true;
        }

        if (dog_spawn(x, y, k_dog_max_hunger, 0.35f))
        {
            SDL_snprintf(message, message_size,
                         "Dog summoned. Health 40. Hunger 100. Feed it Dog Food before it starves.");
            return true;
        }

        SDL_snprintf(message, message_size, "The world already has the maximum of 24 dogs.");
        return true;
    }

    const float player_x = (float)(player_tile_x * world->tile_size);
    const float player_y = (float)(player_tile_y * world->tile_size);
    Dog *dog = dog_find_near(player_x, player_y, world->tile_size * 1.8f);
    if (dog == NULL)
    {
        SDL_snprintf(message, message_size, "No dog is close enough.");
        return true;
    }

    if (item == TILE_DOG_FOOD)
    {
        dog->hunger = SDL_min(k_dog_max_hunger, dog->hunger + 45.0f);
        dog->health = SDL_min(k_dog_max_health, dog->health + 8.0f);
        dog->fear_timer = 0.0f;
        dog->bark_timer = 0.2f;
        if (out_consumed != NULL)
        {
            *out_consumed = true;
        }
        SDL_snprintf(message, message_size, "Dog fed. Health %.0f. Hunger %.0f.",
                     dog->health, dog->hunger);
        return true;
    }

    if (item == TILE_SMALLAXE || item == TILE_PICKAXE)
    {
        const float damage = item == TILE_SMALLAXE ? 12.0f : 9.0f;
        dog->health -= damage;
        dog->fear_timer = 4.0f;
        dog->bark_timer = 0.1f;
        if (dog->health <= 0.0f)
        {
            dog->alive = false;
            SDL_snprintf(message, message_size, "The dog was killed.");
        }
        else
        {
            SDL_snprintf(message, message_size, "Dog hit. Health %.0f. It runs away.",
                         dog->health);
        }
        return true;
    }

    return false;
}

bool dog_system_describe_at(const World *world,
                            int tile_x,
                            int tile_y,
                            char *message,
                            size_t message_size)
{
    if (world == NULL || message == NULL || message_size == 0 || world->tile_size <= 0)
    {
        return false;
    }

    const float x = (float)(tile_x * world->tile_size);
    const float y = (float)(tile_y * world->tile_size);
    Dog *dog = dog_find_near(x, y, world->tile_size * 0.65f);
    if (dog == NULL)
    {
        return false;
    }

    const char *condition = dog->hunger <= 0.0f ? "starving" :
                            dog->hunger < 25.0f ? "very hungry" :
                            dog->hunger < 55.0f ? "hungry" : "well fed";
    SDL_snprintf(message, message_size,
                 "Living dog. Health %.0f of 40. Hunger %.0f of 100, %s.",
                 dog->health, dog->hunger, condition);
    return true;
}
