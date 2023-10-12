#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/mfile.h"
#include "vh/mstream.h"
#include "vh/str.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"

#include "iup.h"
#include "iupgfx.h"

#include <stdio.h>
#include <ctype.h>

#include "json-c/json.h"
#include "zlib.h"

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }
static int import_hash40(struct db_interface* dbi, struct db* db, const char* file_name)
{
    struct mfile mf;
    struct mstream ms;

    log_info("Importing hash40 strings from '%s'\n", file_name);

    if (mfile_map(&mf, file_name) != 0)
        goto open_file_failed;

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    ms = mstream_from_mfile(&mf);

    while (!mstream_at_end(&ms))
    {
        /* Extract hash40 and label, splitting on comma */
        struct str_view h40_str, label;
        if (mstream_read_string_until_delim(&ms, ',', &h40_str) != 0)
            break;
        if (mstream_read_string_until_condition(&ms, newline_or_end, &label) != 0)
            break;

        /* Convert hash40 into value */
        uint64_t h40;
        if (str_hex_to_u64(h40_str, &h40) != 0)
            continue;

        if (h40 == 0 || label.len == 0)
            continue;

        if (dbi->motion_add(db, h40, label) != 0)
            goto add_failed;
    }

    dbi->transaction_commit(db);
    mfile_unmap(&mf);

    return 0;

    add_failed               : dbi->transaction_rollback(db);
    transaction_begin_failed : mfile_unmap(&mf);
    open_file_failed         : return -1;
}

