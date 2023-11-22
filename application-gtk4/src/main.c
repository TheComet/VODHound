#include "application/game_browser.h"

#include "vh/db.h"
#include "vh/fs.h"
#include "vh/import.h"
#include "vh/init.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include <gtk/gtk.h>

struct plugin
{
    struct plugin_lib lib;
    struct plugin_ctx* ctx;
    GtkWidget* ui_center;
    GtkWidget* ui_pane;
};

#if defined(VH_MEM_DEBUGGING)
static void
track_plugin_widget_deallocation(GtkWidget* self, gpointer user_pointer)
{
    mem_track_deallocation(self);
}
#endif

static int
open_plugin(GtkNotebook* center, GtkNotebook* pane, struct vec* plugins, struct db_interface* dbi, struct db* db, struct str_view plugin_name)
{
    int insert_pos;
    struct plugin* plugin = vec_emplace(plugins);

    if (plugin_load(&plugin->lib, plugin_name) != 0)
        goto load_plugin_failed;

    plugin->ctx = plugin->lib.i->create(dbi, db);
    if (plugin->ctx == NULL)
        goto create_context_failed;

    plugin->ui_center = NULL;
    if (plugin->lib.i->ui_center)
    {
        plugin->ui_center = plugin->lib.i->ui_center->create(plugin->ctx);
        if (plugin->ui_center == NULL)
            goto create_ui_center_failed;

#if defined(VH_MEM_DEBUGGING)
        mem_track_allocation(plugin->ui_center);
        g_signal_connect(plugin->ui_center, "destroy", G_CALLBACK(track_plugin_widget_deallocation), NULL);
#endif
    }

    plugin->ui_pane = NULL;
    if (plugin->lib.i->ui_pane)
    {
        plugin->ui_pane = plugin->lib.i->ui_pane->create(plugin->ctx);
        if (plugin->ui_pane == NULL)
            goto create_ui_pane_failed;

#if defined(VH_MEM_DEBUGGING)
        mem_track_allocation(plugin->ui_pane);
        g_signal_connect(plugin->ui_pane, "destroy", G_CALLBACK(track_plugin_widget_deallocation), NULL);
#endif
    }

    if (plugin->ui_center)
    {
        GtkWidget* label = gtk_label_new(plugin->lib.i->info->name);
        gtk_notebook_append_page(center, plugin->ui_center, label);
        /*
        insert_pos = IupGetChildCount(center_view) - 1;
        IupSetAttribute(plugin->ui_center, "TABTITLE", plugin->lib.i->info->name);
        if (IupInsert(center_view, IupGetChild(center_view, insert_pos), plugin->ui_center) == NULL)
            goto add_to_ui_center_failed;
        IupSetInt(center_view, "VALUEPOS", insert_pos);
        IupMap(plugin->ui_center);
        IupRefresh(plugin->ui_center);*/
    }

    if (plugin->ui_pane)
    {
        GtkWidget* label = gtk_label_new(plugin->lib.i->info->name);
        gtk_notebook_append_page(pane, plugin->ui_pane, label);
        /*
        insert_pos = IupGetChildCount(pane_view) - 1;
        IupSetAttribute(plugin->ui_pane, "TABTITLE", plugin->lib.i->info->name);
        if (IupInsert(pane_view, IupGetChild(pane_view, insert_pos), plugin->ui_pane) == NULL)
            goto add_to_ui_pane_failed;
        IupSetInt(pane_view, "VALUEPOS", insert_pos);
        IupMap(plugin->ui_pane);
        IupRefresh(plugin->ui_pane);*/
    }

    return 0;

add_to_ui_pane_failed:
    /*IupDetach(plugin->ui_center);*/
add_to_ui_center_failed:
    if (plugin->ui_pane)
        plugin->lib.i->ui_pane->destroy(plugin->ctx, plugin->ui_pane);
create_ui_pane_failed:
    if (plugin->ui_center)
        plugin->lib.i->ui_center->destroy(plugin->ctx, plugin->ui_center);
create_ui_center_failed:
    plugin->lib.i->destroy(plugin->ctx);
create_context_failed:
    plugin_unload(&plugin->lib);
load_plugin_failed:
    vec_pop(plugins);

    return -1;
}

