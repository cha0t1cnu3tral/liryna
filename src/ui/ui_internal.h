#ifndef LIRYNA_UI_INTERNAL_H
#define LIRYNA_UI_INTERNAL_H

#include <stddef.h>

#include "ui.h"

typedef enum UiWidgetType
{
    UI_WIDGET_CONTAINER = 0,
    UI_WIDGET_BUTTON,
} UiWidgetType;

typedef enum UiContainerDirection
{
    UI_CONTAINER_VERTICAL = 0,
    UI_CONTAINER_HORIZONTAL,
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
};

typedef struct UiScreenDefinition
{
    UiScreen id;
    const char *title;
    const UiWidget *root;
} UiScreenDefinition;

#define UI_ARRAY_COUNT(items) ((int)(sizeof(items) / sizeof((items)[0])))

#define UI_BUTTON(label_, action_) \
    {UI_WIDGET_BUTTON, label_, NULL, true, true, action_, UI_CONTAINER_VERTICAL, NULL, 0}

#define UI_VERTICAL_CONTAINER(label_, children_) \
    {UI_WIDGET_CONTAINER, label_, NULL, true, false, UI_ACTION_NONE, \
     UI_CONTAINER_VERTICAL, children_, UI_ARRAY_COUNT(children_)}

#endif
