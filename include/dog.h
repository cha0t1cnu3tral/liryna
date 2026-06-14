#ifndef LIRYNA_DOG_H
#define LIRYNA_DOG_H

#include <stdbool.h>
#include <stddef.h>

#include <SDL3/SDL.h>

typedef struct World World;

bool dog_system_init(void);
void dog_system_reset(void);
void dog_system_shutdown(void);
int dog_system_populate_world(const World *world);
void dog_system_update(const World *world, float delta_time);
void dog_system_render(const World *world, SDL_Renderer *renderer);

bool dog_system_item_has_action(int item);
bool dog_system_use_item(const World *world,
                         int item,
                         int player_tile_x,
                         int player_tile_y,
                         int facing_x,
                         int facing_y,
                         char *message,
                         size_t message_size,
                         bool *out_consumed);
bool dog_system_describe_at(const World *world,
                            int tile_x,
                            int tile_y,
                            char *message,
                            size_t message_size);

#endif
