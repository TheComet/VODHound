#include "application/import.h"

#include "vh/db_ops.h"
#include "vh/init.h"
#include "vh/log.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"
#include "vh/str.h"
#include "vh/vec.h"

#include "iup.h"
#include "iupgfx.h"

#include <stdio.h>
#include <ctype.h>
#include <time.h>

struct plugin_view_popup_ctx
{
    Ihandle* tabs;
    Ihandle* menu;
};

struct plugin_state
{
    struct plugin plugin;
    struct plugin_ctx* ctx;
    Ihandle* ui;
};

static int
plugin_view_open_plugin(Ihandle* plugin_view, struct str_view plugin_name)
{
    int insert_pos;
    struct vec* plugin_state_vec = (struct vec*)IupGetAttribute(plugin_view, "_IUP_plugin_state_vec");
    struct plugin_state* state = vec_emplace(plugin_state_vec);

    if (plugin_load(&state->plugin, plugin_name) != 0)
        goto load_plugin_failed;

    state->ctx = state->plugin.i->create();
    if (state->ctx == NULL)
        goto create_context_failed;

    state->ui = state->plugin.i->ui->create(state->ctx);
    if (state->ui == NULL)
        goto create_ui_failed;

    insert_pos = IupGetChildCount(plugin_view) - 1;
    IupSetAttribute(state->ui, "TABTITLE", state->plugin.i->name);
    if (IupInsert(plugin_view, IupGetChild(plugin_view, insert_pos), state->ui) == NULL)
        goto add_to_ui_failed;
    IupSetInt(plugin_view, "VALUEPOS", insert_pos);

    IupMap(state->ui);
    IupRefresh(state->ui);

    return 0;

    add_to_ui_failed      : state->plugin.i->ui->destroy(state->ctx, state->ui);
    create_ui_failed      : state->plugin.i->destroy(state->ctx);
    create_context_failed : plugin_unload(&state->plugin);
    load_plugin_failed    : vec_pop(plugin_state_vec);

    return -1;
}

struct query_game_ctx
{
    struct db_interface* dbi;
    struct db* db;
    Ihandle* replays;
    struct str name;
};

static int on_game_query(
    int game_id,
    uint64_t time_started,
    uint64_t time_ended,
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
    struct query_game_ctx* ctx = user;
    struct str_view team1, team2, fighter1, fighter2, score1, score2;
    int s1, s2, game_number;

    char datetime[17];
    char node_attr[19];  /* "ADDLEAF" + "-2147483648" */

    time_started = time_started / 1000;
    strftime(datetime, sizeof(datetime), "%y-%m-%d %H:%M", localtime((time_t*)&time_started));

    str_split2(cstr_view(teams), ',', &team1, &team2);
    str_split2(cstr_view(fighters), ',', &fighter1, &fighter2);
    str_split2(cstr_view(scores), ',', &score1, &score2);

    str_dec_to_int(score1, &s1);
    str_dec_to_int(score2, &s2);
    game_number = s1 + s2 + 1;

    str_fmt(&ctx->name, "%s - %s %s - %.*s (%.*s) vs %.*s (%.*s) Game %d",
        datetime, round, format,
        team1.len, team1.data, fighter1.len, fighter1.data,
        team2.len, team2.data, fighter2.len, fighter2.data,
        game_number);
    str_terminate(&ctx->name);

    IupSetInt(ctx->replays, "VALUE", 0);  /* Select the "Replays" root node */
    int insert_id = IupGetInt(ctx->replays, "CHILDCOUNT");  /* Number of children in the "Replays" root node */
    int node_id = insert_id + 1;  /* Account for there being 1 root node */
    sprintf(node_attr, "ADDLEAF%d", insert_id);
    IupSetAttribute(ctx->replays, node_attr, ctx->name.data);
    IupTreeSetUserId(ctx->replays, node_id, (void*)(intptr_t)game_id);

    return 0;
}

struct on_replay_browser_video_path_ctx
{
    struct vec* plugin_state_vec;
    int64_t frame_offset;
    struct str_view file_name;
    struct path file_path;
};

static int
on_replay_browser_video_path(const char* path, void* user)
{
    int combined_success = 0;
    struct on_replay_browser_video_path_ctx* ctx = user;

    path_set(&ctx->file_path, cstr_view(path));
    path_join(&ctx->file_path, ctx->file_name);
    path_terminate(&ctx->file_path);

    if (!fs_file_exists(ctx->file_path.str.data))
        return 0;

    VEC_FOR_EACH(ctx->plugin_state_vec, struct plugin_state, state)
        struct plugin_interface* i = state->plugin.i;
        if (i->video == NULL)
            continue;
        combined_success |= i->video->open_file(state->ctx, ctx->file_path.str.data, 1) == 0;
    VEC_END_EACH

    return combined_success;
}

