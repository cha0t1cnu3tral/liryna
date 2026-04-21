#include "ui.h"
#include "screens/screen_registry.h"

#include <stdio.h>

typedef struct UiFocusSearch
{
    int target_index;
    int current_index;
    const UiWidget *result;
} UiFocusSearch;

typedef struct UiContainerSearch
{
    int target_index;
    int current_index;
    const UiWidget *result;
    const UiWidget *root;
} UiContainerSearch;

static const char *ui_widget_type_name(UiWidgetType type)
{
    switch (type)
    {
    case UI_WIDGET_BUTTON:
        return "button";
    case UI_WIDGET_CONTAINER:
    default:
        return "container";
    }
}

static bool ui_is_external_action(UiAction action)
{
    return action == UI_ACTION_NEW_WORLD || action == UI_ACTION_EXIT;
}

static bool ui_is_valid(const UiState *ui)
{
    return ui != NULL;
}

static bool ui_widget_can_focus(const UiWidget *widget)
{
    return widget != NULL && widget->focusable;
}

static bool ui_widget_is_container(const UiWidget *widget)
{
    return widget != NULL && widget->type == UI_WIDGET_CONTAINER;
}

static void ui_describe_widget(const UiWidget *widget, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return;
    }

    if (widget == NULL)
    {
        snprintf(out, out_size, "Unavailable.");
        return;
    }

    const char *label = widget->label != NULL ? widget->label : "Unnamed";
    const char *type_name = ui_widget_type_name(widget->type);

    if (!widget->enabled)
    {
        if (widget->value != NULL && widget->value[0] != '\0')
        {
            snprintf(out, out_size, "%s, %s, unavailable, %s.",
                     label, widget->value, type_name);
        }
        else
        {
            snprintf(out, out_size, "%s, unavailable, %s.", label, type_name);
        }
        return;
    }

    if (widget->value != NULL && widget->value[0] != '\0')
    {
        snprintf(out, out_size, "%s, %s, %s.", label, widget->value, type_name);
        return;
    }

    snprintf(out, out_size, "%s, %s.", label, type_name);
}

static int ui_count_focusable_widgets(const UiWidget *widget)
{
    if (widget == NULL)
    {
        return 0;
    }

    int count = ui_widget_can_focus(widget) ? 1 : 0;

    for (int i = 0; i < widget->child_count; i++)
    {
        count += ui_count_focusable_widgets(&widget->children[i]);
    }

    return count;
}

static int ui_count_switchable_containers(const UiWidget *widget, const UiWidget *root)
{
    if (widget == NULL)
    {
        return 0;
    }

    int count = 0;
    if (widget != root && ui_widget_is_container(widget) &&
        ui_count_focusable_widgets(widget) > 0)
    {
        count++;
    }

    for (int i = 0; i < widget->child_count; i++)
    {
        count += ui_count_switchable_containers(&widget->children[i], root);
    }

    return count;
}

static void ui_find_switchable_container(const UiWidget *widget,
                                         UiContainerSearch *search)
{
    if (widget == NULL || search == NULL || search->result != NULL)
    {
        return;
    }

    if (widget != search->root && ui_widget_is_container(widget) &&
        ui_count_focusable_widgets(widget) > 0)
    {
        if (search->current_index == search->target_index)
        {
            search->result = widget;
            return;
        }

        search->current_index++;
    }

    for (int i = 0; i < widget->child_count; i++)
    {
        ui_find_switchable_container(&widget->children[i], search);
    }
}

static int ui_container_count_for_root(const UiWidget *root)
{
    const int nested_count = ui_count_switchable_containers(root, root);
    return nested_count > 0 ? nested_count : (ui_count_focusable_widgets(root) > 0 ? 1 : 0);
}

static const UiWidget *ui_get_container_for_root(const UiWidget *root, int index)
{
    if (root == NULL)
    {
        return NULL;
    }

    const int nested_count = ui_count_switchable_containers(root, root);
    if (nested_count <= 0)
    {
        return ui_count_focusable_widgets(root) > 0 ? root : NULL;
    }

    UiContainerSearch search = {
        .target_index = index,
        .current_index = 0,
        .result = NULL,
        .root = root,
    };

    ui_find_switchable_container(root, &search);
    return search.result;
}

