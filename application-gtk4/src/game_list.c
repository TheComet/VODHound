#include "application/game_list.h"
#include "application/fighter_icons.h"

#include "vh/db.h"
#include "vh/log.h"
#include "vh/str.h"
#include "vh/vec.h"

#include <gtk/gtk.h>

#define GAME_LIST_COLUMNS_LIST                       \
    X(TIME,   column1,       column_1,    "Time")    \
    X(TEAM1,  icon_label,    icon_column, "Team 1")  \
    X(TEAM2,  icon_label,    icon_column, "Team 2")  \
    X(ROUND,  center_label,  column_n,    "Round")   \
    X(FORMAT, center_label,  column_n,    "Format")  \
    X(SCORE,  center_label,  column_n,    "Score")   \
    X(GAME,   center_label,  column_n,    "Game")    \
    X(STAGE,  left_label,    column_n,    "Stage")

enum game_list_column
{
#define X(name, setup, bind, str) COL_##name,
    GAME_LIST_COLUMNS_LIST
#undef X
};

struct _VhAppGameList;
struct _VhAppGameList* vhapp_game_list_new(void);

struct _VhAppGameListObject
{
    GObject parent_instance;
    struct _VhAppGameList* event_game_list;
    struct strlist columns;
    int fighter_ids[2][8];  /* [team][player slot] */
    int costumes[2][8];
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
vhapp_game_list_object_new_event(
    struct str_view date,
    struct str_view event_name)
{
    VhAppGameListObject* obj = g_object_new(VHAPP_TYPE_GAME_LIST_OBJECT, NULL);

    strlist_init(&obj->columns);
    strlist_add_terminated(&obj->columns, date);
    strlist_add_terminated(&obj->columns, event_name);

    obj->event_game_list = vhapp_game_list_new();

    obj->fighter_ids[0][0] = -1;
    obj->costumes[0][0] = -1;
    obj->fighter_ids[1][0] = -1;
    obj->costumes[1][0] = -1;

    return obj;
}

VhAppGameListObject*
vhapp_game_list_object_new_game(
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

    obj->event_game_list = NULL;

    obj->fighter_ids[0][0] = -1;
    obj->costumes[0][0] = -1;
    obj->fighter_ids[1][0] = -1;
    obj->costumes[1][0] = -1;

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
        VhAppGameListObject* obj = VHAPP_GAME_LIST_OBJECT(*pobj);
        if (obj->event_game_list)
            g_object_unref(obj->event_game_list);
        g_object_unref(obj);
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

static GListModel*
expand_node_cb(gpointer item, gpointer user_data)
{
    VhAppGameListObject* obj = VHAPP_GAME_LIST_OBJECT(item);

    /* Can't expand nodes that aren't events */
    if (obj->event_game_list == NULL)
        return NULL;

    return G_LIST_MODEL(g_object_ref(obj->event_game_list));
}

static void
setup_column1_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    GtkWidget* expander = gtk_tree_expander_new();
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), label);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_list_item_set_child(item, expander);
}

static void
bind_column_1_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* expander = gtk_list_item_get_child(item);
    GtkTreeListRow* row = gtk_list_item_get_item(item);
    GtkWidget* label = gtk_tree_expander_get_child(GTK_TREE_EXPANDER(expander));
    VhAppGameListObject* game_object = gtk_tree_list_row_get_item(row);
    enum game_list_column column = (enum game_list_column)(intptr_t)user_data;
    struct str_view str = strlist_view(&game_object->columns, column);

    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);

    gtk_label_set_text(GTK_LABEL(label), str.data);
    g_object_unref(game_object);
}

static void
setup_icon_label_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* image = gtk_image_new();
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_box_append(GTK_BOX(box), image);
    gtk_box_append(GTK_BOX(box), label);
    gtk_list_item_set_child(item, box);
}

