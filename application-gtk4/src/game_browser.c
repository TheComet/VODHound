#include "application/game_browser.h"
#include "application/fighter_icons.h"

#include "vh/db.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/str.h"
#include "vh/vec.h"

#include <gtk/gtk.h>

#define COLUMNS_LIST                                 \
    X(TIME,   column1,       column_1,    "Time")    \
    X(TEAM1,  icon_label,    icon_column, "Team 1")  \
    X(TEAM2,  icon_label,    icon_column, "Team 2")  \
    X(ROUND,  center_label,  column_n,    "Round")   \
    X(FORMAT, center_label,  column_n,    "Format")  \
    X(SCORE,  center_label,  column_n,    "Score")   \
    X(GAME,   center_label,  column_n,    "Game")    \
    X(STAGE,  left_label,    column_n,    "Stage")

enum column
{
#define X(name, setup, bind, str) COL_##name,
    COLUMNS_LIST
#undef X
};

struct _VhAppGameTree;
static struct _VhAppGameTree* vhapp_game_tree_new(void);

struct _VhAppGameTreeEntry
{
    GObject parent_instance;
    struct _VhAppGameTree* children;
    struct strlist columns;
    int game_id;
    int fighter_ids[2][8];  /* [team][player slot] */
    int costumes[2][8];
};

#define VHAPP_TYPE_GAME_TREE_ENTRY (vhapp_game_tree_entry_get_type())
G_DECLARE_FINAL_TYPE(VhAppGameTreeEntry, vhapp_game_tree_entry, VHAPP, GAME_TREE_ENTRY, GObject);
G_DEFINE_TYPE(VhAppGameTreeEntry, vhapp_game_tree_entry, G_TYPE_OBJECT);

static void
vhapp_game_tree_entry_finalize(GObject* object)
{
    VhAppGameTreeEntry* self = VHAPP_GAME_TREE_ENTRY(object);
    strlist_deinit(&self->columns);
    G_OBJECT_CLASS(vhapp_game_tree_entry_parent_class)->finalize(object);
}

static VhAppGameTreeEntry*
vhapp_game_tree_entry_new_event(
    struct str_view date,
    struct str_view event_name)
{
    VhAppGameTreeEntry* obj = g_object_new(VHAPP_TYPE_GAME_TREE_ENTRY, NULL);

    strlist_init(&obj->columns);
    strlist_add_terminated(&obj->columns, date);
    strlist_add_terminated(&obj->columns, event_name);

    obj->children = vhapp_game_tree_new();

    obj->game_id = -1;
    obj->fighter_ids[0][0] = -1;
    obj->costumes[0][0] = -1;
    obj->fighter_ids[1][0] = -1;
    obj->costumes[1][0] = -1;

    return obj;
}

static VhAppGameTreeEntry*
vhapp_game_tree_entry_new_game(
    int game_id,
    struct str_view time,
    struct str_view team1,
    struct str_view team2,
    struct str_view round,
    struct str_view format,
    struct str_view score,
    struct str_view game,
    struct str_view stage)
{
    VhAppGameTreeEntry* obj = g_object_new(VHAPP_TYPE_GAME_TREE_ENTRY, NULL);

    strlist_init(&obj->columns);
    strlist_add_terminated(&obj->columns, time);
    strlist_add_terminated(&obj->columns, team1);
    strlist_add_terminated(&obj->columns, team2);
    strlist_add_terminated(&obj->columns, round);
    strlist_add_terminated(&obj->columns, format);
    strlist_add_terminated(&obj->columns, score);
    strlist_add_terminated(&obj->columns, game);
    strlist_add_terminated(&obj->columns, stage);

    obj->children = NULL;

    obj->game_id = game_id;
    obj->fighter_ids[0][0] = -1;
    obj->costumes[0][0] = -1;
    obj->fighter_ids[1][0] = -1;
    obj->costumes[1][0] = -1;

    return obj;
}

static void
vhapp_game_tree_entry_class_init(VhAppGameTreeEntryClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    object_class->finalize = vhapp_game_tree_entry_finalize;
}

static void
vhapp_game_tree_entry_init(VhAppGameTreeEntry* obj)
{
}

