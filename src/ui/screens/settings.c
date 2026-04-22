#include "screen_registry.h"

#include "settings.h"

const UiScreenDefinition *ui_settings_screen(void)
{
    static bool initialized = false;
    static UiWidget volume_controls[4];
    static UiWidget navigation_buttons[1];
    static UiWidget containers[2];
    static UiWidget root;
    static UiScreenDefinition screen;

    if (!initialized)
    {
        volume_controls[0] = (UiWidget)UI_SLIDER(
            "Master volume", settings_get_int_ptr(SETTING_MASTER_VOLUME), 0, 100, 5);
        volume_controls[1] = (UiWidget)UI_SLIDER(
            "Music volume", settings_get_int_ptr(SETTING_MUSIC_VOLUME), 0, 100, 5);
        volume_controls[2] = (UiWidget)UI_SLIDER(
            "Ambience volume", settings_get_int_ptr(SETTING_AMBIENCE_VOLUME), 0, 100, 5);
        volume_controls[3] = (UiWidget)UI_SLIDER(
            "Footsteps volume", settings_get_int_ptr(SETTING_FOOTSTEPS_VOLUME), 0, 100, 5);

        navigation_buttons[0] = (UiWidget)UI_BUTTON("Back", UI_ACTION_BACK);

        containers[0] = (UiWidget)UI_VERTICAL_CONTAINER("Volume controls", volume_controls);
        containers[0].child_count = UI_ARRAY_COUNT(volume_controls);
        containers[1] = (UiWidget)UI_VERTICAL_CONTAINER("Navigation", navigation_buttons);
        containers[1].child_count = UI_ARRAY_COUNT(navigation_buttons);

        root = (UiWidget)UI_VERTICAL_CONTAINER("Settings", containers);
        root.child_count = UI_ARRAY_COUNT(containers);

        screen.id = UI_SCREEN_SETTINGS;
        screen.title = "Settings";
        screen.root = &root;

        initialized = true;
    }

    return &screen;
}
