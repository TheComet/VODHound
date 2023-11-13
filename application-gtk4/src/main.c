#include <gtk/gtk.h>

#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/init.h"
#include "vh/vec.h"

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

#define GAME_LIST_COLUMNS_LIST \
    X(TIME, "Time")            \
    X(TEAM1, "Team 1")         \
    X(TEAM2, "Team 2")         \
    X(ROUND, "Round")          \
    X(FORMAT, "Format")        \
    X(SCORE, "Score")          \
    X(GAME, "Game")            \
    X(STAGE, "Stage")

enum game_list_column
{
#define X(name, str) name,
    GAME_LIST_COLUMNS_LIST
#undef X
};

struct _VhAppGameListObject
{
    GObject parent_instance;
    struct strlist columns;
};

#define VHAPP_TYPE_GAME_LIST_OBJECT (vhapp_game_list_object_get_type())
G_DECLARE_FINAL_TYPE(VhAppGameListObject, vhapp_game_list_object, VHAPP, GAME_LIST_OBJECT, GObject);
G_DEFINE_TYPE(VhAppGameListObject, vhapp_game_list_object, G_TYPE_OBJECT);

static void
vhapp_game_list_object_finalize(GObject* object)
{
    VhAppGameListObject* self = VHAPP_GAME_LIST_OBJECT(object);
    strlist_deinit(&self->columns);
    G_OBJECT_CLASS(vhapp_game_list_object_parent_class)->finalize(object);
}

VhAppGameListObject*
vhapp_game_list_object_new(
    struct str_view time,
    struct str_view team1,
    struct str_view team2,
    struct str_view round,
    struct str_view format,
    struct str_view score,
    struct str_view game,
    struct str_view stage)
{
    VhAppGameListObject* obj = g_object_new(VHAPP_TYPE_GAME_LIST_OBJECT, NULL);
    strlist_init(&obj->columns);
    strlist_add_terminated(&obj->columns, time);
    strlist_add_terminated(&obj->columns, team1);
    strlist_add_terminated(&obj->columns, team2);
    strlist_add_terminated(&obj->columns, round);
    strlist_add_terminated(&obj->columns, format);
    strlist_add_terminated(&obj->columns, score);
    strlist_add_terminated(&obj->columns, game);
    strlist_add_terminated(&obj->columns, stage);
    return obj;
}

static void
vhapp_game_list_object_class_init(VhAppGameListObjectClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    object_class->finalize = vhapp_game_list_object_finalize;
}

static void
vhapp_game_list_object_init(VhAppGameListObject* class)
{
}

struct _VhAppGameList
{
    GObject parent_instance;
    struct vec items;
};
struct _VhAppGameListClass
{
    GObject parent_class;
};

#define VHAPP_TYPE_GAME_LIST (vhapp_game_list_get_type())
G_DECLARE_FINAL_TYPE(VhAppGameList, vhapp_game_list, VHAPP, GAME_LIST, GObject);

static GType
vhapp_game_list_get_item_type(GListModel *list)
{
    return G_TYPE_OBJECT;
}

static guint
vhapp_game_list_get_n_items(GListModel *list)
{
    VhAppGameList* self = VHAPP_GAME_LIST(list);
    return vec_count(&self->items);
}

static gpointer
vhapp_game_list_get_item(GListModel* list, guint position)
{
    VhAppGameList* self = VHAPP_GAME_LIST(list);
    if (position >= vec_count(&self->items))
        return NULL;
    return g_object_ref(*(GObject**)vec_get(&self->items, position));
}

static void
vhapp_game_list_model_init(GListModelInterface* iface)
{
    iface->get_item_type = vhapp_game_list_get_item_type;
    iface->get_n_items = vhapp_game_list_get_n_items;
    iface->get_item = vhapp_game_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE(VhAppGameList, vhapp_game_list, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, vhapp_game_list_model_init))

static void
vhapp_game_list_dispose(GObject* object)
{
    VhAppGameList* self = VHAPP_GAME_LIST(object);
    VEC_FOR_EACH(&self->items, GObject*, pobj)
        g_object_unref(*pobj);
    VEC_END_EACH
    vec_deinit(&self->items);
    G_OBJECT_CLASS(vhapp_game_list_parent_class)->dispose(object);
}

static void
vhapp_game_list_class_init(VhAppGameListClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    object_class->dispose = vhapp_game_list_dispose;
}

static void
vhapp_game_list_init(VhAppGameList* self)
{
    vec_init(&self->items, sizeof(GObject*));
}

