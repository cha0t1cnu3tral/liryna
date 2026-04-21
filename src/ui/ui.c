#include "ui.h"
#include "screens/screen_registry.h"

#include <stdio.h>
#include <string.h>

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
    case UI_WIDGET_TOGGLE:
        return "toggle";
    case UI_WIDGET_SLIDER:
        return "slider";
    case UI_WIDGET_EDIT_BOX:
        return "edit box";
    case UI_WIDGET_PICKER:
        return "picker";
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

static bool ui_container_is_grid(const UiWidget *container)
{
    return ui_widget_is_container(container) &&
           container->direction == UI_CONTAINER_GRID &&
           container->grid_columns > 0;
}

static int ui_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static bool ui_widget_value_text(const UiWidget *widget, char *out, size_t out_size)
{
    if (widget == NULL || out == NULL || out_size == 0)
    {
        return false;
    }

    out[0] = '\0';

    switch (widget->type)
    {
    case UI_WIDGET_TOGGLE:
        if (widget->toggle_value == NULL)
        {
            return false;
        }
        snprintf(out, out_size, "%s", *widget->toggle_value ? "on" : "off");
        return true;
    case UI_WIDGET_SLIDER:
        if (widget->int_value == NULL)
        {
            return false;
        }
        snprintf(out, out_size, "%d", *widget->int_value);
        return true;
    case UI_WIDGET_EDIT_BOX:
        if (widget->edit_value == NULL || widget->edit_value[0] == '\0')
        {
            snprintf(out, out_size, "empty");
            return true;
        }
        snprintf(out, out_size, "%s", widget->edit_value);
        return true;
    case UI_WIDGET_PICKER:
        if (widget->picker_options == NULL || widget->picker_index == NULL ||
            widget->picker_option_count <= 0)
        {
            return false;
        }
        {
            const int index = ui_clamp_int(*widget->picker_index, 0,
                                           widget->picker_option_count - 1);
            const char *option = widget->picker_options[index];
            snprintf(out, out_size, "%s", option != NULL ? option : "unknown");
        }
        return true;
    case UI_WIDGET_CONTAINER:
    case UI_WIDGET_BUTTON:
    default:
        if (widget->value == NULL || widget->value[0] == '\0')
        {
            return false;
        }
        snprintf(out, out_size, "%s", widget->value);
        return true;
    }
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
    char value[96];
    const bool has_value = ui_widget_value_text(widget, value, sizeof(value));

    if (!widget->enabled)
    {
        if (has_value)
        {
            snprintf(out, out_size, "%s, %s, unavailable, %s.",
                     label, value, type_name);
        }
        else
        {
            snprintf(out, out_size, "%s, unavailable, %s.", label, type_name);
        }
        return;
    }

    if (has_value)
    {
        snprintf(out, out_size, "%s, %s, %s.", label, value, type_name);
        return;
    }

    snprintf(out, out_size, "%s, %s.", label, type_name);
}

static void ui_delete_last_utf8_char(char *text)
{
    if (text == NULL || text[0] == '\0')
    {
        return;
    }

    size_t length = strlen(text);
    if (length == 0)
    {
        return;
    }

    length--;
    while (length > 0 && (((unsigned char)text[length] & 0xC0U) == 0x80U))
    {
        length--;
    }

    text[length] = '\0';
}

static bool ui_edit_box_append_text(const UiWidget *widget, const char *text_input)
{
    if (widget == NULL || widget->type != UI_WIDGET_EDIT_BOX ||
        widget->edit_value == NULL || widget->edit_capacity == 0 ||
        text_input == NULL || text_input[0] == '\0')
    {
        return false;
    }

    const size_t current_length = strlen(widget->edit_value);
    if (current_length >= widget->edit_capacity - 1)
    {
        return false;
    }

    const size_t available = widget->edit_capacity - current_length - 1;
    strncat(widget->edit_value, text_input, available);
    widget->edit_value[widget->edit_capacity - 1] = '\0';
    return true;
}

static bool ui_edit_box_backspace(const UiWidget *widget)
{
    if (widget == NULL || widget->type != UI_WIDGET_EDIT_BOX ||
        widget->edit_value == NULL || widget->edit_value[0] == '\0')
    {
        return false;
    }

    ui_delete_last_utf8_char(widget->edit_value);
    return true;
}

static void ui_announce_widget_value(const UiWidget *widget, UiAnnounceFn announce,
                                     bool interrupt)
{
    if (widget == NULL || announce == NULL)
    {
        return;
    }

    char value[96];
    if (!ui_widget_value_text(widget, value, sizeof(value)))
    {
        return;
    }

    char message[112];
    snprintf(message, sizeof(message), "%s.", value);
    announce(message, interrupt);
}