static void
close_plugin(struct plugin* plugin)
{
    if (plugin->lib.i->video && plugin->lib.i->video->is_open(plugin->ctx))
        plugin->lib.i->video->close(plugin->ctx);
/*
    if (plugin->ui_pane)
        IupDetach(plugin->ui_pane);
    if (plugin->ui_center)
        IupDetach(plugin->ui_center);*/

    if (plugin->ui_pane)
        plugin->lib.i->ui_pane->destroy(plugin->ctx, plugin->ui_pane);
    if (plugin->ui_center)
        plugin->lib.i->ui_center->destroy(plugin->ctx, plugin->ui_center);

    plugin->lib.i->destroy(plugin->ctx);
    plugin_unload(&plugin->lib);
}

struct on_video_path_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct vec* plugins;
    int64_t frame_offset;
    struct str_view file_name;
    struct path file_path;
};

static int
on_video_path(const char* path, void* user)
{
    int combined_success = 0;
    struct on_video_path_ctx* ctx = user;

    if (path_set(&ctx->file_path, cstr_view(path)) < 0)
        return -1;
    if (path_join(&ctx->file_path, ctx->file_name) < 0)
        return -1;
    path_terminate(&ctx->file_path);

    if (!fs_file_exists(ctx->file_path.str.data))
        return 0;

    VEC_FOR_EACH(ctx->plugins, struct plugin, state)
        struct plugin_interface* i = state->lib.i;
        if (i->video == NULL)
            continue;
        combined_success |= i->video->open_file(state->ctx, ctx->file_path.str.data, 1) == 0;
    VEC_END_EACH

    return combined_success;
}

static int
on_game_video(const char* file_name, const char* path_hint, int64_t frame_offset, void* user)
{
    struct on_video_path_ctx* ctx = user;

    /* Try the path hint first */
    ctx->file_name = cstr_view(file_name);
    if (*path_hint)
        switch (on_replay_browser_video_path(path_hint, ctx))
        {
            case 1  : return 1;
            case 0  : break;
            default : return -1;
        }

    /* Will have to search video paths for the video file */
    switch (ctx->dbi->video.get_paths(ctx->db, on_video_path, ctx))
    {
        case 1  :
            path_dirname(&ctx->file_path);
            ctx->dbi->video.set_path_hint(ctx->db, ctx->file_name, path_view(ctx->file_path));
            return 1;
        case 0  : break;
        default : return -1;
    }

    return 0;
}

static int
on_game_selected(int game_id)
{
    struct on_video_path_ctx ctx;
    int game_id;

    if (selected)
    {
        /* Notify plugins of new replay selection */
        VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
            struct plugin_interface* i = state->plugin.i;
            if (i->replays)
                i->replays->select(state->ctx, &game_id, 1);
        VEC_END_EACH

            /* Iterate all videos associated with this game */
            path_init(&ctx.file_path);
        if (ctx.dbi->game.get_videos(ctx.db, game_id, on_replay_browser_game_video, &ctx) <= 0)
        {
            /* If video failed to open, clear */
            VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
                struct plugin_interface* i = state->plugin.i;
            if (i->video == NULL)
                continue;
            if (!i->video->is_open(state->ctx))
                i->video->clear(state->ctx);
            VEC_END_EACH
        }
        path_deinit(&ctx.file_path);
    }
    else
    {
        /* Clear replay selection in plugins */
        VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
            struct plugin_interface* i = state->plugin.i;
        if (i->replays)
            i->replays->clear(state->ctx);
        VEC_END_EACH

            /* Close all open video files */
            VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
            struct plugin_interface* i = state->plugin.i;
        if (i->video == NULL)
            continue;
        if (i->video->is_open(state->ctx))
            i->video->close(state->ctx);
        VEC_END_EACH
    }

    return IUP_DEFAULT;
}

static void
page_removed(GtkNotebook* self, GtkWidget* child, guint page_num, gpointer user_data)
{
    struct vec* plugins = user_data;
    log_dbg("page_removed()\n");
}

static GtkWidget*
property_panel_new(struct vec* plugins)
{
    GtkWidget* notebook = gtk_notebook_new();
    g_signal_connect(notebook, "page-removed", G_CALLBACK(page_removed), plugins);
    return notebook;
}

static GtkWidget*
plugin_view_new(struct vec* plugins)
{
    GtkWidget* notebook = gtk_notebook_new();
    g_signal_connect(notebook, "page-removed", G_CALLBACK(page_removed), plugins);
    return notebook;
}

