#ifndef LIRYNA_UI_H
#define LIRYNA_UI_H

#include <stdbool.h>

typedef enum UiScreen
{
    UI_SCREEN_MENU = 0,
    UI_SCREEN_WORLD,
    UI_SCREEN_SAVED_WORLDS,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_HELP,
} UiScreen;

typedef enum UiAction
{
    UI_ACTION_NONE = 0,
    UI_ACTION_NEW_WORLD,
    UI_ACTION_EXIT,
    UI_ACTION_OPEN_SAVED_WORLDS,
    UI_ACTION_OPEN_SETTINGS,
    UI_ACTION_OPEN_HELP,
    UI_ACTION_BACK,
} UiAction;

typedef struct UiState
{
    UiScreen screen;
    int focused_index;
    UiScreen screen_stack[8];
    int screen_stack_count;
} UiState;

typedef void (*UiAnnounceFn)(const char *text, bool interrupt);

void ui_init(UiState *ui, UiAnnounceFn announce);
void ui_update(UiState *ui, bool up_pressed, bool down_pressed,
               bool activate_pressed, bool back_pressed, UiAction *action,
               UiAnnounceFn announce);
UiScreen ui_screen(const UiState *ui);
void ui_show_screen(UiState *ui, UiScreen screen, UiAnnounceFn announce);

#endif
