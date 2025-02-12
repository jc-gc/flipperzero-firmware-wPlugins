#pragma once

#include "helpers/camera_suite_storage.h"
#include "scenes/camera_suite_scene.h"
#include "views/camera_suite_view_guide.h"
#include "views/camera_suite_view_start.h"
#include "views/camera_suite_view_style_1.h"
#include "views/camera_suite_view_style_2.h"
#include <assets_icons.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/button_menu.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <stdlib.h>

#define TAG "Camera Suite"

typedef struct {
    Gui* gui;
    NotificationApp* notification;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    CameraSuiteViewStart* camera_suite_view_start;
    CameraSuiteViewStyle1* camera_suite_view_style_1;
    CameraSuiteViewStyle2* camera_suite_view_style_2;
    CameraSuiteViewGuide* camera_suite_view_guide;
    uint32_t haptic;
    uint32_t speaker;
    uint32_t led;
    ButtonMenu* button_menu;
} CameraSuite;

typedef enum {
    CameraSuiteViewIdStartscreen,
    CameraSuiteViewIdMenu,
    CameraSuiteViewIdScene1,
    CameraSuiteViewIdScene2,
    CameraSuiteViewIdGuide,
    CameraSuiteViewIdSettings,
} CameraSuiteViewId;

typedef enum {
    CameraSuiteHapticOff,
    CameraSuiteHapticOn,
} CameraSuiteHapticState;

typedef enum {
    CameraSuiteSpeakerOff,
    CameraSuiteSpeakerOn,
} CameraSuiteSpeakerState;

typedef enum {
    CameraSuiteLedOff,
    CameraSuiteLedOn,
} CameraSuiteLedState;
