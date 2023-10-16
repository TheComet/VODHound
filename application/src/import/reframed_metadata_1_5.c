#include "vh/db_ops.h"
#include "vh/log.h"

#include "json-c/json.h"

int
import_reframed_metadata_1_5(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root)
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