struct _VhAppGameTree
{
    GObject parent_instance;
    struct vec items;
};
struct _VhAppGameTreeClass
{
    GObject parent_class;
};

#define VHAPP_TYPE_GAME_TREE (vhapp_game_tree_get_type())
G_DECLARE_FINAL_TYPE(VhAppGameTree, vhapp_game_tree, VHAPP, GAME_TREE, GObject);

static GType
vhapp_game_tree_get_item_type(GListModel *list)
{
    return G_TYPE_OBJECT;
}

static guint
vhapp_game_tree_get_n_items(GListModel *list)
{
    VhAppGameTree* self = VHAPP_GAME_TREE(list);
    return vec_count(&self->items);
}

static gpointer
vhapp_game_tree_get_item(GListModel* list, guint position)
{
    VhAppGameTree* self = VHAPP_GAME_TREE(list);
    if (position >= vec_count(&self->items))
        return NULL;
    return g_object_ref(*(GObject**)vec_get(&self->items, position));
}

static void
vhapp_game_tree_model_init(GListModelInterface* iface)
{
    iface->get_item_type = vhapp_game_tree_get_item_type;
    iface->get_n_items = vhapp_game_tree_get_n_items;
    iface->get_item = vhapp_game_tree_get_item;
}

G_DEFINE_TYPE_WITH_CODE(VhAppGameTree, vhapp_game_tree, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL, vhapp_game_tree_model_init))

static void
vhapp_game_tree_dispose(GObject* object)
{
    VhAppGameTree* self = VHAPP_GAME_TREE(object);
    VEC_FOR_EACH(&self->items, GObject*, pobj)
        VhAppGameTreeEntry* obj = VHAPP_GAME_TREE_ENTRY(*pobj);
        if (obj->children)
            g_object_unref(obj->children);
        g_object_unref(obj);
    VEC_END_EACH
    vec_deinit(&self->items);
    G_OBJECT_CLASS(vhapp_game_tree_parent_class)->dispose(object);
}

static void
vhapp_game_tree_class_init(VhAppGameTreeClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    object_class->dispose = vhapp_game_tree_dispose;
}

static void
vhapp_game_tree_init(VhAppGameTree* self)
{
    vec_init(&self->items, sizeof(GObject*));
}

static VhAppGameTree*
vhapp_game_tree_new(void)
{
    return g_object_new(VHAPP_TYPE_GAME_TREE, NULL);
}

void
vhapp_game_tree_append(VhAppGameTree* self, VhAppGameTreeEntry* item)
{
    vec_push(&self->items, &item);
    g_list_model_items_changed(G_LIST_MODEL(self), vec_count(&self->items) - 1, 0, 1);
}

enum
{
    SIGNAL_GAMES_SELECTED,
    SIGNAL_COUNT
};

static gint game_browser_signals[SIGNAL_COUNT];

struct _VhAppGameBrowser
{
    GtkWidget parent_instance;
    VhAppGameTree* tree;
    GtkWidget* top_widget;
    struct vec selected_game_ids;
};

