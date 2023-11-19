#include "application/game_list.h"

#include "vh/db.h"
#include "vh/import.h"
#include "vh/init.h"

#include <gtk/gtk.h>

static GtkWidget*
property_panel_new(void)
{
    return gtk_button_new();
}

static GtkWidget*
plugin_view_new(void)
{
    return gtk_button_new();
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
};

static void
activate(GtkApplication* app, gpointer user_data)
{
    GtkWidget* window;
    GtkWidget* replay_browser;
    GtkWidget* paned1;
    GtkWidget* paned2;
    struct app_activate_ctx* ctx = user_data;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VODHound");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    paned2 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned2), plugin_view_new());
    gtk_paned_set_end_child(GTK_PANED(paned2), property_panel_new());
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
    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    dbi->close(db);
    vh_deinit();
    vh_threadlocal_deinit();

    return status;

migrate_db_failed  : dbi->close(db);
open_db_failed     : vh_deinit();
vh_init_failed     : vh_threadlocal_deinit();
vh_init_tl_failed  : return -1;
}
