#include "application/game_browser.h"

#include "vh/db.h"
#include "vh/fs.h"
#include "vh/import.h"
#include "vh/init.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include "vh/frame_data.h"

#include <gtk/gtk.h>

#define VHAPP_TYPE_PLUGIN_MODULE (vhapp_plugin_module_get_type())
G_DECLARE_FINAL_TYPE(VhAppPluginModule, vhapp_plugin_module, VHAPP, PLUGIN_MODULE, GTypeModule)

struct _VhAppPluginModule
{
    GTypeModule parent_instance;
};
struct _VhAppPluginModuleClass
{
    GTypeModuleClass parent_class;
};
G_DEFINE_TYPE(VhAppPluginModule, vhapp_plugin_module, G_TYPE_TYPE_MODULE);

static gboolean
vhapp_plugin_module_load(GTypeModule* type_module)
{
    log_dbg("vhapp_plugin_module_load()\n");
    mem_track_allocation(type_module);
    return TRUE;
}

static void
vhapp_plugin_module_unload(GTypeModule* type_module)
{
    log_dbg("vhapp_plugin_module_unload()\n");
    mem_track_deallocation(type_module);
}

static void
vhapp_plugin_module_init(VhAppPluginModule* self) {}

static void
vhapp_plugin_module_class_init(VhAppPluginModuleClass* class)
{
    GTypeModuleClass* module_class = G_TYPE_MODULE_CLASS(class);

    module_class->load = vhapp_plugin_module_load;
    module_class->unload = vhapp_plugin_module_unload;
}

struct plugin
{
    struct plugin_lib lib;
    struct plugin_ctx* ctx;
    GTypeModule* plugin_module;
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

    plugin->plugin_module = g_object_new(VHAPP_TYPE_PLUGIN_MODULE, NULL);
    g_object_ref_sink(plugin->plugin_module);

    plugin->ctx = plugin->lib.i->create(plugin->plugin_module, dbi, db);
    if (plugin->ctx == NULL)
        goto create_context_failed;

    plugin->ui_center = NULL;
    if (plugin->lib.i->ui_center)
    {
        plugin->ui_center = plugin->lib.i->ui_center->create(plugin->ctx);
        if (plugin->ui_center == NULL)
            goto create_ui_center_failed;

        mem_track_allocation(plugin->ui_center);
#if defined(VH_MEM_DEBUGGING)
        g_signal_connect(plugin->ui_center, "destroy", G_CALLBACK(track_plugin_widget_deallocation), NULL);
#endif
    }

    plugin->ui_pane = NULL;
    if (plugin->lib.i->ui_pane)
    {
        plugin->ui_pane = plugin->lib.i->ui_pane->create(plugin->ctx);
        if (plugin->ui_pane == NULL)
            goto create_ui_pane_failed;

        mem_track_allocation(plugin->ui_pane);
#if defined(VH_MEM_DEBUGGING)
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
    plugin->lib.i->destroy(plugin->plugin_module, plugin->ctx);
create_context_failed:
    g_object_unref(plugin->plugin_module);
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
    
    if (plugin->lib.i->replays)
        plugin->lib.i->replays->clear(plugin->ctx);

    if (plugin->ui_pane)
        plugin->lib.i->ui_pane->destroy(plugin->ctx, plugin->ui_pane);
    if (plugin->ui_center)
        plugin->lib.i->ui_center->destroy(plugin->ctx, plugin->ui_center);

    plugin->lib.i->destroy(plugin->plugin_module, plugin->ctx);
    g_object_unref(plugin->plugin_module);
    plugin_unload(&plugin->lib);
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
    gpointer user_data)
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

struct on_video_path_ctx
{
    struct db_interface* dbi;
    struct db* db;
    int64_t frame_offset;
    struct str_view file_name;
    struct path file_path;
};

static int
on_video_path(const char* path, void* user)
{
    struct on_video_path_ctx* ctx = user;

    if (path_set(&ctx->file_path, cstr_view(path)) < 0)
        return -1;
    if (path_join(&ctx->file_path, ctx->file_name) < 0)
        return -1;
    path_terminate(&ctx->file_path);

    /* Stop iterating (return 1) if the file exists */
    return fs_file_exists(ctx->file_path.str.data);
}

static int
on_game_video(int video_id, const char* file_name, const char* path_hint, int64_t frame_offset, void* user)
{
    struct on_video_path_ctx* ctx = user;

    /* Try the path hint first */
    ctx->file_name = cstr_view(file_name);
    ctx->frame_offset = frame_offset;
    if (*path_hint)
        switch (on_video_path(path_hint, ctx))
        {
            case 1  : return 1;
            case 0  : break;
            default : return -1;
        }

    /* Will have to search video paths for the video file */
    switch (ctx->dbi->video.get_paths(ctx->db, on_video_path, ctx))
    {
        case 1  :
            /* Update path hint, since the hint failed but we found a matchinig file */
            ctx->dbi->video.set_path_hint(ctx->db,
                ctx->file_name,
                path_dirname_view(&ctx->file_path));
            return 1;
        case 0  : break;
        default : return -1;
    }

    return 0;
}

struct app_activate_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct path current_video_file;
    struct vec plugins;
};

