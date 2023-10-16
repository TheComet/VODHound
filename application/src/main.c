#include "vh/db_ops.h"
#include "vh/init.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/mfile.h"
#include "vh/mstream.h"
#include "vh/plugin.h"
#include "vh/plugin_loader.h"
#include "vh/str.h"
#include "vh/vec.h"

#include "iup.h"
#include "iupgfx.h"

#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include "json-c/json.h"
#include "zlib.h"

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }
static int import_hash40(struct db_interface* dbi, struct db* db, const char* file_name)
{
    struct mfile mf;
    struct mstream ms;

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s'\n", file_name);
        goto open_file_failed;
    }

    log_info("Importing hash40 strings from '%s'\n", file_name);

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

int import_rfr_metadata_1_5_into_db(struct db_interface* dbi, struct db* db, struct json_object* root)
{
    char* err_msg;
    int ret;

    /*
    {
        "gameinfo":{
            "format":"Coaching",
            "number" : 14,
            "set" : 1, 
            "stageid" : 0,
            "timestampend" : 1661373123783,
            "timestampstart" : 1661372917135,
            "winner" : 1
        },
        "playerinfo" : [
            {
                "fighterid":8,
                "name" : "TheComet",
                "tag" : "Player 1"
            },
            {
                "fighterid":91,
                "name" : "Mitch",
                "tag" : "Player 2"
            }
        ] ,
        "type" : "game",
        "version" : "1.5",
        "videoinfo" : {
            "filename":"",
            "filepath" : "",
            "offsetms" : ""
         }
    }*/

    struct json_object* game_info = json_object_object_get(root, "gameinfo");
    const char* event_type = "Singles Bracket";  /* Most likely singles */
    const char* set_format = json_object_get_string(json_object_object_get(game_info, "format"));
    int set_format_id;
    {
        const char* long_name;
        if (strcmp(set_format, "Bo3") == 0)       long_name = "Best of 3";
        else if (strcmp(set_format, "Bo5") == 0)  long_name = "Best of 5";
        else if (strcmp(set_format, "Bo7") == 0)  long_name = "Best of 7";
        else if (strcmp(set_format, "FT5") == 0)  long_name = "First to 5";
        else if (strcmp(set_format, "FT10") == 0) long_name = "First to 10";
        else
        {
            /* In 1.5, there was no "event" property yet, so often, things like "Coaching" or "Practice" were
             * stored into the "set format" property. */
            event_type = set_format;
            set_format = "Free";
            long_name = "Freeplay";
        }

        set_format_id = dbi->set_format_add_or_get(db, cstr_view(set_format), cstr_view(long_name));
        if (set_format_id < 0)
            return -1;
    }

    /* Only create an entry in the bracket type table if it is NOT friendlies */
    int event_id = -1;
    if (strcmp(event_type, "Friendlies"))
    {
        int event_type_id;
        if ((event_type_id = dbi->event_type_add_or_get(db, cstr_view(event_type))) < 0)
            return -1;
        if ((event_id = dbi->event_add_or_get(db, event_type_id, cstr_view(""))) < 0)
            return -1;
    }

    int round_type_id = -1;
    int round_number = -1;
    if (strcmp(event_type, "Singles Bracket") == 0)
    {
        round_type_id = dbi->round_type_add_or_get(db, cstr_view("WR"), cstr_view("Winner's Round"));
        if (round_type_id < 0)
            return -1;

        round_number = json_object_get_int(json_object_object_get(game_info, "set"));
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
        const char* social = "";
        const char* pronouns = "";

        int person_id = dbi->person_add_or_get(db,
            sponsor_id,
            cstr_view(name),
            cstr_view(tag),
            cstr_view(social),
            cstr_view(pronouns));
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
        round_type_id,
        round_number,
        set_format_id,
        winner_team_id,
        stage_id,
        time_started,
        time_ended);
    if (game_id < 0)
        return -1;

    if (event_id != -1)
        if (dbi->game_associate_event(db, game_id, event_id) < 0)
            return -1;

    for (int i = 0; i != json_object_array_length(player_info); ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, i);
        const char* name = json_object_get_string(json_object_object_get(player, "name"));
        int is_loser_side = 0;
        int fighter_id = json_object_get_int(json_object_object_get(player, "fighterid"));
        int costume = 0;

        int person_id = dbi->person_get_id(db, cstr_view(name));
        if (person_id < 0)
            return -1;

        int team_id = dbi->person_get_team_id(db, cstr_view(name));
        if (team_id < 0)
            return -1;

        if (dbi->game_player_add(db, person_id, game_id, i, team_id, fighter_id, costume, is_loser_side) != 0)
            return -1;

        /* 1.5 did not have scores yet, but we would still like to have a valid game number.
         * The game number is calculated off of the score. Tracking the scores over multiple
         * games is possible, but we are lazy and simply set the score of player1 here.
         */
        int game_number = json_object_get_int(json_object_object_get(game_info, "number"));
        int score = i == 0 ? game_number - 1 : 0;
        if (score < 0)
            score = 0;
        if (dbi->score_add(db, game_id, team_id, score) != 0)
            return -1;
    }

    return game_id;
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
        if (website == NULL)
            website = "";
        if (name && *name)
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
            if (website == NULL)
                website = "";
            if (name && *name)
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
    if (event_type == NULL || !*event_type)
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
    int round_number = -1;
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
        const char* long_name;
        if (strcmp(set_format, "Bo3") == 0)       long_name = "Best of 3";
        else if (strcmp(set_format, "Bo5") == 0)  long_name = "Best of 5";
        else if (strcmp(set_format, "Bo7") == 0)  long_name = "Best of 7";
        else if (strcmp(set_format, "FT5") == 0)  long_name = "First to 5";
        else if (strcmp(set_format, "FT10") == 0) long_name = "First to 10";
        else
        {
            set_format = "Free";
            long_name = "Freeplay";
        }

        set_format_id = dbi->set_format_add_or_get(db, cstr_view(set_format), cstr_view(long_name));
        if (set_format_id < 0)
            return -1;
    }

    int round_type_id = -1;
    if (strcmp(set_format, "Free") == 0)
    {
        round_number = -1;
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

        round_type_id = dbi->round_type_add_or_get(db, round_type, cstr_view(long_name));
        if (round_type_id < 0)
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
        round_type_id,
        round_number,
        set_format_id,
        winner_team_id,
        stage_id,
        time_started,
        time_ended);
    if (game_id < 0)
        return -1;

    if (tournament_id != -1)
        if (dbi->game_associate_tournament(db, game_id, tournament_id) < 0)
            return -1;

    if (event_id != -1)
        if (dbi->game_associate_event(db, game_id, event_id) < 0)
            return -1;

    for (int i = 0; i != json_object_array_length(player_info); ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, i);
        const char* name = json_object_get_string(json_object_object_get(player, "name"));
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

    if (strcmp(version_str, "1.7") == 0)
    {
        game_id = import_rfr_metadata_1_7_into_db(dbi, db, root);
        if (game_id < 0)
            goto fail;
    }
    else if (strcmp(version_str, "1.5") == 0)
    {
        game_id = import_rfr_metadata_1_5_into_db(dbi, db, root);
        if (game_id < 0)
            goto fail;
    }
    else
    {
        log_err("Failed to import RFR: Unsupported metadata version %s\n", version_str);
        goto fail;
    }

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

