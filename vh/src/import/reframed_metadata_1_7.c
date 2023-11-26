#include "vh/db.h"
#include "vh/log.h"

#include "json-c/json.h"

#include <ctype.h>

int
reframed_add_or_get_person_to_db(
    struct db_interface* dbi, struct db* db,
    int sponsor_id,
    struct str_view name, struct str_view tag,
    struct str_view social, struct str_view pronouns);

int
import_reframed_metadata_1_7(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root)
{
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
            tournament_id = dbi->tournament.add_or_get(db, cstr_view(name), cstr_view(website));
            if (tournament_id < 0)
                return -1;
        }
    }

    struct json_object* tournament_sponsors = json_object_object_get(tournament, "sponsors");
    if (tournament_sponsors && json_object_get_type(tournament_sponsors) == json_type_array)
        for (int i = 0; i != (int)json_object_array_length(tournament_sponsors); ++i)
        {
            int sponsor_id = -1;
            struct json_object* sponsor = json_object_array_get_idx(tournament_sponsors, (size_t)i);
            const char* name = json_object_get_string(json_object_object_get(sponsor, "name"));
            const char* website = json_object_get_string(json_object_object_get(sponsor, "website"));
            if (website == NULL)
                website = "";
            if (name && *name)
            {
                sponsor_id = dbi->sponsor.add_or_get(db, cstr_view(""), cstr_view(name), cstr_view(website));
                if (sponsor_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && sponsor_id != -1)
                if (dbi->tournament.add_sponsor(db, tournament_id, sponsor_id) != 0)
                    return -1;
        }

    struct json_object* tournament_organizers = json_object_object_get(tournament, "organizers");
    if (tournament_organizers && json_object_get_type(tournament_organizers) == json_type_array)
        for (int i = 0; i != (int)json_object_array_length(tournament_organizers); ++i)
        {
            int person_id = -1;
            struct json_object* organizer = json_object_array_get_idx(tournament_organizers, (size_t)i);
            const char* name = json_object_get_string(json_object_object_get(organizer, "name"));
            const char* social = json_object_get_string(json_object_object_get(organizer, "social"));
            const char* pronouns = json_object_get_string(json_object_object_get(organizer, "pronouns"));
            if (name && *name)
            {
                person_id = reframed_add_or_get_person_to_db(dbi, db,
                    -1,  /* No sponsor */
                    cstr_view(name),
                    cstr_view(name),
                    cstr_view(social ? social : ""),
                    cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament.add_organizer(db, tournament_id, person_id) != 0)
                    return -1;
        }

    struct json_object* tournament_commentators = json_object_object_get(root, "commentators");
    if (tournament_commentators && json_object_get_type(tournament_commentators) == json_type_array)
        for (int i = 0; i != (int)json_object_array_length(tournament_commentators); ++i)
        {
            int person_id = -1;
            struct json_object* commentator = json_object_array_get_idx(tournament_commentators, (size_t)i);
            const char* name = json_object_get_string(json_object_object_get(commentator, "name"));
            const char* social = json_object_get_string(json_object_object_get(commentator, "social"));
            const char* pronouns = json_object_get_string(json_object_object_get(commentator, "pronouns"));
            if (name && *name)
            {
                person_id = reframed_add_or_get_person_to_db(dbi, db,
                    -1,  /* No sponsor */
                    cstr_view(name),
                    cstr_view(name),
                    cstr_view(social ? social : ""),
                    cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament.add_commentator(db, tournament_id, person_id) != 0)
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
        if ((event_type_id = dbi->event.add_or_get_type(db, cstr_view(event_type))) < 0)
            return -1;

        const char* event_url = json_object_get_string(json_object_object_get(event, "url"));
        if (event_url == NULL)
            event_url = "";  /* Default is empty string for URL */
        if ((event_id = dbi->event.add_or_get(db, event_type_id, cstr_view(event_url))) < 0)
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

        set_format_id = dbi->set_format.add_or_get(db, cstr_view(set_format), cstr_view(long_name));
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

        round_type_id = dbi->round.add_or_get_type(db, round_type, cstr_view(long_name));
        if (round_type_id < 0)
            return -1;
    }

    uint64_t time_started = (uint64_t)json_object_get_int64(json_object_object_get(game_info, "timestampstart"));
    uint64_t time_ended = (uint64_t)json_object_get_int64(json_object_object_get(game_info, "timestampend"));
    int stage_id = json_object_get_int(json_object_object_get(game_info, "stageid"));
    int winner = json_object_get_int(json_object_object_get(game_info, "winner"));

    struct json_object* player_info = json_object_object_get(root, "playerinfo");
    int winner_team_id = -1;
    if (player_info && json_object_get_type(player_info) != json_type_array)
        return -1;
    int player_count = (int)json_object_array_length(player_info);
    int people_ids[4] = {-1, -1, -1, -1};
    for (int i = 0; i != player_count; ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, (size_t)i);
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

        if (cstr_starts_with(cstr_view(tag), "Player "))
            tag = name;

        int sponsor_id = -1;
        if (*sponsor)
            if ((sponsor_id = dbi->sponsor.add_or_get(db, cstr_view(sponsor), cstr_view(""), cstr_view(""))) < 0)
                return -1;

        int person_id = reframed_add_or_get_person_to_db(dbi, db,
            sponsor_id,
            cstr_view(name),
            cstr_view(tag),
            cstr_view(social ? social : ""),
            cstr_view(pronouns ? pronouns : ""));
        if (person_id < 0)
            return -1;
        if (i < 4)
            people_ids[i] = person_id;

        int team_id = dbi->team.add_or_get(db, cstr_view(name), cstr_view(""));
        if (team_id < 0)
            return -1;

        if (dbi->team.add_member(db, team_id, person_id) != 0)
            return -1;

        if (winner == i)
            winner_team_id = team_id;
    }

    /*
     * Checks if a game with the same timestamp and same people exists.
     * The reason for checking for person_id as well as timestamp is because
     * it is possible for two games to have the same timestamp but with
     * different players. By also checking if the game has the same players,
     * we support importing games with identical timestamps.
     */
    if (dbi->game.exists(db, time_started, people_ids, player_count))
    {
        log_warn("Duplicate rfr, skipping...\n");
        return -1;
    }

    int game_id = dbi->game.add(db,
        round_type_id,
        round_number,
        set_format_id,
        winner_team_id,
        stage_id,
        time_started,
        (int)(time_ended - time_started));
    if (game_id < 0)
        return -1;

    if (tournament_id != -1)
        if (dbi->game.associate_tournament(db, game_id, tournament_id) < 0)
            return -1;

    if (event_id != -1)
        if (dbi->game.associate_event(db, game_id, event_id) < 0)
            return -1;

    for (int i = 0; i != (int)json_object_array_length(player_info); ++i)
    {
        struct json_object* player = json_object_array_get_idx(player_info, (size_t)i);
        const char* name = json_object_get_string(json_object_object_get(player, "name"));
        int is_loser_side = json_object_get_boolean(json_object_object_get(player, "loserside"));
        int fighter_id = json_object_get_int(json_object_object_get(player, "fighterid"));
        int costume = json_object_get_int(json_object_object_get(player, "costume"));

        int person_id = dbi->person.get_id_from_name(db, cstr_view(name));
        if (person_id < 0)
            return -1;

        int team_id = dbi->person.get_team_id_from_name(db, cstr_view(name));
        if (team_id < 0)
            return -1;

        if (dbi->game.add_player(db, person_id, game_id, i, team_id, fighter_id, costume, is_loser_side) != 0)
            return -1;

        struct json_object* score = json_object_object_get(game_info, i == 0 ? "score1" : "score2");
        if (score)
            if (dbi->score.add(db, game_id, team_id, json_object_get_int(score)) != 0)
                return -1;
    }

    return game_id;
}
