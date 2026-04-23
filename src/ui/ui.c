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
    return action == UI_ACTION_START_WORLD_SURVIVAL ||
           action == UI_ACTION_START_WORLD_CREATIVE ||
           action == UI_ACTION_SELECT_CREATIVE_TILE ||
           action == UI_ACTION_EXIT;
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

static int *ui_widget_local_int_value(const UiWidget *widget)
{
    if (widget == NULL)
    {
        return NULL;
    }

    if (widget->type == UI_WIDGET_PICKER)
    {
        return widget->picker_index;
    }

    return widget->int_value;
}

static bool ui_widget_get_bool_value(const UiWidget *widget, bool *out)
{
    if (widget == NULL || out == NULL)
    {
        return false;
    }

    switch (widget->value_binding)
    {
    case UI_VALUE_BOOL_POINTER:
        if (widget->toggle_value == NULL)
        {
            return false;
        }
        *out = *widget->toggle_value;
        return true;
    case UI_VALUE_BOOL_SETTING:
        if (settings_get_type(widget->setting_id) != SETTING_TYPE_BOOL)
        {
            return false;
        }
        *out = settings_get_bool(widget->setting_id);
        return true;
    case UI_VALUE_NONE:
    case UI_VALUE_INT_POINTER:
    case UI_VALUE_STRING_BUFFER:
    case UI_VALUE_INT_SETTING:
    case UI_VALUE_STRING_SETTING:
    default:
        return false;
    }
}

static bool ui_widget_set_bool_value(const UiWidget *widget, bool value)
{
    if (widget == NULL)
    {
        return false;
    }

    switch (widget->value_binding)
    {
    case UI_VALUE_BOOL_POINTER:
        if (widget->toggle_value == NULL)
        {
            return false;
        }
        *widget->toggle_value = value;
        return true;
    case UI_VALUE_BOOL_SETTING:
        if (settings_get_type(widget->setting_id) != SETTING_TYPE_BOOL)
        {
            return false;
        }
        settings_set_bool(widget->setting_id, value);
        return true;
    case UI_VALUE_NONE:
    case UI_VALUE_INT_POINTER:
    case UI_VALUE_STRING_BUFFER:
    case UI_VALUE_INT_SETTING:
    case UI_VALUE_STRING_SETTING:
    default:
        return false;
    }
}

static bool ui_widget_get_int_value(const UiWidget *widget, int *out)
{
    if (widget == NULL || out == NULL)
    {
        return false;
    }

    switch (widget->value_binding)
    {
    case UI_VALUE_INT_POINTER:
    {
        int *value = ui_widget_local_int_value(widget);
        if (value == NULL)
        {
            return false;
        }
        *out = *value;
        return true;
    }
    case UI_VALUE_INT_SETTING:
        if (settings_get_type(widget->setting_id) != SETTING_TYPE_INT)
        {
            return false;
        }
        *out = settings_get_int(widget->setting_id);
        return true;
    case UI_VALUE_NONE:
    case UI_VALUE_BOOL_POINTER:
    case UI_VALUE_STRING_BUFFER:
    case UI_VALUE_BOOL_SETTING:
    case UI_VALUE_STRING_SETTING:
    default:
        return false;
    }
}

static bool ui_widget_set_int_value(const UiWidget *widget, int value)
{
    if (widget == NULL)
    {
        return false;
    }

    switch (widget->value_binding)
    {
    case UI_VALUE_INT_POINTER:
    {
        int *target = ui_widget_local_int_value(widget);
        if (target == NULL)
        {
            return false;
        }
        *target = value;
        return true;
    }
    case UI_VALUE_INT_SETTING:
        if (settings_get_type(widget->setting_id) != SETTING_TYPE_INT)
        {
            return false;
        }
        settings_set_int(widget->setting_id, value);
        return true;
    case UI_VALUE_NONE:
    case UI_VALUE_BOOL_POINTER:
    case UI_VALUE_STRING_BUFFER:
    case UI_VALUE_BOOL_SETTING:
    case UI_VALUE_STRING_SETTING:
    default:
        return false;
    }
}