static bool ui_adjust_widget_value(const UiWidget *widget, int delta, bool activate)
{
    if (widget == NULL || !widget->enabled)
    {
        return false;
    }

    switch (widget->type)
    {
    case UI_WIDGET_TOGGLE:
        if (widget->toggle_value == NULL)
        {
            return false;
        }

        if (activate || delta == 0)
        {
            *widget->toggle_value = !*widget->toggle_value;
        }
        else
        {
            *widget->toggle_value = delta > 0;
        }
        return true;
    case UI_WIDGET_SLIDER:
        if (widget->int_value == NULL || delta == 0)
        {
            return false;
        }
        {
            const int step = widget->step_value > 0 ? widget->step_value : 1;
            const int next_value = *widget->int_value + (delta * step);
            *widget->int_value = ui_clamp_int(next_value, widget->min_value,
                                              widget->max_value);
        }
        return true;
    case UI_WIDGET_PICKER:
        if (widget->picker_index == NULL || widget->picker_option_count <= 0 ||
            delta == 0)
        {
            return false;
        }
        *widget->picker_index += delta;
        while (*widget->picker_index < 0)
        {
            *widget->picker_index += widget->picker_option_count;
        }
        while (*widget->picker_index >= widget->picker_option_count)
        {
            *widget->picker_index -= widget->picker_option_count;
        }
        return true;
    case UI_WIDGET_EDIT_BOX:
    case UI_WIDGET_CONTAINER:
    case UI_WIDGET_BUTTON:
    default:
        return false;
    }
}

static int ui_grid_row_start(int index, int columns)
{
    return (index / columns) * columns;
}

static int ui_grid_row_end(int row_start, int count, int columns)
{
    int row_end = row_start + columns - 1;
    if (row_end >= count)
    {
        row_end = count - 1;
    }

    return row_end;
}

static int ui_grid_last_index_in_column(int count, int columns, int column)
{
    int index = column;
    while (index + columns < count)
    {
        index += columns;
    }

    return index;
}

static int ui_grid_move_left(int index, int count, int columns)
{
    const int row_start = ui_grid_row_start(index, columns);
    const int row_end = ui_grid_row_end(row_start, count, columns);
    return index > row_start ? index - 1 : row_end;
}

static int ui_grid_move_right(int index, int count, int columns)
{
    const int row_start = ui_grid_row_start(index, columns);
    const int row_end = ui_grid_row_end(row_start, count, columns);
    return index < row_end ? index + 1 : row_start;
}

static int ui_grid_move_up(int index, int count, int columns)
{
    if (index - columns >= 0)
    {
        return index - columns;
    }

    return ui_grid_last_index_in_column(count, columns, index % columns);
}

static int ui_grid_move_down(int index, int count, int columns)
{
    if (index + columns < count)
    {
        return index + columns;
    }

    return index % columns;
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
    case UI_ACTION_OPEN_UI_TEST:
        ui_push_screen(ui, UI_SCREEN_TEST, announce);
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
               bool left_pressed, bool right_pressed,
               bool next_container_pressed, bool previous_container_pressed,
               bool activate_pressed, bool back_pressed,
               bool backspace_pressed, const char *text_input,
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

    if (ui_container_is_grid(container))
    {
        int next_index = ui->focused_widget_index;
        const int columns = container->grid_columns;
        const bool grid_navigation_pressed =
            left_pressed || right_pressed || up_pressed || down_pressed;

        if (left_pressed)
        {
            next_index = ui_grid_move_left(next_index, focusable_count, columns);
        }
        else if (right_pressed)
        {
            next_index = ui_grid_move_right(next_index, focusable_count, columns);
        }
        else if (up_pressed)
        {
            next_index = ui_grid_move_up(next_index, focusable_count, columns);
        }
        else if (down_pressed)
        {
            next_index = ui_grid_move_down(next_index, focusable_count, columns);
        }

        if (grid_navigation_pressed)
        {
            ui->focused_widget_index = next_index;
            ui_announce_focus(ui, announce, true);
            return;
        }
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

    const UiWidget *widget = ui_get_focused_widget(ui, screen);

    if (text_input != NULL && text_input[0] != '\0')
    {
        if (ui_edit_box_append_text(widget, text_input))
        {
            ui_announce_widget_value(widget, announce, true);
            return;
        }
    }

    if (backspace_pressed)
    {
        if (ui_edit_box_backspace(widget))
        {
            ui_announce_widget_value(widget, announce, true);
            return;
        }
    }

    if (left_pressed)
    {
        if (ui_adjust_widget_value(widget, -1, false))
        {
            ui_announce_widget_value(widget, announce, true);
            return;
        }
    }

    if (right_pressed)
    {
        if (ui_adjust_widget_value(widget, 1, false))
        {
            ui_announce_widget_value(widget, announce, true);
            return;
        }
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

    if (widget == NULL || !widget->enabled)
    {
        ui_announce_focus(ui, announce, true);
        return;
    }

    if (ui_adjust_widget_value(widget, 0, true))
    {
        ui_announce_widget_value(widget, announce, true);
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