VhAppGameList*
vhapp_game_list_new(void)
{
    return g_object_new(VHAPP_TYPE_GAME_LIST, NULL);
}

void
vhapp_game_list_append(VhAppGameList* self, VhAppGameListObject* item)
{
    vec_push(&self->items, &item);
    g_list_model_items_changed(G_LIST_MODEL(self), vec_count(&self->items) - 1, 0, 1);
}

static void
setup_label_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_list_item_set_child(item, label);
}

static void
bind_column(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* label = gtk_list_item_get_child(item);
    VhAppGameListObject* game_object = gtk_list_item_get_item(item);
    enum game_list_column column = (enum game_list_column)(intptr_t)user_data;
    struct str_view str = strlist_view(&game_object->columns, column);
    gtk_label_set_text(GTK_LABEL(label), str.data);
}

struct game_list_query_ctx
{
    VhAppGameList* game_list;
};

static int on_game_list_query(
    int game_id,
    uint64_t time_started,
    int duration,
    const char* tournament,
    const char* event,
    const char* stage,
    const char* round,
    const char* format,
    const char* teams,
    const char* scores,
    const char* slots,
    const char* sponsors,
    const char* players,
    const char* fighters,
    const char* costumes,
    void* user)
{
    struct game_list_query_ctx* ctx = user;
    struct str_view team1, team2, fighter1, fighter2, score1, score2;
    int s1, s2, game_number;

    char datetime[17];
    char scores_str[36];  /* -2147483648 - -2147483648 */
    char game_str[12];    /* -2147483648 */

    time_started = time_started / 1000;
    strftime(datetime, sizeof(datetime), "%y-%m-%d %H:%M", localtime((time_t*)&time_started));

    str_split2(cstr_view(teams), ',', &team1, &team2);
    str_split2(cstr_view(fighters), ',', &fighter1, &fighter2);
    str_split2(cstr_view(scores), ',', &score1, &score2);
    sprintf(scores_str, "%.*s - %.*s", score1.len, score1.data, score2.len, score2.data);

    str_dec_to_int(score1, &s1);
    str_dec_to_int(score2, &s2);
    game_number = s1 + s2 + 1;
    sprintf(game_str, "%d", game_number);

    vhapp_game_list_append(ctx->game_list,
        vhapp_game_list_object_new(
            cstr_view(datetime),
            team1, team2,
            cstr_view(round),
            cstr_view(format),
            cstr_view(scores_str),
            cstr_view(game_str),
            cstr_view(stage)));

    return 0;
}

static GtkWidget*
game_list_new(struct db_interface* dbi, struct db* db)
{
    VhAppGameList* game_list;
    VhAppGameListObject* item;
    GtkMultiSelection* selection_model;
    GtkListItemFactory* item_factory;
    GtkWidget* column_view;
    GtkColumnViewColumn* column;
    struct game_list_query_ctx query_ctx;

    game_list = vhapp_game_list_new();
    query_ctx.game_list = game_list;
    log_dbg("Querying games...\n");
    dbi->game.query(db, on_game_list_query, &query_ctx);
    log_dbg("Loaded %d games\n", vhapp_game_list_get_n_items(G_LIST_MODEL(game_list)));

    selection_model = gtk_multi_selection_new(G_LIST_MODEL(game_list));
    column_view = gtk_column_view_new(GTK_SELECTION_MODEL(selection_model));
    //gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(column_view), TRUE);
    gtk_widget_set_vexpand(column_view, TRUE);

#define X(name, str)                                                        \
        item_factory = gtk_signal_list_item_factory_new();                  \
        g_signal_connect(item_factory, "setup", G_CALLBACK(setup_label_cb), NULL); \
        g_signal_connect(item_factory, "bind", G_CALLBACK(bind_column), (void*)(intptr_t)name); \
        column = gtk_column_view_column_new(str, item_factory);             \
        gtk_column_view_append_column(GTK_COLUMN_VIEW(column_view), column);\
        g_object_unref(column);
    GAME_LIST_COLUMNS_LIST
#undef X

    return column_view;
}

static GtkWidget*
replay_browser_new(struct db_interface* dbi, struct db* db)
{
    GtkWidget* search;
    GtkWidget* replays;
    GtkWidget* scroll;
    GtkWidget* vbox;
    GtkWidget* groups;
    GtkWidget* paned;

    search = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");

    replays = game_list_new(dbi, db);
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
    gtk_paned_set_start_child(GTK_PANED(paned1), replay_browser_new(ctx->dbi, ctx->db));
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
    struct app_activate_ctx ctx;
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
    ctx.dbi = dbi;
    ctx.db = db;
    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);
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
