#ifndef LIRYNA_UI_INTERNAL_H
#define LIRYNA_UI_INTERNAL_H

#include <stddef.h>

#include "ui.h"

typedef enum UiWidgetType
{
    UI_WIDGET_CONTAINER = 0,
    UI_WIDGET_BUTTON,
    UI_WIDGET_TOGGLE,
    UI_WIDGET_SLIDER,
    UI_WIDGET_EDIT_BOX,
    UI_WIDGET_PICKER,
} UiWidgetType;

typedef enum UiContainerDirection
{
    UI_CONTAINER_VERTICAL = 0,
    UI_CONTAINER_HORIZONTAL,
    UI_CONTAINER_GRID,
} UiContainerDirection;

typedef struct UiWidget UiWidget;

struct UiWidget
{
    UiWidgetType type;
    const char *label;
    const char *value;
    bool enabled;
    bool focusable;
    UiAction action;
    UiContainerDirection direction;
    const UiWidget *children;
    int child_count;
    int grid_columns;
    bool *toggle_value;
    int *int_value;
    int min_value;
    int max_value;
    int step_value;
    char *edit_value;
    size_t edit_capacity;
    const char *const *picker_options;
    int picker_option_count;
    int *picker_index;
};

typedef struct UiScreenDefinition
{
    UiScreen id;
    const char *title;
    const UiWidget *root;
} UiScreenDefinition;

#define UI_ARRAY_COUNT(items) ((int)(sizeof(items) / sizeof((items)[0])))

#define UI_BUTTON(label_, action_) \
    {.type = UI_WIDGET_BUTTON, .label = label_, .enabled = true, .focusable = true, \
     .action = action_, .direction = UI_CONTAINER_VERTICAL}

#define UI_TOGGLE(label_, value_) \
    {.type = UI_WIDGET_TOGGLE, .label = label_, .enabled = true, .focusable = true, \
     .direction = UI_CONTAINER_VERTICAL, .toggle_value = value_}

#define UI_SLIDER(label_, value_, min_, max_, step_) \
    {.type = UI_WIDGET_SLIDER, .label = label_, .enabled = true, .focusable = true, \
     .direction = UI_CONTAINER_VERTICAL, .int_value = value_, .min_value = min_, \
     .max_value = max_, .step_value = step_}

#define UI_EDIT_BOX(label_, value_, capacity_) \
    {.type = UI_WIDGET_EDIT_BOX, .label = label_, .enabled = true, .focusable = true, \
     .direction = UI_CONTAINER_VERTICAL, .edit_value = value_, .edit_capacity = capacity_}

#define UI_PICKER(label_, options_, index_) \
    {.type = UI_WIDGET_PICKER, .label = label_, .enabled = true, .focusable = true, \
     .direction = UI_CONTAINER_VERTICAL, .picker_options = options_, \
     .picker_option_count = UI_ARRAY_COUNT(options_), .picker_index = index_}

#define UI_VERTICAL_CONTAINER(label_, children_) \
    {.type = UI_WIDGET_CONTAINER, .label = label_, .enabled = true, .focusable = false, \
     .action = UI_ACTION_NONE, .direction = UI_CONTAINER_VERTICAL, \
     .children = children_, .child_count = UI_ARRAY_COUNT(children_)}

#define UI_GRID_CONTAINER(label_, children_, columns_) \
    {.type = UI_WIDGET_CONTAINER, .label = label_, .enabled = true, .focusable = false, \
     .action = UI_ACTION_NONE, .direction = UI_CONTAINER_GRID, \
     .children = children_, .child_count = UI_ARRAY_COUNT(children_), \
     .grid_columns = columns_}

#endif