static int
import_mapping_info(struct db_interface* dbi, struct db* db, const char* file_name)
{
    json_object* root = json_object_from_file(file_name);
    json_object* jversion = json_object_object_get(root, "version");

    json_object* statuses = json_object_object_get(root, "fighterstatus");
    json_object* fighter_ids = json_object_object_get(root, "fighterid");
    json_object* stage_ids = json_object_object_get(root, "stageid");
    json_object* hit_statuses = json_object_object_get(root, "hitstatus");

    json_object* base_statuses = json_object_object_get(statuses, "base");
    json_object* specific_statuses = json_object_object_get(statuses, "specific");

    if (root == NULL)
    {
        log_err("File '%s' not found\n", file_name);
        return -1;
    }

    log_info("Importing mapping info from '%s'\n", file_name);
    log_dbg("mapping info version: %s\n", json_object_get_string(jversion));

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    { json_object_object_foreach(fighter_ids, fighter_id_str, fighter_name)
    {
        int fighter_id = atoi(fighter_id_str);
        const char* name = json_object_get_string(fighter_name);
        if (dbi->fighter_add(db, fighter_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(stage_ids, stage_id_str, stage_name)
    {
        int stage_id = atoi(stage_id_str);
        const char* name = json_object_get_string(stage_name);
        if (dbi->stage_add(db, stage_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(base_statuses, status_str, enum_name)
    {
        int status_id = atoi(status_str);
        const char* name = json_object_get_string(enum_name);
        if (dbi->status_enum_add(db, -1, status_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(specific_statuses, fighter_id_str, fighter)
    {
        int fighter_id = atoi(fighter_id_str);
        json_object_object_foreach(fighter, status_str, enum_name)
        {
            int status_id = atoi(status_str);
            const char* name = json_object_get_string(enum_name);
            if (dbi->status_enum_add(db, fighter_id, status_id, cstr_view(name)) != 0)
                goto fail;
        }
    }}

    { json_object_object_foreach(hit_statuses, hit_status_str, hit_status_name)
    {
        int hit_status_id = atoi(hit_status_str);
        const char* name = json_object_get_string(hit_status_name);
        if (dbi->hit_status_enum_add(db, hit_status_id, cstr_view(name)) != 0)
            goto fail;
    }}

    json_object_put(root);
    return dbi->transaction_commit(db);

    fail                     : dbi->transaction_rollback(db);
    transaction_begin_failed : json_object_put(root);
    unsupported_version      : return -1;
}

int import_rfr_metadata_1_7_into_db(struct db_interface* dbi, struct db* db, struct json_object* root)
{
    char* err_msg;
    int ret;

    struct json_object* tournament = json_object_object_get(root, "tournament");
    struct json_object* tournament_name = json_object_object_get(tournament, "name");
    struct json_object* tournament_website = json_object_object_get(tournament, "website");
    int tournament_id = -1;
    {
        const char* name = json_object_get_string(tournament_name);
        const char* website = json_object_get_string(tournament_website);
        if (name && website && *name)
        {
            tournament_id = dbi->tournament_add_or_get(db, cstr_view(name), cstr_view(website));
            if (tournament_id < 0)
                return -1;
        }
    }

    struct json_object* tournament_sponsors = json_object_object_get(tournament, "sponsors");
    if (tournament_sponsors && json_object_get_type(tournament_sponsors) == json_type_array)
        for (int i = 0; i != json_object_array_length(tournament_sponsors); ++i)
        {
            int sponsor_id = -1;
            struct json_object* sponsor = json_object_array_get_idx(tournament_sponsors, i);
            const char* name = json_object_get_string(json_object_object_get(sponsor, "name"));
            const char* website = json_object_get_string(json_object_object_get(sponsor, "website"));
            if (name && website && *name)
            {
                sponsor_id = dbi->sponsor_add_or_get(db, cstr_view(""), cstr_view(name), cstr_view(website));
                if (sponsor_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && sponsor_id != -1)
                if (dbi->tournament_sponsor_add(db, tournament_id, sponsor_id) != 0)
                    return -1;
        }

    struct json_object* tournament_organizers = json_object_object_get(tournament, "organizers");
    if (tournament_organizers && json_object_get_type(tournament_organizers) == json_type_array)
        for (int i = 0; i != json_object_array_length(tournament_organizers); ++i)
        {
            int person_id = -1;
            struct json_object* organizer = json_object_array_get_idx(tournament_organizers, i);
            const char* name = json_object_get_string(json_object_object_get(organizer, "name"));
            const char* social = json_object_get_string(json_object_object_get(organizer, "social"));
            const char* pronouns = json_object_get_string(json_object_object_get(organizer, "pronouns"));
            if (name && *name)
            {
                person_id = dbi->person_add_or_get(db,
                        -1,
                        cstr_view(name),
                        cstr_view(name),
                        cstr_view(social ? social : ""),
                        cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament_organizer_add(db, tournament_id, person_id) != 0)
                    return -1;
        }

    struct json_object* tournament_commentators = json_object_object_get(root, "commentators");
    if (tournament_commentators && json_object_get_type(tournament_commentators) == json_type_array)
        for (int i = 0; i != json_object_array_length(tournament_commentators); ++i)
        {
            int person_id = -1;
            struct json_object* commentator = json_object_array_get_idx(tournament_commentators, i);
            const char* name = json_object_get_string(json_object_object_get(commentator, "name"));
            const char* social = json_object_get_string(json_object_object_get(commentator, "social"));
            const char* pronouns = json_object_get_string(json_object_object_get(commentator, "pronouns"));
            if (name && *name)
            {
                person_id = dbi->person_add_or_get(db,
                        -1,
                        cstr_view(name),
                        cstr_view(name),
                        cstr_view(social ? social : ""),
                        cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament_commentator_add(db, tournament_id, person_id) != 0)
                    return -1;
        }

    struct json_object* event = json_object_object_get(root, "event");
    const char* event_type = json_object_get_string(json_object_object_get(event, "type"));
    if (event_type == NULL || !event_type)
        event_type = "Friendlies";  /* fallback to friendlies */
    /* Only create an entry in the bracket type table if it is NOT friendlies */
    int event_id = -1;
    if (strcmp(event_type, "Friendlies"))
    {
        int event_type_id;
        if ((event_type_id = dbi->event_type_add_or_get(db, cstr_view(event_type))) < 0)
            return -1;

        const char* event_url = json_object_get_string(json_object_object_get(event, "url"));
        if (event_url == NULL)
            event_url = "";  /* Default is empty string for URL */
        if ((event_id = dbi->event_add_or_get(db, event_type_id, cstr_view(event_url))) < 0)
            return -1;
    }

    struct json_object* game_info = json_object_object_get(root, "gameinfo");

    /* This will equal something like "WR2" or "Pools 3". Want to parse it into
     * "WR" and the number 2, or "Pools" and the number 3, respectively. */
    struct str_view round_type = cstr_view(json_object_get_string(json_object_object_get(game_info, "round")));
    const char* round_number_cstr = round_type.data;
    int round_number = 1;
    while (*round_number_cstr && !isdigit(*round_number_cstr))
        round_number_cstr++;
    if (*round_number_cstr)
    {
        /* Remove digits and trailing space from round type string */
        while (round_type.data[round_type.len-1] == ' ' || isdigit(round_type.data[round_type.len-1]))
            round_type.len--;

        round_number = atoi(round_number_cstr);
        if (round_number < 1)
            round_number = 1;
    }

    const char* set_format = json_object_get_string(json_object_object_get(game_info, "format"));
    int set_format_id;
    {
        const char* long_name = "Freeplay";
        if (strcmp(set_format, "Bo3") == 0)       long_name = "Best of 3";
        else if (strcmp(set_format, "Bo5") == 0)  long_name = "Best of 5";
        else if (strcmp(set_format, "Bo7") == 0)  long_name = "Best of 7";
        else if (strcmp(set_format, "FT5") == 0)  long_name = "First to 5";
        else if (strcmp(set_format, "FT10") == 0) long_name = "First to 10";

        set_format_id = dbi->set_format_add_or_get(db, cstr_view(set_format), cstr_view(long_name));
        if (set_format_id < 0)
            return -1;
    }

    int round_id;
    if (strcmp(set_format, "Free") == 0)
    {
        if ((round_id = dbi->round_add_or_get(db, -1, round_number)) < 0)
            return -1;
    }
    else
    {
        const char* long_name = "Winner's Round";
        if (cstr_equal(round_type, "WR"))         long_name = "Winner's Round";
        else if (cstr_equal(round_type, "WQF"))   long_name = "Winner's Quarter Finals";
        else if (cstr_equal(round_type, "WSF"))   long_name = "Winner's Semi Finals";
        else if (cstr_equal(round_type, "WF"))    long_name = "Winner's Finals";
        else if (cstr_equal(round_type, "LR"))    long_name = "Loser's Round";
        else if (cstr_equal(round_type, "LQF"))   long_name = "Loser's Quarter Finals";
        else if (cstr_equal(round_type, "LSF"))   long_name = "Loser's Semi Finals";
        else if (cstr_equal(round_type, "LF"))    long_name = "Loser's Finals";
        else if (cstr_equal(round_type, "GF"))    long_name = "Grand Finals";
        else if (cstr_equal(round_type, "GFR"))   long_name = "Grand Finals Reset";
        else if (cstr_equal(round_type, "Pools")) long_name = "Pools";

        int round_type_id = dbi->round_type_add_or_get(db, round_type, cstr_view(long_name));
        if (round_type_id < 0)
            return -1;

        if ((round_id = dbi->round_add_or_get(db, round_type_id, round_number)) < 0)
            return -1;
    }

    uint64_t time_started = json_object_get_int64(json_object_object_get(game_info, "timestampstart"));
    uint64_t time_ended = json_object_get_int64(json_object_object_get(game_info, "timestampend"));
    int stage_id = json_object_get_int(json_object_object_get(game_info, "stageid"));
    int winner = json_object_get_int(json_object_object_get(game_info, "winner"));

    struct json_object* player_info = json_object_object_get(root, "playerinfo");
    int winner_team_id = -1;
    if (player_info && json_object_get_type(player_info) != json_type_array)
        return -1;
    for (int i = 0; i != json_object_array_length(player_info); ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, i);
        const char* name = json_object_get_string(json_object_object_get(player, "name"));
        const char* tag = json_object_get_string(json_object_object_get(player, "tag"));
        const char* social = json_object_get_string(json_object_object_get(player, "social"));
        const char* pronouns = json_object_get_string(json_object_object_get(player, "pronouns"));
        const char* sponsor = json_object_get_string(json_object_object_get(player, "sponsor"));

        if (tag == NULL || !*tag)
        {
            log_err("Player has no tag!\n");
            return -1;
        }

        if (name == NULL || !*name)
        {
            log_warn("Player has no name!\n");
            name = tag;
        }

        int sponsor_id = -1;
        if (*sponsor)
            if ((sponsor_id = dbi->sponsor_add_or_get(db, cstr_view(sponsor), cstr_view(""), cstr_view(""))) < 0)
                return -1;

        int person_id = dbi->person_add_or_get(db,
            sponsor_id,
            cstr_view(name),
            cstr_view(tag),
            cstr_view(social ? social : ""),
            cstr_view(pronouns ? pronouns : ""));
        if (person_id < 0)
            return -1;

        int team_id = dbi->team_add_or_get(db, cstr_view(name), cstr_view(""));
        if (team_id < 0)
            return -1;

        if (dbi->team_member_add(db, team_id, person_id) != 0)
            return -1;

        if (winner == i)
            winner_team_id = team_id;
    }

    int game_id = dbi->game_add_or_get(db,
         -1,
         tournament_id,
         event_id,
         round_id,
         set_format_id,
         winner_team_id,
         stage_id,
         time_started,
         time_ended);

    for (int i = 0; i != json_object_array_length(player_info); ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, i);
        const char* name = json_object_get_string(json_object_object_get(player, "name"));
        const char* tag = json_object_get_string(json_object_object_get(player, "tag"));
        const char* social = json_object_get_string(json_object_object_get(player, "social"));
        const char* pronouns = json_object_get_string(json_object_object_get(player, "pronouns"));
        const char* sponsor = json_object_get_string(json_object_object_get(player, "sponsor"));
        int is_loser_side = json_object_get_boolean(json_object_object_get(player, "loserside"));
        int fighter_id = json_object_get_int(json_object_object_get(player, "fighterid"));
        int costume = json_object_get_int(json_object_object_get(player, "costume"));

        int person_id = dbi->person_get_id(db, cstr_view(name));
        if (person_id < 0)
            return -1;

        int team_id = dbi->person_get_team_id(db, cstr_view(name));
        if (team_id < 0)
            return -1;

        if (dbi->game_player_add(db, person_id, game_id, i, team_id, fighter_id, costume, is_loser_side) != 0)
            return -1;

        struct json_object* score = json_object_object_get(game_info, i == 0 ? "score1" : "score2");
        if (score)
            if (dbi->score_add(db, game_id, team_id, json_object_get_int(score)) != 0)
                return -1;
    }

    return game_id;
}

int import_rfr_metadata_into_db(struct db_interface* dbi, struct db* db, struct mstream* ms)
{
    int game_id;
    struct json_tokener* tok = json_tokener_new();
    struct json_object* root = json_tokener_parse_ex(tok, ms->address, ms->size);
    json_tokener_free(tok);

    if (root == NULL)
        goto parse_failed;

    struct json_object* version = json_object_object_get(root, "version");
    const char* version_str = json_object_get_string(version);
    if (version_str == NULL)
        goto fail;

    if (strcmp(version_str, "1.5") == 0)
    {}
    else if (strcmp(version_str, "1.6") == 0)
    {}
    else if (strcmp(version_str, "1.7") == 0)
    {
        game_id = import_rfr_metadata_1_7_into_db(dbi, db, root);
        if (game_id < 0)
            goto fail;
    }
    else
        goto fail;

    json_object_put(root);
    return game_id;

    fail         : json_object_put(root);
    parse_failed : return -1;
}

int import_rfr_framedata_1_5_into_db(struct db_interface* dbi, struct db* db, struct mstream* ms, int game_id)
{
    int frame_count = mstream_read_lu32(ms);
    int fighter_count = mstream_read_u8(ms);

    for (int fighter_idx = 0; fighter_idx != fighter_count; ++fighter_idx)
        for (int frame = 0; frame != frame_count; ++frame)
        {
            uint64_t timestamp = mstream_read_lu64(ms);
            uint32_t frames_left = mstream_read_lu32(ms);
            float posx = mstream_read_lf32(ms);
            float posy = mstream_read_lf32(ms);
            float damage = mstream_read_lf32(ms);
            float hitstun = mstream_read_lf32(ms);
            float shield = mstream_read_lf32(ms);
            uint16_t status = mstream_read_lu16(ms);
            uint32_t motion_l = mstream_read_lu32(ms);
            uint8_t motion_h = mstream_read_u8(ms);
            uint8_t hit_status = mstream_read_u8(ms);
            uint8_t stocks = mstream_read_u8(ms);
            uint8_t flags = mstream_read_u8(ms);

            uint64_t motion = ((uint64_t)motion_h << 32) | motion_l;
            int attack_connected = (flags & 0x01) ? 1 : 0;
            int facing_left = (flags & 0x02) ? 1 : 0;
            int opponent_in_hitlag = (flags & 0x04) ? 1 : 0;

            if (mstream_past_end(ms))
                return -1;

            if (dbi->frame_add(db, game_id, fighter_idx, timestamp, frame,
                    frames_left, posx, posy, damage, hitstun, shield, status,
                    hit_status, motion, stocks, attack_connected, facing_left, opponent_in_hitlag) != 0)
                return -1;
        }

    if (!mstream_at_end(ms))
        return -1;

    return 0;
}

int import_rfr_framedata_into_db(struct db_interface* dbi, struct db* db, struct mstream* ms, int game_id)
{
    uint8_t major = mstream_read_u8(ms);
    uint8_t minor = mstream_read_u8(ms);
    if (major == 1 && minor == 5)
    {
        uLongf uncompressed_size = mstream_read_lu32(ms);
        if (uncompressed_size == 0 || uncompressed_size > 128*1024*1024)
            return -1;

        void* uncompressed_data = mem_alloc(uncompressed_size);
        if (uncompress(
            (uint8_t*)uncompressed_data, &uncompressed_size,
            (const uint8_t*)mstream_ptr(ms), mstream_bytes_left(ms)) != Z_OK)
        {
            mem_free(uncompressed_data);
            return -1;
        }

        struct mstream uncompressed_stream = mstream_from_memory(
                uncompressed_data, uncompressed_size);
        int result = import_rfr_framedata_1_5_into_db(dbi, db, &uncompressed_stream, game_id);
        mem_free(uncompressed_data);
        return result;
    }

    return -1;
}

int import_rfr_video_metadata_into_db(struct db_interface* dbi, struct db* db, struct mstream* ms)
{
    return 0;
}

int import_rfr_into_db(struct db_interface* dbi, struct db* db, const char* file_name)
{
    struct mfile mf;
    struct mstream ms;

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s'\n", file_name);
        goto mmap_failed;
    }

    log_info("Importing replay '%s'\n", file_name);

    ms = mstream_from_mfile(&mf);
    if (memcmp(mstream_read(&ms, 4), "RFR1", 4) != 0)
    {
        puts("File has invalid header");
        goto invalid_header;
    }

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    uint8_t num_entries = mstream_read_u8(&ms);
    int game_id = -1;
    for (int i = 0; i != num_entries; ++i)
    {
        const void* type = mstream_read(&ms, 4);
        int offset = mstream_read_lu32(&ms);
        int size = mstream_read_lu32(&ms);
        struct mstream blob = mstream_from_mstream(&ms, offset, size);

        if (memcmp(type, "META", 4) == 0)
        {
            game_id = import_rfr_metadata_into_db(dbi, db, &blob);
            if (game_id < 0)
                goto fail;
        }
        else if (memcmp(type, "FDAT", 4) == 0)
        {
            if (import_rfr_framedata_into_db(dbi, db, &blob, game_id) != 0)
                goto fail;
        }
        else if (memcmp(type, "VIDM", 4) == 0)
        {
            if (import_rfr_video_metadata_into_db(dbi, db, &blob) != 0)
                goto fail;
        }
    }

    if (dbi->transaction_commit(db) != 0)
        goto fail;

    mfile_unmap(&mf);
    return 0;

    fail                     : dbi->transaction_rollback(db);
    transaction_begin_failed :
    invalid_header           : mfile_unmap(&mf);
    mmap_failed              : return -1;
}

static void
import_all_rfr(struct db_interface* dbi, struct db*  db)
{
    do {

    } while (0);
}

static Ihandle*
create_replay_browser(void)
{
    Ihandle *groups, *filter_label, *filters, *replays;
    Ihandle *vbox, *hbox, *sbox;

    groups = IupList(NULL);
    IupSetAttributes(groups, "EXPAND=YES, 1=All, VALUE=1");

    sbox = IupSbox(groups);
    IupSetAttribute(sbox, "DIRECTION", "SOUTH");
    IupSetAttribute(sbox, "COLOR", "225 225 225");

    filter_label = IupLabel("Filters:");
    IupSetAttribute(filter_label, "PADDING", "10x5");
    filters = IupText(NULL);
    IupSetAttribute(filters, "EXPAND", "HORIZONTAL");
    hbox = IupHbox(filter_label, filters, NULL);

    replays = IupTree();
    IupSetHandle("replay_browser", replays);

    return IupVbox(sbox, hbox, replays, NULL);
}

static Ihandle*
create_plugin_view(void)
{
    Ihandle* empty_tab = IupCanvas(NULL);
    IupSetAttribute(empty_tab, "TABTITLE", "+");
    Ihandle* tabs = IupTabs(empty_tab, NULL);
    IupSetHandle("plugin_view", tabs);
    return tabs;
}

static Ihandle*
create_center_view(void)
{
    Ihandle *replays, *plugins;
    Ihandle *sbox, *hbox;

    replays = create_replay_browser();
    plugins = create_plugin_view();
    sbox = IupSbox(replays);
    IupSetAttribute(sbox, "COLOR", "225 225 225");
    IupSetAttribute(sbox, "DIRECTION", "EAST");
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
    Ihandle* connection_status = IupLabel("Disconnected");
    IupSetAttribute(connection_status, "NAME", "STATUSBAR");
    IupSetAttribute(connection_status, "EXPAND", "HORIZONTAL");
    IupSetAttribute(connection_status, "PADDING", "10x5");
    IupSetAttribute(connection_status, "ALIGNMENT", "ARIGHT");
    return connection_status;
}

static Ihandle*
create_main_dialog(void)
{
    Ihandle* vbox = IupVbox(
        create_center_view(),
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
    IupOpen(&argc, &argv);
    IupSetGlobal("UTF8MODE", "Yes");
    IupGfxOpen();
    Ihandle* dlg = create_main_dialog();

    /*
     * If we don't clear SIZE after showing the dialog, then when resizing the
     * window smaller than the size, RESIZE_CB is no longer called on some
     * widgets. Not sure if this is a bug or not, but SIZE seems to be the only
     * way to set the initial size of the window.
     */
    //IupSetAttribute(dlg, "SIZE", "400x400");
    IupShowXY(dlg, 0, 0);
    //IupSetAttribute(dlg, "SIZE", NULL);

    Ihandle* replays = IupGetHandle("replay_browser");
    IupSetAttribute(replays, "TITLE", "Replays");
    IupSetAttribute(replays, "ADDBRANCH", "2023-08-20");
    IupSetAttribute(replays, "ADDLEAF1", "19:45 Game 1");
    IupSetAttribute(replays, "ADDLEAF2", "19:52 Game 2");
    IupSetAttribute(replays, "INSERTBRANCH1", "2023-08-22");
    IupSetAttribute(replays, "ADDLEAF4", "12:25 Game 1");
    IupSetAttribute(replays, "ADDLEAF5", "12:28 Game 2");
    IupSetAttribute(replays, "ADDLEAF6", "12:32 Game 3");

    Ihandle* plugin_view = IupGetHandle("plugin_view");
    struct strlist sl;
    struct plugin plugin;
    struct plugin_ctx* plugin_ctx;
    Ihandle* plugin_ui;
    int video_open = 0;

    strlist_init(&sl);
    plugin_scan(&sl);
    for (int i = 0; i != (int)strlist_count(&sl); ++i)
    {
        struct str_view name = strlist_get(&sl, i);
        if (cstr_equal(name, "VOD Review"))
        {
            if (plugin_load(&plugin, strlist_get(&sl, i)) != 0)
                goto plugin_load_failed;
            if ((plugin_ctx = plugin.i->create()) == NULL)
                goto create_plugin_ctx_failed;
            if ((plugin_ui = plugin.i->ui->create(plugin_ctx)) == NULL)
                goto create_plugin_ui_failed;
            IupSetAttribute(plugin_ui, "TABTITLE", plugin.i->name);
            int insert_pos = IupGetChildCount(plugin_view) - 1;
            if (IupInsert(plugin_view, IupGetChild(plugin_view, insert_pos), plugin_ui) == NULL)
                goto add_to_ui_failed;
            IupMap(plugin_ui);
            IupRefresh(plugin_ui);
            IupSetInt(plugin_view, "VALUEPOS", insert_pos);
            //video_open = plugin.i->video->open_file(plugin_ctx, "C:\\Users\\Startklar\\Downloads\\Prefers_Land_Behind.mp4", 1) == 0;
            //video_open = plugin.i->video->open_file(plugin_ctx, "C:\\Users\\AlexanderMurray\\Downloads\\pika-dj-mixups.mp4", 1) == 0;
            video_open = plugin.i->video->open_file(plugin_ctx, "/home/thecomet/videos/ssbu/2023-09-05 - Stino/2023-09-05_19-49-31.mkv", 1) == 0;
            if (!video_open)
                goto open_video_failed;

            break;

            open_video_failed        : IupUnmap(plugin_ui);
            add_to_ui_failed         : plugin.i->ui->destroy(plugin_ctx, plugin_ui);
            create_plugin_ui_failed  : plugin.i->destroy(plugin_ctx);
            create_plugin_ctx_failed : plugin_unload(&plugin);
            plugin_load_failed       : break;
        }
    }
    strlist_deinit(&sl);

    IupMainLoop();

    if (video_open)
    {
        plugin.i->video->close(plugin_ctx);
        IupDetach(plugin_ui);
        IupRefresh(plugin_view);
        plugin.i->ui->destroy(plugin_ctx, plugin_ui);
        plugin.i->destroy(plugin_ctx);
        plugin_unload(&plugin);
    }

    IupDestroy(dlg);
    IupGfxClose();
    IupClose();

    return EXIT_SUCCESS;
    /*
    struct db_interface* dbi = db("sqlite");
    struct db* db = dbi->open_and_prepare("rf.db");
    if (db == NULL)
        goto open_db_failed;

    import_mapping_info(dbi, db, "migrations/mappingInfo.json");
    import_hash40(dbi, db, "ParamLabels.csv");
    import_all_rfr(dbi, db);*/

    /*
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-09-51 - Singles Bracket - Bo3 (Pools 1) - TheComet (Pikachu) vs Aff (Donkey Kong) - Game 1 (0-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-13-39 - Singles Bracket - Bo3 (Pools 1) - TheComet (Pikachu) vs Aff (Donkey Kong) - Game 2 (1-0) - Town and City.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-19-12 - Singles Bracket - Bo3 (Pools 2) - TheComet (Pikachu) vs Keppler (Roy) - Game 1 (0-0) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-23-38 - Singles Bracket - Bo3 (Pools 2) - TheComet (Pikachu) vs Keppler (Roy) - Game 2 (0-1) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-39-28 - Singles Bracket - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 1 (0-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-44-17 - Singles Bracket - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 2 (1-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-52-03 - Singles Bracket - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 3 (1-1) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_20-06-46 - Singles Bracket - Bo3 (Pools 4) - TheComet (Pikachu) vs karsten187 (Wolf) - Game 1 (0-0) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_20-11-47 - Singles Bracket - Bo3 (Pools 4) - TheComet (Pikachu) vs karsten187 (Wolf) - Game 2 (0-1) - Small Battlefield.rfr");
*/

    //dbi->close(db);
    return EXIT_SUCCESS;

    open_db_failed : return -1;
}