static void
bind_icon_column_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    enum game_list_column column = (enum game_list_column)(intptr_t)user_data;
    GtkWidget* box = gtk_list_item_get_child(item);
    GtkTreeListRow* row = gtk_list_item_get_item(item);
    VhAppGameListObject* game_obj = gtk_tree_list_row_get_item(row);

    GtkWidget* image = gtk_widget_get_first_child(box);
    GtkWidget* label = gtk_widget_get_next_sibling(image);

    /*
     * The event list object only has strings for the first two columns
     * (date and name of the event), whereas game list objects have strings
     * for all columns. Because the label widgets get re-used in the UI, we
     * need to make sure to clear the text on the remaining columns if this is
     * an event list object.
     */
    if (game_obj->event_game_list)
        gtk_image_clear(GTK_IMAGE(image));
    else
    {
        int team = column == COL_TEAM1 ? 0 : 1;
        char* resource_path = fighter_icon_get_resource_path_from_id(
            game_obj->fighter_ids[team][0],
            game_obj->costumes[team][0]);
        gtk_image_set_from_resource(GTK_IMAGE(image), resource_path);
        fighter_icon_free_str(resource_path);
    }

    if (game_obj->event_game_list && column >= 2)
        gtk_label_set_text(GTK_LABEL(label), "");
    else
    {
        struct str_view str = strlist_view(&game_obj->columns, column);
        gtk_label_set_text(GTK_LABEL(label), str.data);
    }

    g_object_unref(game_obj);
}

static void
setup_left_label_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_list_item_set_child(item, label);
}

static void
setup_center_label_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_list_item_set_child(item, label);
}

static void
bind_column_n_cb(GtkSignalListItemFactory* self, GtkListItem* item, gpointer user_data)
{
    enum game_list_column column = (enum game_list_column)(intptr_t)user_data;
    GtkWidget* label = gtk_list_item_get_child(item);
    GtkTreeListRow* row = gtk_list_item_get_item(item);
    VhAppGameListObject* game_list_obj = gtk_tree_list_row_get_item(row);

    /*
     * The event list object only has strings for the first two columns
     * (date and name of the event), whereas game list objects have strings
     * for all columns. Because the label widgets get re-used in the UI, we
     * need to make sure to clear the text on the remaining columns if this is
     * an event list object.
     */
    if (game_list_obj->event_game_list && column >= 2)
        gtk_label_set_text(GTK_LABEL(label), "");
    else
    {
        /* NOTE: We made sure to null-terminate these strings in the strlist */
        struct str_view str = strlist_view(&game_list_obj->columns, column);
        gtk_label_set_text(GTK_LABEL(label), str.data);
    }

    g_object_unref(game_list_obj);
}

struct on_game_ctx
{
    VhAppGameList* event_list;
    VhAppGameList* game_list;

    /* Last date + event returned from the db.
     * We use this to determine when to start a new root node */
    int year, month, mday;
    int event_id;
};

