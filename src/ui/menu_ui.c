#include "menu_ui.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct MenuItem
{
    const char *label;
} MenuItem;

static const MenuItem k_menu_items[] = {
    {"New world"},
    {"Saved worlds"},
    {"Settings"},
    {"Help"},
    {"Exit"},
};

static const int k_menu_item_count = (int)(sizeof(k_menu_items) / sizeof(k_menu_items[0]));

static bool ui_is_valid(const UiState *ui)
{
    return ui != NULL;
}

static const char *ui_current_menu_label(const UiState *ui)
{
    if (!ui_is_valid(ui) || ui->selected_menu_index < 0 || ui->selected_menu_index >= k_menu_item_count)
    {
        return "";
    }

    return k_menu_items[ui->selected_menu_index].label;
}

static void ui_announce_menu_focus(const UiState *ui, UiAnnounceFn announce, bool interrupt)
{
    if (announce == NULL)
    {
        return;
    }

    char message[96];
    snprintf(message, sizeof(message), "%s.", ui_current_menu_label(ui));
    announce(message, interrupt);
}

static void ui_draw_text_centered(SDL_Renderer *renderer, int y, const char *text)
{
    if (renderer == NULL || text == NULL)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_GetRenderOutputSize(renderer, &width, &height);
    (void)height;

    const float text_width = (float)(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * (int)strlen(text));
    const float x = ((float)width - text_width) * 0.5f;

    SDL_RenderDebugText(renderer, x, (float)y, text);
}

void ui_init(UiState *ui, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    ui->screen = UI_SCREEN_MENU;
    ui->selected_menu_index = 0;
    ui->placeholder_index = 1;

    if (announce != NULL)
    {
        announce("Leryna.", true);
        announce("Main menu.", false);
        ui_announce_menu_focus(ui, announce, false);
    }
}

void ui_update(UiState *ui, bool up_pressed, bool down_pressed,
               bool activate_pressed, bool back_pressed, UiAction *action,
               UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    if (action != NULL)
    {
        *action = UI_ACTION_NONE;
    }

    if (ui->screen == UI_SCREEN_MENU)
    {
        if (up_pressed)
        {
            ui->selected_menu_index--;
            if (ui->selected_menu_index < 0)
            {
                ui->selected_menu_index = k_menu_item_count - 1;
            }
            ui_announce_menu_focus(ui, announce, true);
        }

        if (down_pressed)
        {
            ui->selected_menu_index++;
            if (ui->selected_menu_index >= k_menu_item_count)
            {
                ui->selected_menu_index = 0;
            }
            ui_announce_menu_focus(ui, announce, true);
        }

        if (!activate_pressed)
        {
            return;
        }

        if (ui->selected_menu_index == 0)
        {
            ui->screen = UI_SCREEN_WORLD;
            if (action != NULL)
            {
                *action = UI_ACTION_NEW_WORLD;
            }
            if (announce != NULL)
            {
                announce("Generating new world.", true);
            }
            return;
        }

        if (ui->selected_menu_index == (k_menu_item_count - 1))
        {
            if (action != NULL)
            {
                *action = UI_ACTION_EXIT;
            }
            if (announce != NULL)
            {
                announce("Exiting.", true);
            }
            return;
        }

        ui->screen = UI_SCREEN_PLACEHOLDER;
        ui->placeholder_index = ui->selected_menu_index;
        if (announce != NULL)
        {
            char message[128];
            snprintf(message, sizeof(message), "%s page. Placeholder.", ui_current_menu_label(ui));
            announce(message, true);
        }
        return;
    }

    if (back_pressed)
    {
        ui->screen = UI_SCREEN_MENU;
        if (announce != NULL)
        {
            announce("Main menu.", true);
            ui_announce_menu_focus(ui, announce, false);
        }
    }
}

UiScreen ui_screen(const UiState *ui)
{
    if (!ui_is_valid(ui))
    {
        return UI_SCREEN_MENU;
    }

    return ui->screen;
}

void ui_render(const UiState *ui, SDL_Renderer *renderer)
{
    if (!ui_is_valid(ui) || renderer == NULL)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 8, 10, 16, 255);
    SDL_RenderClear(renderer);

    if (ui->screen == UI_SCREEN_MENU)
    {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        ui_draw_text_centered(renderer, 24, "Leryna");

        for (int i = 0; i < k_menu_item_count; i++)
        {
            const int y = 110 + (i * 24);
            if (i == ui->selected_menu_index)
            {
                char focused_line[80];
                snprintf(focused_line, sizeof(focused_line), "> %s <", k_menu_items[i].label);
                SDL_SetRenderDrawColor(renderer, 255, 230, 120, 255);
                ui_draw_text_centered(renderer, y, focused_line);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 180, 184, 198, 255);
                ui_draw_text_centered(renderer, y, k_menu_items[i].label);
            }
        }
        return;
    }

    if (ui->screen == UI_SCREEN_PLACEHOLDER)
    {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        ui_draw_text_centered(renderer, 24, "Leryna");

        SDL_SetRenderDrawColor(renderer, 180, 184, 198, 255);
        ui_draw_text_centered(renderer, 120, k_menu_items[ui->placeholder_index].label);
        ui_draw_text_centered(renderer, 150, "Blank placeholder page");
        ui_draw_text_centered(renderer, 180, "Press Escape to return");
    }
}