static void ui_find_focusable_widget(const UiWidget *widget, UiFocusSearch *search)
{
    if (widget == NULL || search == NULL || search->result != NULL)
    {
        return;
    }

    if (ui_widget_can_focus(widget))
    {
        if (search->current_index == search->target_index)
        {
            search->result = widget;
            return;
        }

        search->current_index++;
    }

    for (int i = 0; i < widget->child_count; i++)
    {
        ui_find_focusable_widget(&widget->children[i], search);
    }
}

static const UiWidget *ui_get_focusable_widget(const UiWidget *container, int index)
{
    UiFocusSearch search = {
        .target_index = index,
        .current_index = 0,
        .result = NULL,
    };

    ui_find_focusable_widget(container, &search);
    return search.result;
}

static const UiWidget *ui_get_focused_container(const UiState *ui,
                                                const UiScreenDefinition *screen)
{
    if (!ui_is_valid(ui) || screen == NULL)
    {
        return NULL;
    }

    return ui_get_container_for_root(screen->root, ui->focused_container_index);
}

static const UiWidget *ui_get_focused_widget(const UiState *ui,
                                             const UiScreenDefinition *screen)
{
    const UiWidget *container = ui_get_focused_container(ui, screen);
    if (container == NULL)
    {
        return NULL;
    }

    return ui_get_focusable_widget(container, ui->focused_widget_index);
}

static void ui_announce_focus(const UiState *ui, UiAnnounceFn announce, bool interrupt)
{
    if (!ui_is_valid(ui) || announce == NULL)
    {
        return;
    }

    const UiScreenDefinition *screen = ui_get_screen_definition(ui->screen);
    if (screen == NULL)
    {
        return;
    }

    const UiWidget *widget = ui_get_focused_widget(ui, screen);
    if (widget == NULL)
    {
        return;
    }

    char message[160];
    ui_describe_widget(widget, message, sizeof(message));
    announce(message, interrupt);
}

static void ui_announce_screen(const UiState *ui, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui) || announce == NULL)
    {
        return;
    }

    const UiScreenDefinition *screen = ui_get_screen_definition(ui->screen);
    if (screen == NULL || screen->title == NULL)
    {
        return;
    }

    char message[96];
    snprintf(message, sizeof(message), "%s.", screen->title);
    announce(message, true);
    ui_announce_focus(ui, announce, false);
}

static void ui_reset_focus(UiState *ui)
{
    if (ui != NULL)
    {
        ui->focused_container_index = 0;
        ui->focused_widget_index = 0;
    }
}

static void ui_set_screen_internal(UiState *ui, UiScreen screen, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    ui->screen = screen;
    ui_reset_focus(ui);
    ui_announce_screen(ui, announce);
}

static void ui_push_screen(UiState *ui, UiScreen screen, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    if (ui->screen_stack_count < (int)(sizeof(ui->screen_stack) / sizeof(ui->screen_stack[0])))
    {
        ui->screen_stack[ui->screen_stack_count] = ui->screen;
        ui->screen_stack_count++;
    }

    ui_set_screen_internal(ui, screen, announce);
}

static void ui_pop_screen(UiState *ui, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    if (ui->screen_stack_count > 0)
    {
        ui->screen_stack_count--;
        ui_set_screen_internal(ui, ui->screen_stack[ui->screen_stack_count], announce);
        return;
    }

    if (ui->screen != UI_SCREEN_MENU)
    {
        ui_set_screen_internal(ui, UI_SCREEN_MENU, announce);
    }
}

static bool ui_handle_internal_action(UiState *ui, UiAction selected_action,
                                      UiAnnounceFn announce)
{
    switch (selected_action)
    {
    case UI_ACTION_OPEN_SAVED_WORLDS:
        ui_push_screen(ui, UI_SCREEN_SAVED_WORLDS, announce);
        return true;
    case UI_ACTION_OPEN_SETTINGS:
        ui_push_screen(ui, UI_SCREEN_SETTINGS, announce);
        return true;
    case UI_ACTION_OPEN_HELP:
        ui_push_screen(ui, UI_SCREEN_HELP, announce);
        return true;
    case UI_ACTION_BACK:
        ui_pop_screen(ui, announce);
        return true;
    case UI_ACTION_NONE:
    case UI_ACTION_NEW_WORLD:
    case UI_ACTION_EXIT:
    default:
        return false;
    }
}