static int ui_widget_min_int_value(const UiWidget *widget)
{
    if (widget != NULL && widget->value_binding == UI_VALUE_INT_SETTING)
    {
        return settings_get_int_min(widget->setting_id);
    }

    return widget != NULL ? widget->min_value : 0;
}

static int ui_widget_max_int_value(const UiWidget *widget)
{
    if (widget != NULL && widget->value_binding == UI_VALUE_INT_SETTING)
    {
        return settings_get_int_max(widget->setting_id);
    }

    return widget != NULL ? widget->max_value : 0;
}

static int ui_widget_int_step(const UiWidget *widget)
{
    if (widget != NULL && widget->value_binding == UI_VALUE_INT_SETTING)
    {
        return settings_get_int_step(widget->setting_id);
    }

    return widget != NULL && widget->step_value > 0 ? widget->step_value : 1;
}

static const char *ui_widget_string_value(const UiWidget *widget)
{
    if (widget == NULL)
    {
        return NULL;
    }

    switch (widget->value_binding)
    {
    case UI_VALUE_STRING_BUFFER:
        return widget->edit_value;
    case UI_VALUE_STRING_SETTING:
        if (settings_get_type(widget->setting_id) != SETTING_TYPE_STRING)
        {
            return NULL;
        }
        return settings_get_string(widget->setting_id);
    case UI_VALUE_NONE:
    case UI_VALUE_BOOL_POINTER:
    case UI_VALUE_INT_POINTER:
    case UI_VALUE_INT_SETTING:
    case UI_VALUE_BOOL_SETTING:
    default:
        return NULL;
    }
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
    {
        bool value = false;
        if (!ui_widget_get_bool_value(widget, &value))
        {
            return false;
        }
        snprintf(out, out_size, "%s", value ? "on" : "off");
        return true;
    }
    case UI_WIDGET_SLIDER:
    {
        int value = 0;
        if (!ui_widget_get_int_value(widget, &value))
        {
            return false;
        }
        snprintf(out, out_size, "%d", value);
        return true;
    }
    case UI_WIDGET_EDIT_BOX:
    {
        const char *value = ui_widget_string_value(widget);
        if (value == NULL || value[0] == '\0')
        {
            snprintf(out, out_size, "empty");
            return true;
        }
        snprintf(out, out_size, "%s", value);
        return true;
    }
    case UI_WIDGET_PICKER:
        if (widget->picker_options == NULL || widget->picker_option_count <= 0)
        {
            return false;
        }
        {
            int value = 0;
            if (!ui_widget_get_int_value(widget, &value))
            {
                return false;
            }
            const int index = ui_clamp_int(value, 0, widget->picker_option_count - 1);
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
        text_input == NULL || text_input[0] == '\0')
    {
        return false;
    }

    if (widget->value_binding == UI_VALUE_STRING_BUFFER)
    {
        if (widget->edit_value == NULL || widget->edit_capacity == 0)
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

    if (widget->value_binding == UI_VALUE_STRING_SETTING &&
        settings_get_type(widget->setting_id) == SETTING_TYPE_STRING)
    {
        char value[256];
        const size_t capacity = settings_get_string_capacity(widget->setting_id);
        const char *current = settings_get_string(widget->setting_id);
        const size_t current_length = strlen(current);

        if (capacity == 0 || current_length >= capacity - 1)
        {
            return false;
        }

        snprintf(value, sizeof(value), "%s", current);
        const size_t value_length = strlen(value);
        if (value_length >= sizeof(value) - 1)
        {
            return false;
        }

        const size_t setting_available = capacity - current_length - 1;
        const size_t buffer_available = sizeof(value) - value_length - 1;
        const size_t available =
            setting_available < buffer_available ? setting_available : buffer_available;
        strncat(value, text_input, available);
        settings_set_string(widget->setting_id, value);
        return true;
    }

    return false;
}

static bool ui_edit_box_backspace(const UiWidget *widget)
{
    if (widget == NULL || widget->type != UI_WIDGET_EDIT_BOX)
    {
        return false;
    }

    if (widget->value_binding == UI_VALUE_STRING_BUFFER)
    {
        if (widget->edit_value == NULL || widget->edit_value[0] == '\0')
        {
            return false;
        }

        ui_delete_last_utf8_char(widget->edit_value);
        return true;
    }

    if (widget->value_binding == UI_VALUE_STRING_SETTING &&
        settings_get_type(widget->setting_id) == SETTING_TYPE_STRING)
    {
        char value[256];
        const char *current = settings_get_string(widget->setting_id);
        if (current[0] == '\0')
        {
            return false;
        }

        snprintf(value, sizeof(value), "%s", current);
        ui_delete_last_utf8_char(value);
        settings_set_string(widget->setting_id, value);
        return true;
    }

    return false;
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
    {
        bool value = false;
        if (!ui_widget_get_bool_value(widget, &value))
        {
            return false;
        }

        if (activate || delta == 0)
        {
            return ui_widget_set_bool_value(widget, !value);
        }
        return ui_widget_set_bool_value(widget, delta > 0);
    }
    case UI_WIDGET_SLIDER:
        if (delta == 0)
        {
            return false;
        }
        {
            int value = 0;
            if (!ui_widget_get_int_value(widget, &value))
            {
                return false;
            }

            const int step = ui_widget_int_step(widget);
            const int next_value = value + (delta * step);
            return ui_widget_set_int_value(
                widget, ui_clamp_int(next_value, ui_widget_min_int_value(widget),
                                     ui_widget_max_int_value(widget)));
        }
    case UI_WIDGET_PICKER:
        if (widget->picker_option_count <= 0 || delta == 0)
        {
            return false;
        }
        {
            int value = 0;
            if (!ui_widget_get_int_value(widget, &value))
            {
                return false;
            }

            value += delta;
            while (value < 0)
            {
                value += widget->picker_option_count;
            }
            while (value >= widget->picker_option_count)
            {
                value -= widget->picker_option_count;
            }
            return ui_widget_set_int_value(widget, value);
        }
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

static void ui_announce_container(const UiState *ui, UiAnnounceFn announce,
                                  bool interrupt)
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

    const UiWidget *container = ui_get_focused_container(ui, screen);
    if (container == NULL || container == screen->root ||
        container->label == NULL || container->label[0] == '\0')
    {
        return;
    }

    char message[96];
    snprintf(message, sizeof(message), "%s.", container->label);
    announce(message, interrupt);
}

static void ui_announce_container_and_focus(const UiState *ui, UiAnnounceFn announce,
                                            bool interrupt)
{
    ui_announce_container(ui, announce, interrupt);
    ui_announce_focus(ui, announce, false);
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
    ui_announce_container_and_focus(ui, announce, false);
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
    case UI_ACTION_NEW_WORLD:
        ui_push_screen(ui, UI_SCREEN_NEW_WORLD, announce);
        return true;
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
    case UI_ACTION_START_WORLD_SURVIVAL:
    case UI_ACTION_START_WORLD_CREATIVE:
    case UI_ACTION_SELECT_CREATIVE_TILE:
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
        ui_announce_container_and_focus(ui, announce, true);
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
        ui_announce_container_and_focus(ui, announce, true);
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

const char *ui_focused_widget_label(const UiState *ui)
{
    if (!ui_is_valid(ui))
    {
        return NULL;
    }

    const UiScreenDefinition *screen = ui_get_screen_definition(ui->screen);
    if (screen == NULL)
    {
        return NULL;
    }

    const UiWidget *widget = ui_get_focused_widget(ui, screen);
    if (widget == NULL)
    {
        return NULL;
    }

    return widget->label;
}