static int
on_replay_browser_node_selected(Ihandle* ih, int node_id, int selected)
{
    Ihandle* plugin_view;
    struct db_interface* dbi;
    struct db* db;
    struct on_replay_browser_video_path_ctx ctx;
    const char* file_name;
    const char* path_hint;
    int game_id;

    if (!selected)
        return IUP_DEFAULT;

    game_id = (int)(intptr_t)IupTreeGetUserId(ih, node_id);
    dbi = (struct db_interface*)IupGetAttribute(ih, "dbi");
    db = (struct db*)IupGetAttribute(ih, "db");
    plugin_view = IupGetHandle("plugin_view");
    ctx.plugin_state_vec = (struct vec*)IupGetAttribute(plugin_view, "_IUP_plugin_state_vec");

    /* Close all open video files */
    VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
        struct plugin_interface* i = state->plugin.i;
        if (i->video == NULL)
            continue;
        if (i->video->is_open(state->ctx))
            i->video->close(state->ctx);
    VEC_END_EACH

    /* If no video is associated with this game, clear and return */
    if (dbi->game.get_video(db, game_id, &file_name, &path_hint, &ctx.frame_offset) <= 0)
        goto clear_video;

    /* Try the path hint first */
    path_init(&ctx.file_path);
    ctx.file_name = cstr_view(file_name);
    if (on_replay_browser_video_path(path_hint, &ctx) > 0)
    {
        path_deinit(&ctx.file_path);
        return IUP_DEFAULT;
    }

    /* Will have to search video paths for the video file */
    if (dbi->video.query_paths(db, on_replay_browser_video_path, &ctx) > 0)
    {
        path_dirname(&ctx.file_path);
        dbi->video.set_path_hint(db, ctx.file_name, path_view(ctx.file_path));
        path_deinit(&ctx.file_path);
        return IUP_DEFAULT;
    }

    path_deinit(&ctx.file_path);

    /* If video failed to open, clear */
clear_video:
    VEC_FOR_EACH(ctx.plugin_state_vec, struct plugin_state, state)
        struct plugin_interface* i = state->plugin.i;
        if (i->video == NULL)
            continue;
        if (!i->video->is_open(state->ctx))
            i->video->clear(state->ctx);
    VEC_END_EACH

    return IUP_DEFAULT;
}

static int
on_filter_action(Ihandle* ih, int c, const char* text)
{
    return IUP_DEFAULT;
}

static int
on_filter_caret(Ihandle* ih, int lin, int col, int pos)
{
    struct str_view text = cstr_view(IupGetAttribute(ih, "VALUE"));
    struct dbi_interface* dbi = (struct dbi_interface*)IupGetAttribute(ih, "dbi");
    struct db* db = (struct db*)IupGetAttribute(ih, "db");


    return IUP_DEFAULT;
}

static Ihandle*
create_replay_browser(struct db_interface* dbi, struct db* db)
{
    Ihandle *groups, *filter_label, *filters, *replays;
    Ihandle *vbox, *hbox, *sbox;

    groups = IupSetAttributes(IupList(NULL), "EXPAND=YES, 1=All, VALUE=1");
    sbox = IupSetAttributes(IupSbox(groups), "DIRECTION=SOUTH, COLOR=255 255 255");

    filter_label = IupSetAttributes(IupLabel("Filters:"), "PADDING=10x5");
    filters = IupSetAttributes(IupText(NULL), "EXPAND=HORIZONTAL");
    IupSetAttribute(filters, "dbi", (char*)dbi);
    IupSetAttribute(filters, "db", (char*)db);
    IupSetCallback(filters, "ACTION", (Icallback)on_filter_action);
    IupSetCallback(filters, "CARET_CB", (Icallback)on_filter_caret);

    hbox = IupHbox(filter_label, filters, NULL);

    replays = IupTree();
    IupSetCallback(replays, "SELECTION_CB", (Icallback)on_replay_browser_node_selected);
    IupSetHandle("replay_browser", replays);

    return IupVbox(sbox, hbox, replays, NULL);
}