struct _VhAppGameBrowserClass
{
    GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(VhAppGameBrowser, vhapp_game_browser, GTK_TYPE_WIDGET)

static GListModel*
expand_node_cb(gpointer item, gpointer user_data)
{
    VhAppGameTreeEntry* obj = VHAPP_GAME_TREE_ENTRY(item);

    /* Can't expand nodes that aren't events */
    if (obj->children == NULL)
        return NULL;

    return G_LIST_MODEL(g_object_ref(obj->children));
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
    VhAppGameTreeEntry* entry = gtk_tree_list_row_get_item(row);
    enum column column = (enum column)(intptr_t)user_data;
    struct str_view str = strlist_view(&entry->columns, column);

    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), row);

    gtk_label_set_text(GTK_LABEL(label), str.data);
    g_object_unref(entry);
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
    enum column column = (enum column)(intptr_t)user_data;
    GtkWidget* box = gtk_list_item_get_child(item);
    GtkTreeListRow* row = gtk_list_item_get_item(item);
    VhAppGameTreeEntry* entry = gtk_tree_list_row_get_item(row);

    GtkWidget* image = gtk_widget_get_first_child(box);
    GtkWidget* label = gtk_widget_get_next_sibling(image);

    /*
     * The event list object only has strings for the first two columns
     * (date and name of the event), whereas game list objects have strings
     * for all columns. Because the label widgets get re-used in the UI, we
     * need to make sure to clear the text on the remaining columns if this is
     * an event list object.
     */
    if (entry->children)
        gtk_image_clear(GTK_IMAGE(image));
    else
    {
        int team = column == COL_TEAM1 ? 0 : 1;
        char* resource_path = fighter_icon_get_resource_path_from_id(
            entry->fighter_ids[team][0],
            entry->costumes[team][0]);
        gtk_image_set_from_resource(GTK_IMAGE(image), resource_path);
        fighter_icon_free_str(resource_path);
    }

    if (entry->children && column >= 2)
        gtk_label_set_text(GTK_LABEL(label), "");
    else
    {
        struct str_view str = strlist_view(&entry->columns, column);
        gtk_label_set_text(GTK_LABEL(label), str.data);
    }

    g_object_unref(entry);
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
    enum column column = (enum column)(intptr_t)user_data;
    GtkWidget* label = gtk_list_item_get_child(item);
    GtkTreeListRow* row = gtk_list_item_get_item(item);
    VhAppGameTreeEntry* entry = gtk_tree_list_row_get_item(row);

    /*
     * The event list object only has strings for the first two columns
     * (date and name of the event), whereas game list objects have strings
     * for all columns. Because the label widgets get re-used in the UI, we
     * need to make sure to clear the text on the remaining columns if this is
     * an event list object.
     */
    if (entry->children && column >= 2)
        gtk_label_set_text(GTK_LABEL(label), "");
    else
    {
        /* NOTE: We made sure to null-terminate these strings in the strlist */
        struct str_view str = strlist_view(&entry->columns, column);
        gtk_label_set_text(GTK_LABEL(label), str.data);
    }

    g_object_unref(entry);
}

struct on_game_ctx
{
    VhAppGameTree* tree;
    VhAppGameTree* games;

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
    VhAppGameTreeEntry* game_obj;

    char time_str[6];     /* HH:MM */
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
        VhAppGameTreeEntry* event_obj;
        char date_str[11];  /* YYYY-MM-DD */
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm);
        event_obj = vhapp_game_tree_entry_new_event(
                cstr_view(date_str),
                cstr_view(*event ? event : "Other"));
        ctx->games = event_obj->children;
        vhapp_game_tree_append(ctx->tree, event_obj);

        ctx->mday = tm->tm_mday;
        ctx->month = tm->tm_mon;
        ctx->year = tm->tm_year;
        ctx->event_id = event_id;
    }

    game_obj = vhapp_game_tree_entry_new_game(
        game_id,
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

    vhapp_game_tree_append(ctx->games, game_obj);

    return 0;
}

static void
populate_tree_from_db(VhAppGameTree* tree, struct db_interface* dbi, struct db* db)
{
    struct on_game_ctx on_game_ctx = { 0 };
    on_game_ctx.tree = tree;

    log_dbg("Querying games...\n");
    dbi->game.get_all(db, on_game, &on_game_ctx);
    log_dbg("Loaded %d games\n", dbi->game.count(db));
}

static void
column_view_activate_cb(GtkColumnView* self, guint position, gpointer user_pointer)
{
    GtkSelectionModel* selection_model = gtk_column_view_get_model(self);
    GListModel* model = gtk_multi_selection_get_model(GTK_MULTI_SELECTION(selection_model));
    GtkTreeListRow* row = gtk_tree_list_model_get_row(GTK_TREE_LIST_MODEL(model), position);

    gtk_tree_list_row_set_expanded(row,
        !gtk_tree_list_row_get_expanded(row));

    g_object_unref(row);
}

