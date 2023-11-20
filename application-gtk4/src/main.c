#include "application/game_list.h"

#include "vh/db.h"
#include "vh/import.h"
#include "vh/init.h"
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

static GtkWidget*
game_browser_new(struct db_interface* dbi, struct db* db)
{
    GtkWidget* search;
    GtkWidget* games;
    GtkWidget* scroll;
    GtkWidget* vbox;
    GtkWidget* groups;
    GtkWidget* paned;

    search = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");

    games = game_list_new(dbi, db);
    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), games);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), search);
    gtk_box_append(GTK_BOX(vbox), scroll);

    groups = gtk_button_new();

    paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_start_child(GTK_PANED(paned), groups);
    gtk_paned_set_end_child(GTK_PANED(paned), vbox);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_paned_set_position(GTK_PANED(paned), 120);

    return paned;
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
    GtkWidget* replay_browser;
    GtkWidget* paned1;
    GtkWidget* paned2;
    GtkWidget* plugin_view;
    GtkWidget* property_panel;
    struct app_activate_ctx* ctx = user_data;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VODHound");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    plugin_view = plugin_view_new(&ctx->plugins);
    property_panel = property_panel_new(&ctx->plugins);

    paned2 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned2), plugin_view);
    gtk_paned_set_end_child(GTK_PANED(paned2), property_panel);
    gtk_paned_set_resize_start_child(GTK_PANED(paned2), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned2), FALSE);

    paned1 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned1), game_browser_new(ctx->dbi, ctx->db));
    gtk_paned_set_end_child(GTK_PANED(paned1), paned2);
    gtk_paned_set_resize_start_child(GTK_PANED(paned1), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned1), TRUE);
    gtk_paned_set_position(GTK_PANED(paned1), 680);

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