static void
on_games_selected(VhAppGameBrowser* game_browser, int* game_ids, int count, gpointer user_pointer)
{
    int reuse_video = 0;
    struct app_activate_ctx* ctx = user_pointer;
    struct on_video_path_ctx video_ctx = {
        ctx->dbi,
        ctx->db
    };
    path_init(&video_ctx.file_path);

    /*
     * If this is a single selection, try to find the video associate with this
     * game ID. It's pretty common that multiple games are recorded in a single
     * video, so we try to avoid re-opening the same video if possible, because
     * it takes quite a bit of time.
     */
    if (count == 1)
        if (ctx->dbi->game.get_videos(ctx->db, game_ids[0], on_game_video, &video_ctx) > 0)
            if (str_equal(path_view(ctx->current_video_file), path_view(video_ctx.file_path)))
                reuse_video = 1;

    /* Close all open video files */
    if (!reuse_video)
    {
        VEC_FOR_EACH(&ctx->plugins, struct plugin, plugin)
            struct plugin_interface* i = plugin->lib.i;
            if (i->video == NULL)
                continue;
            if (i->video->is_open(plugin->ctx))
                i->video->close(plugin->ctx);
        VEC_END_EACH

        path_clear(&ctx->current_video_file);
    }

    /* Clear replay selection in plugins */
    VEC_FOR_EACH(&ctx->plugins, struct plugin, plugin)
        struct plugin_interface* i = plugin->lib.i;
        if (i->replays)
            i->replays->clear(plugin->ctx);
    VEC_END_EACH

    /* Notify plugins of new replay selection */
    if (count > 0)
    {
        VEC_FOR_EACH(&ctx->plugins, struct plugin, plugin)
            struct plugin_interface* i = plugin->lib.i;
            if (i->replays)
                i->replays->select(plugin->ctx, game_ids, count);
        VEC_END_EACH
    }

    VEC_FOR_EACH(&ctx->plugins, struct plugin, plugin)
        struct plugin_interface* i = plugin->lib.i;
        if (i->video == NULL)
            continue;

        /* Open video file and update current video path */
        if (!reuse_video && video_ctx.file_path.str.len)
            if (i->video->open_file(plugin->ctx, video_ctx.file_path.str.data) == 0)
                path_set_take(&ctx->current_video_file, &video_ctx.file_path);

        /* Seek to beginning */
        if (i->video->is_open(plugin->ctx))
            i->video->seek(plugin->ctx, 0, 1, 60);
        else
        {
            /* 
             * We want to avoid clearing when switching between videos (causes flicker).
             * Only clear if all else fails.
             */
            i->video->clear(plugin->ctx);
        }
    VEC_END_EACH

    path_deinit(&video_ctx.file_path);
}

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

    game_browser = vhapp_game_browser_new(ctx->dbi, ctx->db);
    g_signal_connect(game_browser, "games-selected", G_CALLBACK(on_games_selected), ctx);

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

    /*open_plugin(GTK_NOTEBOOK(plugin_view), GTK_NOTEBOOK(property_panel),
            &ctx->plugins, ctx->dbi, ctx->db, cstr_view("AI Tool"));*/
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
    if (dbi->migrate_to(db, 1) != 0)
        goto migrate_db_failed;

    if (reinit_db)
    {
        dbi->reinit(db);
        frame_data_delete_all();
        import_param_labels_csv(dbi, db, "ParamLabels.csv");
        import_reframed_mapping_info(dbi, db, "migrations/mappingInfo.json");
        import_reframed_all(dbi, db);
        //import_reframed_path(dbi, db, "reframed");
        //import_reframed_path(dbi, db, "/home/thecomet/videos/ssbu/2023-09-20 - SBZ Bi-Weekly/reframed");
        //import_reframed_path(dbi, db, "/home/thecomet/videos/ssbu/2023-11-11 - Smash Hammered #10/reframed");
    }

    app = gtk_application_new("ch.thecomet.vodhound", G_APPLICATION_DEFAULT_FLAGS);

    ctx.dbi = dbi;
    ctx.db = db;
    path_init(&ctx.current_video_file);
    vec_init(&ctx.plugins, sizeof(struct plugin));

    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    VEC_FOR_EACH(&ctx.plugins, struct plugin, plugin)
        close_plugin(plugin);
    VEC_END_EACH
    vec_deinit(&ctx.plugins);
    path_deinit(&ctx.current_video_file);

    dbi->close(db);
    vh_deinit();
    vh_threadlocal_deinit();

    return status;

migrate_db_failed : dbi->close(db);
open_db_failed    : vh_deinit();
vh_init_failed    : vh_threadlocal_deinit();
vh_init_tl_failed : return -1;
}