static void
selection_changed_cb(GtkSelectionModel* self, guint position_hint, guint n_items, gpointer user_data)
{
    GtkBitsetIter iter;
    guint position;
    GListModel* model = gtk_multi_selection_get_model(GTK_MULTI_SELECTION(self));
    VhAppGameBrowser* game_browser = user_data;

    vec_clear(&game_browser->selected_game_ids);
    gtk_bitset_iter_init_at(&iter, gtk_selection_model_get_selection(self), position_hint, &position);
    for (; gtk_bitset_iter_is_valid(&iter); gtk_bitset_iter_next(&iter, &position))
    {
        GtkTreeListRow* row = gtk_tree_list_model_get_row(GTK_TREE_LIST_MODEL(model), position);
        VhAppGameTreeEntry* entry = gtk_tree_list_row_get_item(row);

        if (entry->children == NULL)
            vec_push(&game_browser->selected_game_ids, &entry->game_id);

        g_object_unref(entry);
        g_object_unref(row);
    }

    if (vec_count(&game_browser->selected_game_ids) > 0)
        g_signal_emit(game_browser, game_browser_signals[SIGNAL_GAMES_SELECTED], 0,
            vec_data(&game_browser->selected_game_ids),
            (int)vec_count(&game_browser->selected_game_ids));
}

static GtkWidget*
create_game_list(VhAppGameTree* tree, VhAppGameBrowser* game_browser)
{
    GtkTreeListModel* model;
    GtkMultiSelection* selection_model;
    GtkWidget* column_view;
    GtkListItemFactory* item_factory;
    GtkColumnViewColumn* column;

    model = gtk_tree_list_model_new(G_LIST_MODEL(tree), FALSE, FALSE, expand_node_cb, NULL, NULL);
    gtk_tree_list_model_set_autoexpand(model, TRUE);

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
    COLUMNS_LIST
#undef X

    g_signal_connect(column_view, "activate", G_CALLBACK(column_view_activate_cb), game_browser);
    g_signal_connect(selection_model, "selection-changed", G_CALLBACK(selection_changed_cb), game_browser);

    return column_view;
}

static GtkWidget*
create_top_widget(GtkWidget* game_list)
{
    GtkWidget* search;
    GtkWidget* games;
    GtkWidget* scroll;
    GtkWidget* vbox;
    GtkWidget* groups;
    GtkWidget* paned;

    search = gtk_entry_new();
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(search), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");

    scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(scroll), TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), game_list);

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
vhapp_game_browser_init(VhAppGameBrowser* self)
{
    vec_init(&self->selected_game_ids, sizeof(int));
}

static void
vhapp_game_browser_dispose(GObject* object)
{
    VhAppGameBrowser* self = VHAPP_GAME_BROWSER(object);
    gtk_widget_unparent(self->top_widget);
    vec_deinit(&self->selected_game_ids);
    mem_track_deallocation(object);
    G_OBJECT_CLASS(vhapp_game_browser_parent_class)->dispose(object);
}

static void
vhapp_game_browser_class_init(VhAppGameBrowserClass* class)
{
    GObjectClass* object_class = G_OBJECT_CLASS(class);
    object_class->dispose = vhapp_game_browser_dispose;
    gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class), GTK_TYPE_BIN_LAYOUT);

    game_browser_signals[SIGNAL_GAMES_SELECTED] = g_signal_new("games-selected",
        G_OBJECT_CLASS_TYPE(object_class),
        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
        0,
        NULL, NULL,
        NULL,
        G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_INT);
}

GtkWidget*
vhapp_game_browser_new(struct db_interface* dbi, struct db* db)
{
    GtkWidget* game_list;
    VhAppGameBrowser* game_browser = g_object_new(VHAPP_TYPE_GAME_BROWSER, NULL);
    game_browser->tree = vhapp_game_tree_new();
    game_list = create_game_list(game_browser->tree, game_browser);
    populate_tree_from_db(game_browser->tree, dbi, db);
    game_browser->top_widget = create_top_widget(game_list);
    gtk_widget_set_parent(game_browser->top_widget, GTK_WIDGET(game_browser));

    mem_track_allocation(game_browser);

    return GTK_WIDGET(game_browser);
}

void
vhapp_game_browser_refresh(VhAppGameBrowser* self, struct db_interface* dbi, struct db* db)
{
    populate_tree_from_db(self->tree, dbi, db);
}
