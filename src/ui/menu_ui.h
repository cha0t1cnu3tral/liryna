#ifndef LIRYNA_MENU_UI_H
#define LIRYNA_MENU_UI_H

#include <stdbool.h>

#include <SDL3/SDL.h>

typedef enum UiScreen
{
    UI_SCREEN_MENU = 0,
    UI_SCREEN_WORLD = 1,
    UI_SCREEN_PLACEHOLDER = 2,
} UiScreen;

typedef enum UiAction
{
    UI_ACTION_NONE = 0,
    UI_ACTION_NEW_WORLD = 1,
    UI_ACTION_EXIT = 2,
} UiAction;

typedef struct UiState
{
    UiScreen screen;
    int selected_menu_index;
    int placeholder_index;
} UiState;

typedef void (*UiAnnounceFn)(const char *text, bool interrupt);

void ui_init(UiState *ui, UiAnnounceFn announce);
void ui_update(UiState *ui, bool up_pressed, bool down_pressed,
               bool activate_pressed, bool back_pressed, UiAction *action,
               UiAnnounceFn announce);
UiScreen ui_screen(const UiState *ui);
void ui_render(const UiState *ui, SDL_Renderer *renderer);

#endif