static gboolean
shortcut_activated(GtkWidget* widget,
    GVariant* unused,
    gpointer   row)
{
    log_dbg("activated shift+r\n");
    return TRUE;
}

static void
setup_global_shortcuts(GtkWidget* window)
{
    GtkEventController* controller;
    GtkShortcutTrigger* trigger;
    GtkShortcutAction* action;
    GtkShortcut* shortcut;
 
    controller = gtk_shortcut_controller_new();
    gtk_shortcut_controller_set_scope(
        GTK_SHORTCUT_CONTROLLER(controller),
        GTK_SHORTCUT_SCOPE_GLOBAL);
    gtk_widget_add_controller(window, controller);

    trigger = gtk_keyval_trigger_new(GDK_KEY_r, GDK_SHIFT_MASK);
    action = gtk_callback_action_new(shortcut_activated, NULL, NULL);
    shortcut = gtk_shortcut_new(trigger, action);
    gtk_shortcut_controller_add_shortcut(
        GTK_SHORTCUT_CONTROLLER(controller),
        shortcut);
}

struct app_activate_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct vec plugins;
};

static void
activate(GtkApplication* app, gpointer user_data)
{
    GtkWidget* window;
    GtkWidget* game_browser;
    GtkWidget* paned1;
    GtkWidget* paned2;
    GtkWidget* plugin_view;
    GtkWidget* property_panel;
    struct app_activate_ctx* ctx = user_data;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VODHound");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
    setup_global_shortcuts(window);

    plugin_view = plugin_view_new(&ctx->plugins);
    property_panel = property_panel_new(&ctx->plugins);
    game_browser = vhapp_game_browser_new();
    vhapp_game_browser_refresh(VHAPP_GAME_BROWSER(game_browser), ctx->dbi, ctx->db);

    paned2 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned2), plugin_view);
    gtk_paned_set_end_child(GTK_PANED(paned2), property_panel);
    gtk_paned_set_resize_start_child(GTK_PANED(paned2), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned2), FALSE);
    //gtk_paned_set_position(GTK_PANED(paned2), 800);

    paned1 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned1), game_browser);
    gtk_paned_set_end_child(GTK_PANED(paned1), paned2);
    gtk_paned_set_resize_start_child(GTK_PANED(paned1), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned1), TRUE);
    gtk_paned_set_position(GTK_PANED(paned1), 600);

    gtk_window_set_child(GTK_WINDOW(window), paned1);
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_widget_set_visible(window, 1);

    open_plugin(GTK_NOTEBOOK(plugin_view), GTK_NOTEBOOK(property_panel),
            &ctx->plugins, ctx->dbi, ctx->db, cstr_view("VOD Review"));
    open_plugin(GTK_NOTEBOOK(plugin_view), GTK_NOTEBOOK(property_panel),
            &ctx->plugins, ctx->dbi, ctx->db, cstr_view("Search"));
}

int main(int argc, char** argv)
{
    GtkApplication* app;
    struct app_activate_ctx ctx;
    int status;

    if (vh_threadlocal_init() != 0)
        goto vh_init_tl_failed;
    if (vh_init() != 0)
        goto vh_init_failed;

    int reinit_db = 0;
    struct db_interface* dbi = db("sqlite3");
    struct db* db = dbi->open("vodhound.db");
    if (db == NULL)
        goto open_db_failed;
    if (dbi->migrate(db, reinit_db) != 0)
        goto migrate_db_failed;

    if (reinit_db)
    {
        import_param_labels_csv(dbi, db, "ParamLabels.csv");
        import_reframed_mapping_info(dbi, db, "migrations/mappingInfo.json");
        import_reframed_all(dbi, db);
        //import_reframed_path(dbi, db, "reframed");
    }

    app = gtk_application_new("ch.thecomet.vodhound", G_APPLICATION_DEFAULT_FLAGS);
    ctx.dbi = dbi;
    ctx.db = db;
    vec_init(&ctx.plugins, sizeof(struct plugin));
    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    VEC_FOR_EACH(&ctx.plugins, struct plugin, plugin)
        close_plugin(plugin);
    VEC_END_EACH
    vec_deinit(&ctx.plugins);

    dbi->close(db);
    vh_deinit();
    vh_threadlocal_deinit();

    return status;

migrate_db_failed  : dbi->close(db);
open_db_failed     : vh_deinit();
vh_init_failed     : vh_threadlocal_deinit();
vh_init_tl_failed  : return -1;
}