static int
on_plugin_view_popup_plugin_selected(Ihandle* ih)
{
    Ihandle* plugin_view = IupGetAttributeHandle(ih, "plugin_view");
    plugin_view_open_plugin(plugin_view, cstr_view(IupGetAttribute(ih, "TITLE")));
    return IUP_DEFAULT;
}

static int
on_plugin_view_popup_scan_plugins(struct plugin plugin, void* user)
{
    struct plugin_view_popup_ctx* ctx = user;
    Ihandle* item = IupItem(plugin.i->name, NULL);
    IupSetAttributeHandle(item, "plugin_view", ctx->tabs);
    IupSetCallback(item, "ACTION", (Icallback)on_plugin_view_popup_plugin_selected);
    IupAppend(ctx->menu, item);
    return 0;
}

static int
on_plugin_view_tab_change(Ihandle* ih, int new_pos, int old_pos)
{
    /*
     * If the very last tab is selected (should be the "+" tab), prevent the
     * tab from actually being selected, but instead open a popup menu listing
     * available plugins to load.
     */
    int tab_count = IupGetChildCount(ih);
    if (new_pos == tab_count - 1)
    {
        struct plugin_view_popup_ctx ctx = {
            ih,
            IupMenu(NULL)
        };

        IupSetInt(ih, "VALUEPOS", old_pos);  /* Prevent tab selection */
        plugins_scan(on_plugin_view_popup_scan_plugins, &ctx);
        IupPopup(ctx.menu, IUP_MOUSEPOS, IUP_MOUSEPOS);
        IupDestroy(ctx.menu);
    }

    return IUP_DEFAULT;
}

static int
on_plugin_view_map(Ihandle* ih)
{
    struct vec* plugin_state_vec = vec_alloc(sizeof(struct plugin_state));
    IupSetAttribute(ih, "_IUP_plugin_state_vec", (char*)plugin_state_vec);
    return IUP_DEFAULT;
}
static int
on_plugin_view_unmap(Ihandle* ih)
{
    struct vec* plugin_state_vec = (struct vec*)IupGetAttribute(ih, "_IUP_plugin_state_vec");
    vec_free(plugin_state_vec);
    return IUP_DEFAULT;
}
static int
on_plugin_view_close(Ihandle* ih)
{
    struct vec* plugin_state_vec = (struct vec*)IupGetAttribute(ih, "_IUP_plugin_state_vec");
    VEC_FOR_EACH(plugin_state_vec, struct plugin_state, state)
        if (state->plugin.i->video && state->plugin.i->video->is_open(state->ctx))
            state->plugin.i->video->close(state->ctx);

        IupDetach(state->ui);
        state->plugin.i->ui->destroy(state->ctx, state->ui);
        state->plugin.i->destroy(state->ctx);
        plugin_unload(&state->plugin);
    VEC_END_EACH
    vec_clear(plugin_state_vec);

    return IUP_DEFAULT;
}
static int
on_plugin_view_close_tab(Ihandle* ih, int pos)
{
    struct vec* plugin_state_vec = (struct vec*)IupGetAttribute(ih, "_IUP_plugin_state_vec");
    Ihandle* ui = IupGetChild(ih, pos);
    VEC_FOR_EACH(plugin_state_vec, struct plugin_state, state)
        if (state->ui == ui)
        {
            if (state->plugin.i->video && state->plugin.i->video->is_open(state->ctx))
                state->plugin.i->video->close(state->ctx);

            IupDetach(state->ui);
            state->plugin.i->ui->destroy(state->ctx, state->ui);
            state->plugin.i->destroy(state->ctx);
            plugin_unload(&state->plugin);
            vec_erase_element(plugin_state_vec, state);

            return IUP_IGNORE;  /* We already destroyed the tab */
        }
    VEC_END_EACH

    return IUP_DEFAULT;
}

static Ihandle*
create_plugin_view(void)
{
    Ihandle* empty_tab = IupSetAttributes(IupCanvas(NULL), "TABTITLE=home");
    Ihandle* plus_tab = IupSetAttributes(IupCanvas(NULL), "TABTITLE=+");
    Ihandle* tabs = IupSetAttributes(IupTabs(empty_tab, plus_tab, NULL), "SHOWCLOSE=NO");
    IupSetCallback(tabs, "MAP_CB", (Icallback)on_plugin_view_map);
    IupSetCallback(tabs, "UNMAP_CB", (Icallback)on_plugin_view_unmap);
    IupSetCallback(tabs, "TABCHANGEPOS_CB", (Icallback)on_plugin_view_tab_change);
    IupSetCallback(tabs, "TABCLOSE_CB", (Icallback)on_plugin_view_close_tab);
    IupSetHandle("plugin_view", tabs);
    return tabs;
}

