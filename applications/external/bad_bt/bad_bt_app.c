#include "bad_bt_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <lib/flipper_format/flipper_format.h>

#include <bt/bt_service/bt_i.h>
#include <bt/bt_service/bt.h>

static bool bad_bt_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadBtApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bad_bt_app_back_event_callback(void* context) {
    furi_assert(context);
    BadBtApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void bad_bt_app_tick_event_callback(void* context) {
    furi_assert(context);
    BadBtApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void bad_bt_load_settings(BadBtApp* app) {
    furi_string_reset(app->keyboard_layout);
    BadBtConfig* cfg = &app->config;
    strcpy(cfg->bt_name, "");
    memcpy(cfg->bt_mac, BAD_BT_EMPTY_MAC, BAD_BT_MAC_LEN);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_existing(file, BAD_BT_SETTINGS_PATH)) {
        FuriString* tmp_str = furi_string_alloc();
        if(!flipper_format_read_string(file, "Keyboard_Layout", app->keyboard_layout)) {
            furi_string_reset(app->keyboard_layout);
        }
        if(!flipper_format_read_bool(file, "BT_Remember", &(app->bt_remember), 1)) {
            app->bt_remember = false;
        }
        if(flipper_format_read_string(file, "Bt_Name", tmp_str) && !furi_string_empty(tmp_str)) {
            strlcpy(cfg->bt_name, furi_string_get_cstr(tmp_str), BAD_BT_NAME_LEN);
        } else {
            strcpy(cfg->bt_name, "");
        }
        if(!flipper_format_read_hex(file, "Bt_Mac", (uint8_t*)&cfg->bt_mac, BAD_BT_MAC_LEN)) {
            memcpy(cfg->bt_mac, BAD_BT_EMPTY_MAC, BAD_BT_MAC_LEN);
        }
        furi_string_free(tmp_str);
        flipper_format_file_close(file);
    }
    flipper_format_free(file);

    if(!furi_string_empty(app->keyboard_layout)) {
        FileInfo layout_file_info;
        FS_Error file_check_err = storage_common_stat(
            storage, furi_string_get_cstr(app->keyboard_layout), &layout_file_info);
        if(file_check_err != FSE_OK) {
            furi_string_reset(app->keyboard_layout);
            return;
        }
        if(layout_file_info.size != 256) {
            furi_string_reset(app->keyboard_layout);
        }
    }

    furi_record_close(RECORD_STORAGE);
}

static void bad_bt_save_settings(BadBtApp* app) {
    BadBtConfig* cfg = &app->config;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(file, BAD_BT_SETTINGS_PATH)) {
        flipper_format_write_string(file, "Keyboard_Layout", app->keyboard_layout);
        flipper_format_write_bool(file, "BT_Remember", &(app->bt_remember), 1);
        flipper_format_write_string_cstr(file, "Bt_Name", cfg->bt_name);
        flipper_format_write_hex(file, "Bt_Mac", (uint8_t*)&cfg->bt_mac, BAD_BT_MAC_LEN);
        flipper_format_file_close(file);
    }
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

BadBtApp* bad_bt_app_alloc(char* arg) {
    BadBtApp* app = malloc(sizeof(BadBtApp));

    app->bad_bt_script = NULL;

    app->file_path = furi_string_alloc();
    app->keyboard_layout = furi_string_alloc();

    if(arg && strlen(arg)) {
        furi_string_set(app->file_path, arg);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, BAD_BT_APP_BASE_FOLDER);
    storage_simply_mkdir(storage, BAD_BT_APP_BASE_CONFIG_FOLDER);
    furi_record_close(RECORD_STORAGE);

    bad_bt_load_settings(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);

    app->scene_manager = scene_manager_alloc(&bad_bt_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bad_bt_app_tick_event_callback, 250);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bad_bt_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bad_bt_app_back_event_callback);

    Bt* bt = furi_record_open(RECORD_BT);
    app->bt = bt;
    app->bt->suppress_pin_screen = true;
    bad_bt_config_adjust(&app->config);

    // Save prev config
    FuriHalBtProfile kbd = FuriHalBtProfileHidKeyboard;
    app->prev_bt_mode = furi_hal_bt_get_profile_pairing_method(kbd);
    memcpy(app->prev_bt_mac, furi_hal_bt_get_profile_mac_addr(kbd), BAD_BT_MAC_LEN);
    strlcpy(app->prev_bt_name, furi_hal_bt_get_profile_adv_name(kbd), BAD_BT_NAME_LEN);

    // Adjust BT remember MAC to be serial MAC +2
    memcpy(BAD_BT_BOUND_MAC, furi_hal_version_get_ble_mac(), BAD_BT_MAC_LEN);
    BAD_BT_BOUND_MAC[2] += 2;

    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadBtAppViewWidget, widget_get_view(app->widget));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BadBtAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->bad_bt_view = bad_bt_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadBtAppViewWork, bad_bt_get_view(app->bad_bt_view));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadBtAppViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BadBtAppViewByteInput, byte_input_get_view(app->byte_input));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->conn_init_thread =
        furi_thread_alloc_ex("BadBtConnInit", 1024, (FuriThreadCallback)bad_bt_conn_apply, app);
    furi_thread_start(app->conn_init_thread);
    if(!furi_string_empty(app->file_path)) {
        app->bad_bt_script = bad_bt_script_open(app->file_path, app->bt, app);
        bad_bt_script_set_keyboard_layout(app->bad_bt_script, app->keyboard_layout);
        scene_manager_next_scene(app->scene_manager, BadBtSceneWork);
    } else {
        furi_string_set(app->file_path, BAD_BT_APP_BASE_FOLDER);
        scene_manager_next_scene(app->scene_manager, BadBtSceneFileSelect);
    }

    return app;
}

void bad_bt_app_free(BadBtApp* app) {
    furi_assert(app);

    if(app->bad_bt_script) {
        bad_bt_script_close(app->bad_bt_script);
        app->bad_bt_script = NULL;
    }

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BadBtAppViewWork);
    bad_bt_free(app->bad_bt_view);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, BadBtAppViewWidget);
    widget_free(app->widget);

    // Variable item list
    view_dispatcher_remove_view(app->view_dispatcher, BadBtAppViewVarItemList);
    variable_item_list_free(app->var_item_list);

    // Text Input
    view_dispatcher_remove_view(app->view_dispatcher, BadBtAppViewTextInput);
    text_input_free(app->text_input);

    // Byte Input
    view_dispatcher_remove_view(app->view_dispatcher, BadBtAppViewByteInput);
    byte_input_free(app->byte_input);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Restore connection config
    app->bt->suppress_pin_screen = false;
    if(app->conn_init_thread) {
        furi_thread_join(app->conn_init_thread);
        furi_thread_free(app->conn_init_thread);
        app->conn_init_thread = NULL;
    }
    bad_bt_conn_reset(app);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_BT);

    bad_bt_save_settings(app);

    furi_string_free(app->file_path);
    furi_string_free(app->keyboard_layout);

    free(app);
}

int32_t bad_bt_app(char* p) {
    BadBtApp* bad_bt_app = bad_bt_app_alloc(p);

    view_dispatcher_run(bad_bt_app->view_dispatcher);

    bad_bt_app_free(bad_bt_app);
    return 0;
}