void ui_init(UiState *ui, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    ui->screen = UI_SCREEN_MENU;
    ui->focused_container_index = 0;
    ui->focused_widget_index = 0;
    ui->screen_stack_count = 0;

    if (announce != NULL)
    {
        announce("Liryna.", true);
        ui_announce_screen(ui, announce);
    }
}

void ui_update(UiState *ui, bool up_pressed, bool down_pressed,
               bool next_container_pressed, bool previous_container_pressed,
               bool activate_pressed, bool back_pressed,
               UiAction *action, UiAnnounceFn announce)
{
    if (action != NULL)
    {
        *action = UI_ACTION_NONE;
    }

    if (!ui_is_valid(ui))
    {
        return;
    }

    if (ui->screen == UI_SCREEN_WORLD)
    {
        if (back_pressed)
        {
            ui_pop_screen(ui, announce);
        }
        return;
    }

    const UiScreenDefinition *screen = ui_get_screen_definition(ui->screen);
    if (screen == NULL)
    {
        return;
    }

    const int container_count = ui_container_count_for_root(screen->root);
    if (container_count <= 0)
    {
        if (back_pressed)
        {
            ui_pop_screen(ui, announce);
        }
        return;
    }

    if (ui->focused_container_index < 0 || ui->focused_container_index >= container_count)
    {
        ui->focused_container_index = 0;
        ui->focused_widget_index = 0;
    }

    const UiWidget *container = ui_get_focused_container(ui, screen);
    int focusable_count = ui_count_focusable_widgets(container);
    if (focusable_count <= 0)
    {
        ui->focused_container_index = 0;
        ui->focused_widget_index = 0;
        container = ui_get_focused_container(ui, screen);
        focusable_count = ui_count_focusable_widgets(container);
        if (focusable_count <= 0)
        {
            return;
        }
    }

    if (ui->focused_widget_index < 0 || ui->focused_widget_index >= focusable_count)
    {
        ui->focused_widget_index = 0;
    }

    if (next_container_pressed && container_count > 1)
    {
        ui->focused_container_index++;
        if (ui->focused_container_index >= container_count)
        {
            ui->focused_container_index = 0;
        }
        ui->focused_widget_index = 0;
        ui_announce_focus(ui, announce, true);
        return;
    }

    if (previous_container_pressed && container_count > 1)
    {
        ui->focused_container_index--;
        if (ui->focused_container_index < 0)
        {
            ui->focused_container_index = container_count - 1;
        }
        ui->focused_widget_index = 0;
        ui_announce_focus(ui, announce, true);
        return;
    }

    if (up_pressed)
    {
        ui->focused_widget_index--;
        if (ui->focused_widget_index < 0)
        {
            ui->focused_widget_index = focusable_count - 1;
        }
        ui_announce_focus(ui, announce, true);
    }

    if (down_pressed)
    {
        ui->focused_widget_index++;
        if (ui->focused_widget_index >= focusable_count)
        {
            ui->focused_widget_index = 0;
        }
        ui_announce_focus(ui, announce, true);
    }

    if (back_pressed)
    {
        ui_pop_screen(ui, announce);
        return;
    }

    if (!activate_pressed)
    {
        return;
    }

    const UiWidget *widget = ui_get_focused_widget(ui, screen);
    if (widget == NULL || !widget->enabled)
    {
        ui_announce_focus(ui, announce, true);
        return;
    }

    if (ui_handle_internal_action(ui, widget->action, announce))
    {
        return;
    }

    if (action != NULL && ui_is_external_action(widget->action))
    {
        *action = widget->action;
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

void ui_show_screen(UiState *ui, UiScreen screen, UiAnnounceFn announce)
{
    if (!ui_is_valid(ui))
    {
        return;
    }

    if (screen == UI_SCREEN_WORLD)
    {
        if (ui->screen_stack_count < (int)(sizeof(ui->screen_stack) / sizeof(ui->screen_stack[0])))
        {
            ui->screen_stack[ui->screen_stack_count] = ui->screen;
            ui->screen_stack_count++;
        }
        ui->screen = UI_SCREEN_WORLD;
        ui_reset_focus(ui);
        return;
    }

    ui_set_screen_internal(ui, screen, announce);
}
