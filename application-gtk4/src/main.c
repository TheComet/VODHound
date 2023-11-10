#include <gtk/gtk.h>

#include "vh/db_ops.h"
#include "vh/init.h"
#include "vh/log.h"

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

static void
replay_model_populate(GtkTreeStore* model)
{
    GtkTreeIter iter;
    gtk_tree_store_append(model, &iter, NULL);
    gtk_tree_store_set(model, &iter,
        0, "1998-02-15",
        -1);
    {
        GtkTreeIter child;
        gtk_tree_store_append(model, &child, &iter);
        gtk_tree_store_set(model, &child,
            0, "19:45",
            1, "",
            2, "TheComet",
            3, "",
            4, "Stino",
            5, "WR1",
            6, "Bo5",
            7, "0-0",
            8, "1",
            9, "Final Destination",
            -1);
        gtk_tree_store_append(model, &child, &iter);
        gtk_tree_store_set(model, &child,
            0, "19:51",
            1, "",
            2, "TheComet",
            3, "",
            4, "Stino",
            5, "WR1",
            6, "Bo5",
            7, "0-1",
            8, "2",
            9, "Kalos",
            -1);
        gtk_tree_store_append(model, &child, &iter);
        gtk_tree_store_set(model, &child,
            0, "19:54",
            1, "",
            2, "TheComet",
            3, "",
            4, "Stino",
            5, "WR1",
            6, "Bo5",
            7, "0-2",
            8, "3",
            9, "Kalos",
            -1);
    }

    gtk_tree_store_append(model, &iter, NULL);
    gtk_tree_store_set(model, &iter,
        0, "1998-02-17",
        -1);
    {
        GtkTreeIter child;
        gtk_tree_store_append(model, &child, &iter);
        gtk_tree_store_set(model, &child,
            0, "19:45",
            1, "",
            2, "TheComet",
            3, "",
            4, "Bongo",
            5, "WR1",
            6, "Bo3",
            7, "0-0",
            8, "1",
            9, "Final Destination",
            -1);
        gtk_tree_store_append(model, &child, &iter);
        gtk_tree_store_set(model, &child,
            0, "19:51",
            1, "",
            2, "TheComet",
            3, "",
            4, "Stino",
            5, "WR1",
            6, "Bo5",
            7, "0-1",
            8, "2",
            9, "Kalos",
            -1);
    }
}

static GtkTreeModel*
replay_model_create(void)
{
    GtkTreeStore* model;

    model = gtk_tree_store_new(10,
        G_TYPE_STRING,     /* Time or Date + event/tournament */
        G_TYPE_STRING,     /* Player 1 character */
        G_TYPE_STRING,     /* Player 1 name */
        G_TYPE_STRING,     /* Player 2 character */
        G_TYPE_STRING,     /* Player 2 name */
        G_TYPE_STRING,     /* Round */
        G_TYPE_STRING,     /* Format */
        G_TYPE_STRING,     /* Score */
        G_TYPE_STRING,     /* Game */
        G_TYPE_STRING);    /* Stage */

    return GTK_TREE_MODEL(model);
}

static void
replay_columns_add(GtkTreeView* replays)
{
    GtkCellRenderer* renderer;
    GtkTreeViewColumn* column;
    GtkTreeModel* model;
    int offset;

    model = gtk_tree_view_get_model(replays);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Time", renderer, "text", 0, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "", renderer, "icon-name", 1, NULL);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Player 1", renderer, "text", 2, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "", renderer, "icon-name", 3, NULL);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Player 2", renderer, "text", 4, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Round", renderer, "text", 5, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Format", renderer, "text", 6, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Score", renderer, "text", 7, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Game", renderer, "text", 8, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    offset = gtk_tree_view_insert_column_with_attributes(replays,
        -1, "Stage", renderer, "text", 9, NULL);
    column = gtk_tree_view_get_column(replays, offset - 1);
    gtk_tree_view_column_set_clickable(GTK_TREE_VIEW_COLUMN(column), TRUE);
}

static GtkWidget*
replay_browser_new(void)
{
    GtkWidget* search;
    GtkWidget* replays;
    GtkWidget* scroll;
    GtkWidget* vbox;
    GtkWidget* groups;
    GtkWidget* paned;
    GtkTreeModel* replay_model;

    search = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");

    replay_model = replay_model_create();
    replays = gtk_tree_view_new_with_model(replay_model);
    gtk_widget_set_vexpand(replays, TRUE);
    g_object_unref(replay_model);
    gtk_tree_selection_set_mode(
        gtk_tree_view_get_selection(GTK_TREE_VIEW(replays)),
        GTK_SELECTION_MULTIPLE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(replays));
    replay_columns_add(GTK_TREE_VIEW(replays));

    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), replays);

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

static void
activate(GtkApplication* app, gpointer user_data)
{
    GtkWidget* window;
    GtkWidget* replay_browser;
    GtkWidget* paned1;
    GtkWidget* paned2;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "VODHound");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    paned2 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned2), plugin_view_new());
    gtk_paned_set_end_child(GTK_PANED(paned2), property_panel_new());
    gtk_paned_set_resize_start_child(GTK_PANED(paned2), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned2), FALSE);

    paned1 = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned1), replay_browser_new());
    gtk_paned_set_end_child(GTK_PANED(paned1), paned2);
    gtk_paned_set_resize_start_child(GTK_PANED(paned1), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned1), TRUE);
    gtk_paned_set_position(GTK_PANED(paned1), 500);

    gtk_window_set_child(GTK_WINDOW(window), paned1);
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_widget_set_visible(window, 1);
}

int main(int argc, char** argv)
{
    GtkApplication* app;
    int status;

    if (vh_threadlocal_init() != 0)
        goto vh_init_tl_failed;
    if (vh_init() != 0)
        goto vh_init_failed;

    int reinit_db = 0;
    struct db_interface* dbi = db("sqlite");
    struct db* db = dbi->open_and_prepare("vodhound.db", reinit_db);
    if (db == NULL)
        goto open_db_failed;

    app = gtk_application_new("ch.thecomet.vodhound", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    dbi->close(db);
    vh_deinit();
    vh_threadlocal_deinit();

    return status;

open_db_failed     : vh_deinit();
vh_init_failed     : vh_threadlocal_deinit();
vh_init_tl_failed  : return -1;
}