static Ihandle*
create_center_view(struct db_interface* dbi, struct db* db)
{
    Ihandle *replays, *plugins;
    Ihandle *sbox, *hbox;

    replays = create_replay_browser(dbi, db);
    plugins = create_plugin_view();
    sbox = IupSetAttributes(IupSbox(replays), "DIRECTION=EAST, COLOR=225 225 225");
    return IupHbox(sbox, plugins, NULL);
}

static Ihandle*
create_menus(void)
{
    Ihandle* item_connect = IupItem("&Connect...", NULL);
    Ihandle* item_import_rfp = IupItem("&Import Replay Pack\tCtrl+I", NULL);
    Ihandle* item_quit = IupItem("&Quit\t\t\t\t\tCtrl+Q", NULL);
    Ihandle* file_menu = IupMenu(
        item_connect,
        item_import_rfp,
        IupSeparator(),
        item_quit,
        NULL);

    Ihandle* item_motion_labels_editor = IupItem("Motion Labels Editor", NULL);
    Ihandle* item_path_manager = IupItem("Path Manager", NULL);
    Ihandle* tools_menu = IupMenu(
        item_motion_labels_editor,
        item_path_manager,
        NULL);

    Ihandle* sub_menu_file = IupSubmenu("&File", file_menu);
    Ihandle* sub_menu_tools = IupSubmenu("&Tools", tools_menu);

    return IupMenu(
        sub_menu_file,
        sub_menu_tools,
        NULL);
}

static Ihandle*
create_statusbar(void)
{
    return IupSetAttributes(IupLabel("Disconnected"),
        "NAME=STATUSBAR, EXPAND=HORIZONTAL, PADDING=10x5, ALIGNMENT=ARIGHT");
}

static Ihandle*
create_main_dialog(struct db_interface* dbi, struct db* db)
{
    Ihandle* vbox = IupVbox(
        create_center_view(dbi, db),
        create_statusbar(),
        NULL);

    Ihandle* dlg = IupDialog(vbox);
    IupSetAttributeHandle(dlg, "MENU", create_menus());
    IupSetAttribute(dlg, "TITLE", "VODHound");
    IupSetAttribute(dlg, "MINSIZE", "600x600");
    return dlg;
}

int main(int argc, char **argv)
{
    if (vh_threadlocal_init() != 0)
        goto vh_init_tl_failed;
    if (vh_init() != 0)
        goto vh_init_failed;

    int reinit_db = 0;

    struct db_interface* dbi = db("sqlite");
    struct db* db = dbi->open_and_prepare("vodhound.db", reinit_db);
    if (db == NULL)
        goto open_db_failed;

    if (reinit_db)
    {
        import_param_labels_csv(dbi, db, "ParamLabels.csv");
        import_reframed_mapping_info(dbi, db, "migrations/mappingInfo.json");
        import_reframed_all(dbi, db);
    }

    if (IupOpen(&argc, &argv) != IUP_NOERROR)
        goto open_iup_failed;

    IupSetGlobal("UTF8MODE", "Yes");
    IupGfxOpen();

    Ihandle* dlg = create_main_dialog(dbi, db);
    IupMap(dlg);

    Ihandle* replays = IupGetHandle("replay_browser");
    IupSetAttribute(replays, "TITLE", "Replays");
    IupSetAttribute(replays, "dbi", (char*)dbi);
    IupSetAttribute(replays, "db", (char*)db);

    {
        struct query_game_ctx ctx;
        ctx.dbi = dbi;
        ctx.db = db;
        ctx.replays = replays;
        str_init(&ctx.name);

        dbi->game.query(db, on_game_query, &ctx);

        str_deinit(&ctx.name);
    }

    Ihandle* plugin_view = IupGetHandle("plugin_view");
    plugin_view_open_plugin(plugin_view, cstr_view("VOD Review"));

    IupSetAttribute(dlg, "PLACEMENT", "MAXIMIZED");
    IupShow(dlg);

    IupMainLoop();

    on_plugin_view_close(plugin_view);

    IupDestroy(dlg);
    IupGfxClose();
    IupClose();

    dbi->close(db);
    vh_deinit();
    vh_threadlocal_deinit();

    return 0;

    open_iup_failed    : dbi->close(db);
    open_db_failed     : vh_deinit();
    vh_init_failed     : vh_threadlocal_deinit();
    vh_init_tl_failed  : return -1;
}
