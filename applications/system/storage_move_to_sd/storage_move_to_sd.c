#include "storage_move_to_sd.h"
#include <core/common_defines.h>
#include <core/log.h>
#include "loader/loader.h"
#include <stdint.h>
#include <toolbox/dir_walk.h>
#include <toolbox/path.h>
#include <furi_hal.h>

#define TAG "MoveToSd"

#define MOVE_SRC STORAGE_INT_PATH_PREFIX
#define MOVE_DST STORAGE_EXT_PATH_PREFIX

static bool storage_move_to_sd_check_entry(const char* name, FileInfo* fileinfo, void* ctx) {
    UNUSED(ctx);
    if(file_info_is_dir(fileinfo)) {
        return true;
    }

    return (name && (*name != '.'));
}

void flipper_migrate_files() {
    if(!furi_hal_is_normal_boot()) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Migrate files, Int -> Ext
    storage_common_remove(storage, INT_PATH(".passport.settings"));
    storage_common_copy(storage, ARCHIVE_FAV_OLD_PATH, ARCHIVE_FAV_PATH);
    storage_common_remove(storage, ARCHIVE_FAV_OLD_PATH);
    storage_common_copy(storage, BT_SETTINGS_OLD_PATH, BT_SETTINGS_PATH);
    storage_common_remove(storage, BT_SETTINGS_OLD_PATH);
    storage_common_copy(storage, DOLPHIN_STATE_OLD_PATH, DOLPHIN_STATE_PATH);
    storage_common_remove(storage, DOLPHIN_STATE_OLD_PATH);
    storage_common_copy(storage, POWER_SETTINGS_OLD_PATH, POWER_SETTINGS_PATH);
    storage_common_remove(storage, POWER_SETTINGS_OLD_PATH);
    storage_common_copy(storage, BT_KEYS_STORAGE_OLD_PATH, BT_KEYS_STORAGE_PATH);
    storage_common_remove(storage, BT_KEYS_STORAGE_OLD_PATH);
    storage_common_copy(storage, NOTIFICATION_SETTINGS_OLD_PATH, NOTIFICATION_SETTINGS_PATH);
    storage_common_remove(storage, NOTIFICATION_SETTINGS_OLD_PATH);
    // Ext -> Int
    storage_common_copy(storage, DESKTOP_SETTINGS_OLD_PATH, DESKTOP_SETTINGS_PATH);
    storage_common_remove(storage, DESKTOP_SETTINGS_OLD_PATH);

    FileInfo file_info;
    if(storage_common_stat(storage, U2F_CNT_OLD_FILE, &file_info) == FSE_OK &&
       file_info.size > 200) { // Is on Int and has content
        storage_common_remove(storage, U2F_CNT_FILE); // Remove outdated on Ext
        storage_common_copy(storage, U2F_CNT_OLD_FILE, U2F_CNT_FILE); // Int -> Ext
    }
    storage_common_copy(storage, U2F_KEY_OLD_FILE, U2F_KEY_FILE); // Ext -> Int

    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "flipper_migrate_files DONE");
}

static void storage_move_to_sd_remove_region() {
    if(!furi_hal_is_normal_boot()) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(storage_common_exists(storage, INT_PATH(".region_data"))) {
        storage_common_remove(storage, INT_PATH(".region_data"));
    }

    furi_record_close(RECORD_STORAGE);
}

bool storage_move_to_sd_perform(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    DirWalk* dir_walk = dir_walk_alloc(storage);
    dir_walk_set_recursive(dir_walk, false);
    dir_walk_set_filter_cb(dir_walk, storage_move_to_sd_check_entry, NULL);

    FuriString *path_src, *path_dst;

    path_dst = furi_string_alloc();
    path_src = furi_string_alloc();

    if(dir_walk_open(dir_walk, STORAGE_INT_PATH_PREFIX)) {
        while(dir_walk_read(dir_walk, path_src, NULL) == DirWalkOK) {
            furi_string_set(path_dst, path_src);
            furi_string_replace_at(
                path_dst, 0, strlen(STORAGE_INT_PATH_PREFIX), STORAGE_EXT_PATH_PREFIX);

            storage_common_merge(
                storage, furi_string_get_cstr(path_src), furi_string_get_cstr(path_dst));
            storage_simply_remove_recursive(storage, furi_string_get_cstr(path_src));
        }
    }

    dir_walk_free(dir_walk);
    furi_string_free(path_dst);
    furi_string_free(path_src);

    furi_record_close(RECORD_STORAGE);

    return false;
}