static int on_game(
    int game_id,
    int event_id,
    uint64_t time_started,
    int duration,
    const char* tournament,
    const char* event,
    const char* stage,
    const char* round,
    const char* format,
    const char* scores,
    const char* slots,
    const char* teams,
    const char* players,
    const char* fighter_ids,
    const char* costumes,
    void* user)
{
    int i;
    int s1, s2, game_number;
    struct tm* tm;
    struct on_game_ctx* ctx = user;
    struct str_view left = {0}, right = {0};
    struct str_view team1 = {0}, team2 = {0};
    struct str_view fighters1 = {0}, fighters2 = {0};
    struct str_view costumes1 = {0}, costumes2 = {0};
    struct str_view score1 = {0}, score2 = {0};
    VhAppGameListObject* game_obj;

    char time_str[6];    /* HH:MM */
    char scores_str[36];  /* -2147483648 - -2147483648 */
    char game_str[16];    /* -2147483648 */

    time_started = time_started / 1000;
    tm = localtime((time_t*)&time_started);
    if (tm->tm_year > 9999)
        tm->tm_year = 9999;
    strftime(time_str, sizeof(time_str), "%H:%M", tm);

    str_split2(cstr_view(teams), ',', &team1, &team2);
    str_split2(cstr_view(fighter_ids), ',', &fighters1, &fighters2);
    str_split2(cstr_view(costumes), ',', &costumes1, &costumes2);
    str_split2(cstr_view(scores), ',', &score1, &score2);
    sprintf(scores_str, "%.*s-%.*s", score1.len, score1.data, score2.len, score2.data);

    str_dec_to_int(score1, &s1);
    str_dec_to_int(score2, &s2);
    game_number = s1 + s2 + 1;
    sprintf(game_str, "%d", game_number);

    /* If the event changes, or if the date changes, start a new root node */
    if (ctx->mday != tm->tm_mday ||
         ctx->month != tm->tm_mon ||
         ctx->year != tm->tm_year ||
         ctx->event_id != event_id)
    {
        VhAppGameListObject* event_obj;
        char date_str[11];  /* YYYY-MM-DD */
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm);
        event_obj = vhapp_game_list_object_new_event(
                cstr_view(date_str),
                cstr_view(*event ? event : "Other"));
        ctx->game_list = event_obj->event_game_list;
        vhapp_game_list_append(ctx->event_list, event_obj);

        ctx->mday = tm->tm_mday;
        ctx->month = tm->tm_mon;
        ctx->year = tm->tm_year;
        ctx->event_id = event_id;
    }

    game_obj = vhapp_game_list_object_new_game(
        cstr_view(time_str),
        team1, team2,
        cstr_view(round),
        cstr_view(format),
        cstr_view(scores_str),
        cstr_view(game_str),
        cstr_view(stage));

    str_split2(fighters1, '+', &left, &right);
    for (i = 0; left.len; str_split2(right, '+', &left, &right), i++)
        str_dec_to_int(left, &game_obj->fighter_ids[0][i]);

    str_split2(fighters2, '+', &left, &right);
    for (i = 0; left.len; str_split2(right, '+', &left, &right), i++)
        str_dec_to_int(left, &game_obj->fighter_ids[1][i]);

    str_split2(costumes1, '+', &left, &right);
    for (i = 0; left.len; str_split2(right, '+', &left, &right), i++)
        str_dec_to_int(left, &game_obj->costumes[0][i]);

    str_split2(costumes2, '+', &left, &right);
    for (i = 0; left.len; str_split2(right, '+', &left, &right), i++)
        str_dec_to_int(left, &game_obj->costumes[1][i]);

    vhapp_game_list_append(ctx->game_list, game_obj);

    return 0;
}

GtkWidget*
game_list_new(struct db_interface* dbi, struct db* db)
{
    GtkTreeListModel* model;
    GtkMultiSelection* selection_model;
    GtkWidget* column_view;
    GtkListItemFactory* item_factory;
    GtkColumnViewColumn* column;
    VhAppGameList* event_list;
    struct on_game_ctx on_game_ctx = {0};

    event_list = vhapp_game_list_new();
    model = gtk_tree_list_model_new(G_LIST_MODEL(event_list), FALSE, FALSE, expand_node_cb, NULL, NULL);
    gtk_tree_list_model_set_autoexpand(model, TRUE);

    log_dbg("Querying games...\n");
    on_game_ctx.event_list = event_list;
    dbi->game.get_all(db, on_game, &on_game_ctx);
    log_dbg("Loaded %d games\n", dbi->game.count(db));

    selection_model = gtk_multi_selection_new(G_LIST_MODEL(model));
    column_view = gtk_column_view_new(GTK_SELECTION_MODEL(selection_model));
    /*gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(column_view), TRUE);*/
    gtk_widget_set_vexpand(column_view, TRUE);

#define X(name, setup, bind, str)                                           \
        item_factory = gtk_signal_list_item_factory_new();                  \
        g_signal_connect(item_factory, "setup", G_CALLBACK(setup_##setup##_cb), NULL); \
        g_signal_connect(item_factory, "bind", G_CALLBACK(bind_##bind##_cb), (void*)(intptr_t)COL_##name); \
        column = gtk_column_view_column_new(str, item_factory);             \
        gtk_column_view_append_column(GTK_COLUMN_VIEW(column_view), column);\
        g_object_unref(column);
    GAME_LIST_COLUMNS_LIST
#undef X

    return column_view;
}