static int
import_rfr_video_metadata_1_0_into_db(struct db_interface* dbi, struct db* db, struct json_object* root, int game_id)
{
    int video_id;
    const char* file_name = json_object_get_string(json_object_object_get(root, "filename"));
    int64_t offset = json_object_get_int64(json_object_object_get(root, "offset"));
    if (file_name == NULL || !*file_name)
        return 0;

    video_id = dbi->video_add_or_get(db, cstr_view(file_name), cstr_view(""));
    if (video_id < 0)
        return -1;
    if (dbi->game_associate_video(db, game_id, video_id, offset) < 0)
        return -1;

    return 0;
}

int import_rfr_video_metadata_into_db(struct db_interface* dbi, struct db* db, struct mstream* ms, int game_id)
{
    struct json_tokener* tok = json_tokener_new();
    struct json_object* root = json_tokener_parse_ex(tok, ms->address, ms->size);
    json_tokener_free(tok);

    if (root == NULL)
        goto parse_failed;

    struct json_object* version = json_object_object_get(root, "version");
    const char* version_str = json_object_get_string(version);
    if (version_str == NULL)
        goto fail;

    if (strcmp(version_str, "1.0") == 0)
    {
        if (import_rfr_video_metadata_1_0_into_db(dbi, db, root, game_id) < 0)
            goto fail;
    }
    else
        goto fail;

    json_object_put(root);
    return 0;

    fail         : json_object_put(root);
    parse_failed : return -1;
}