static bool storage_move_to_sd_check(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    bool should_migrate = false;

    DirWalk* dir_walk = dir_walk_alloc(storage);
    dir_walk_set_recursive(dir_walk, false);
    dir_walk_set_filter_cb(dir_walk, storage_move_to_sd_check_entry, NULL);

    FuriString* name;
    name = furi_string_alloc();

    if(dir_walk_open(dir_walk, STORAGE_INT_PATH_PREFIX)) {
        // if at least 1 entry is present, we should migrate
        should_migrate = (dir_walk_read(dir_walk, name, NULL) == DirWalkOK);
    }

    dir_walk_free(dir_walk);
    furi_string_free(name);

    furi_record_close(RECORD_STORAGE);

    return should_migrate;
}

static bool storage_move_to_sd_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    StorageMoveToSd* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool storage_move_to_sd_back_event_callback(void* context) {
    furi_assert(context);
    StorageMoveToSd* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void storage_move_to_sd_unmount_callback(const void* message, void* context) {
    StorageMoveToSd* app = context;
    furi_assert(app);
    const StorageEvent* storage_event = message;

    if((storage_event->type == StorageEventTypeCardUnmount) ||
       (storage_event->type == StorageEventTypeCardMountError)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MoveToSdCustomEventExit);
    }
}

static StorageMoveToSd* storage_move_to_sd_alloc() {
    StorageMoveToSd* app = malloc(sizeof(StorageMoveToSd));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&storage_move_to_sd_scene_handlers, app);

    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, storage_move_to_sd_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, storage_move_to_sd_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, StorageMoveToSdViewWidget, widget_get_view(app->widget));

    scene_manager_next_scene(app->scene_manager, StorageMoveToSdConfirm);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    app->sub = furi_pubsub_subscribe(
        storage_get_pubsub(storage), storage_move_to_sd_unmount_callback, app);
    furi_record_close(RECORD_STORAGE);

    return app;
}

static void storage_move_to_sd_free(StorageMoveToSd* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    furi_pubsub_unsubscribe(storage_get_pubsub(storage), app->sub);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, StorageMoveToSdViewWidget);
    widget_free(app->widget);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t storage_move_to_sd_app(void* p) {
    UNUSED(p);

    if(storage_move_to_sd_check()) {
        StorageMoveToSd* app = storage_move_to_sd_alloc();
        notification_message(app->notifications, &sequence_display_backlight_on);
        view_dispatcher_run(app->view_dispatcher);
        storage_move_to_sd_free(app);
    } else {
        FURI_LOG_I(TAG, "Nothing to move");
    }

    // Remove unused region file from int memory
    storage_move_to_sd_remove_region();
    // MIGRATE SETTINGS FILES
    flipper_migrate_files();

    return 0;
}

static void storage_move_to_sd_mount_callback(const void* message, void* context) {
    UNUSED(context);

    const StorageEvent* storage_event = message;

    if(storage_event->type == StorageEventTypeCardMount) {
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_start(loader, "StorageMoveToSd", NULL, NULL);
        furi_record_close(RECORD_LOADER);
    }
}

int32_t storage_move_to_sd_start(void* p) {
    UNUSED(p);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    furi_pubsub_subscribe(storage_get_pubsub(storage), storage_move_to_sd_mount_callback, NULL);

    furi_record_close(RECORD_STORAGE);
    return 0;
}