int import_rfr_into_db(struct db_interface* dbi, struct db* db, const char* file_name)
{
    struct blob_entry
    {
        const void* type;
        int offset;
        int size;
    } entries[3];

    struct mfile mf;
    struct mstream ms;

    uint8_t num_entries;
    int i;
    int entry_idx;
    int game_id;

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

    /*
     * Blobs can be in any order within the RFR file. We have to import them
     * in a specific order for the db operations to work. This order is:
     *   1) META (metadata)
     *   2) VIDM (video metadata) depends on game_id from META
     *   3) FDAT (frame data) depends on game_id from META
     * The "MAPI" (mapping info) blob doesn't need to be loaded, because we
     * create the mapping info structures from a JSON file now.
     */
    num_entries = mstream_read_u8(&ms);
    for (i = 0, entry_idx = 0; i != num_entries; ++i)
    {
        const void* type = mstream_read(&ms, 4);
        int offset = mstream_read_lu32(&ms);
        int size = mstream_read_lu32(&ms);
        if (entry_idx < 3 &&
            (memcmp(type, "META", 4) == 0 || memcmp(type, "FDAT", 4) == 0 || memcmp(type, "VIDM", 4) == 0))
        {
            entries[entry_idx].type = type;
            entries[entry_idx].offset = offset;
            entries[entry_idx].size = size;
            entry_idx++;
        }
    }

    num_entries = entry_idx;
    game_id = -1;
    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "META", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            game_id = import_rfr_metadata_into_db(dbi, db, &blob);
            if (game_id < 0)
                goto fail;
            break;
        }
    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "VIDM", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            if (import_rfr_video_metadata_into_db(dbi, db, &blob, game_id) < 0)
                goto fail;
            break;
        }
#if 0
    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "FDAT", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            if (import_rfr_framedata_into_db(dbi, db, &blob, game_id) < 0)
                goto fail;
            break;
        }
#endif

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
import_all_rfr(struct db_interface* dbi, struct db* db)
{
    do {
//#include "rfr_files_less.h"
    } while (0);
}

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
    if (dbi->game_get_video(db, game_id, &file_name, &path_hint, &ctx.frame_offset) <= 0)
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
    if (dbi->video_paths_query(db, on_replay_browser_video_path, &ctx) > 0)
    {
        path_dirname(&ctx.file_path);
        dbi->video_set_path_hint(db, ctx.file_name, path_view(ctx.file_path));
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

static Ihandle*
create_replay_browser(void)
{
    Ihandle* groups, * filter_label, * filters, * replays;
    Ihandle* vbox, * hbox, * sbox;

    groups = IupSetAttributes(IupList(NULL), "EXPAND=YES, 1=All, VALUE=1");
    sbox = IupSetAttributes(IupSbox(groups), "DIRECTION=SOUTH, COLOR=255 255 255");

    filter_label = IupSetAttributes(IupLabel("Filters:"), "PADDING=10x5");
    filters = IupSetAttributes(IupText(NULL), "EXPAND=HORIZONTAL");
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
create_center_view(void)
{
    Ihandle *replays, *plugins;
    Ihandle *sbox, *hbox;

    replays = create_replay_browser();
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

static int ignore_plugins(struct plugin plugin, void* user) { return 0; }

struct on_rfr_file_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct path file_path;
};

static int
on_rfr_file(const char* name, void* user)
{
    struct on_rfr_file_ctx* ctx = user;
    path_join(&ctx->file_path, cstr_view(name));
    path_terminate(&ctx->file_path);
    import_rfr_into_db(ctx->dbi, ctx->db, ctx->file_path.str.data);
    path_dirname(&ctx->file_path);

    return 0;
}

int main(int argc, char **argv)
{
    if (vh_init() != 0)
        goto vh_init_failed;

    struct db_interface* dbi = db("sqlite");
    struct db* db = dbi->open_and_prepare("vodhound.db", 0);
    if (db == NULL)
        goto open_db_failed;

#if 0
    import_mapping_info(dbi, db, "migrations/mappingInfo.json");
    import_hash40(dbi, db, "ParamLabels.csv");
#endif

#if 0
    import_all_rfr(dbi, db);
#endif

#if 0
    struct on_rfr_file_ctx ctx = { dbi, db };
    path_init(&ctx.file_path);
    path_set(&ctx.file_path, cstr_view("reframed"));
    fs_list(path_view(ctx.file_path), on_rfr_file, &ctx);
    path_deinit(&ctx.file_path);
#endif

#if 0
    dbi->video_path_add(db, cstr_view("C:\\Users\\Startklar\\Downloads"));
    dbi->video_path_add(db, cstr_view("C:\\Users\\AlexanderMurray\\Downloads"));
    dbi->video_path_add(db, cstr_view("/home/thecomet/videos/ssbu/2023-09-05 - Stino"));
#endif

    if (IupOpen(&argc, &argv) != IUP_NOERROR)
        goto open_iup_failed;

    IupSetGlobal("UTF8MODE", "Yes");
    IupGfxOpen();

    Ihandle* dlg = create_main_dialog();

    IupSetAttribute(dlg, "PLACEMENT", "MAXIMIZED");
    IupShow(dlg);

    Ihandle* replays = IupGetHandle("replay_browser");
    IupSetAttribute(replays, "TITLE", "Replays");
    IupSetAttribute(replays, "dbi", (char*)dbi);
    IupSetAttribute(replays, "db", (char*)db);
    /*
    IupSetAttribute(replays, "ADDBRANCH", "2023-08-20");
    IupSetAttribute(replays, "ADDLEAF1", "19:45 Game 1");
    IupSetAttribute(replays, "ADDLEAF2", "19:52 Game 2");
    IupSetAttribute(replays, "INSERTBRANCH1", "2023-08-22");
    IupSetAttribute(replays, "ADDLEAF4", "12:25 Game 1");
    IupSetAttribute(replays, "ADDLEAF5", "12:28 Game 2");
    IupSetAttribute(replays, "ADDLEAF6", "12:32 Game 3");*/

    {
        struct query_game_ctx ctx;
        ctx.dbi = dbi;
        ctx.db = db;
        ctx.replays = replays;
        str_init(&ctx.name);

        dbi->games_query(db, on_game_query, &ctx);

        str_deinit(&ctx.name);
    }

    Ihandle* plugin_view = IupGetHandle("plugin_view");
    plugin_view_open_plugin(plugin_view, cstr_view("VOD Review"));

    IupMainLoop();

    on_plugin_view_close(plugin_view);

    IupDestroy(dlg);
    IupGfxClose();
    IupClose();

    dbi->close(db);
    vh_deinit();

    return EXIT_SUCCESS;

    open_iup_failed : dbi->close(db);
    open_db_failed  : vh_deinit();
    vh_init_failed  : return -1;
}
