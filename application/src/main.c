#include "rf/db_ops.h"
#include "rf/log.h"
#include "rf/mem.h"
#include "rf/mfile.h"
#include "rf/mstream.h"
#include "rf/str.h"

#include <stdio.h>

#include "json.h"
#include "zlib.h"

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }
static int import_hash40(struct rf_db_interface* dbi, struct rf_db* db, const char* file_name)
{
    struct rf_mfile mf;
    struct rf_mstream ms;

    rf_log_info("Importing hash40 strings from '%s'\n", file_name);

    if (rf_mfile_map(&mf, file_name) != 0)
        goto open_file_failed;

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    ms = rf_mstream_from_mfile(&mf);

    while (!rf_mstream_at_end(&ms))
    {
        /* Extract hash40 and label, splitting on comma */
        struct rf_str_view h40_str, label;
        if (rf_mstream_read_string_until_delim(&ms, ',', &h40_str) != 0)
            break;
        if (rf_mstream_read_string_until_condition(&ms, newline_or_end, &label) != 0)
            break;

        /* Convert hash40 into value */
        uint64_t h40;
        if (rf_str_hex_to_u64(h40_str, &h40) != 0)
            continue;

        if (h40 == 0 || label.len == 0)
            continue;

        if (dbi->motion_add(db, h40, label) != 0)
            goto add_failed;
    }

    dbi->transaction_commit(db);
    rf_mfile_unmap(&mf);

    return 0;

add_failed               : dbi->transaction_rollback(db);
transaction_begin_failed : rf_mfile_unmap(&mf);
open_file_failed         : return -1;
}

static int
import_mapping_info(struct rf_db_interface* dbi, struct rf_db* db, const char* file_name)
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
        rf_log_err("File '%s' not found\n", file_name);
        return -1;
    }

    rf_log_info("Importing mapping info from '%s'\n", file_name);
    rf_log_dbg("mapping info version: %s\n", json_object_get_string(jversion));

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    { json_object_object_foreach(fighter_ids, fighter_id_str, fighter_name)
    {
        int fighter_id = atoi(fighter_id_str);
        const char* name = json_object_get_string(fighter_name);
        if (dbi->fighter_add(db, fighter_id, rf_cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(stage_ids, stage_id_str, stage_name)
    {
        int stage_id = atoi(stage_id_str);
        const char* name = json_object_get_string(stage_name);
        if (dbi->stage_add(db, stage_id, rf_cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(base_statuses, status_str, enum_name)
    {
        int status_id = atoi(status_str);
        const char* name = json_object_get_string(enum_name);
        if (dbi->status_enum_add(db, -1, status_id, rf_cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(specific_statuses, fighter_id_str, fighter)
    {
        int fighter_id = atoi(fighter_id_str);
        json_object_object_foreach(fighter, status_str, enum_name)
        {
            int status_id = atoi(status_str);
            const char* name = json_object_get_string(enum_name);
            if (dbi->status_enum_add(db, fighter_id, status_id, rf_cstr_view(name)) != 0)
                goto fail;
        }
    }}

    { json_object_object_foreach(hit_statuses, hit_status_str, hit_status_name)
    {
        int hit_status_id = atoi(hit_status_str);
        const char* name = json_object_get_string(hit_status_name);
        if (dbi->hit_status_enum_add(db, hit_status_id, rf_cstr_view(name)) != 0)
            goto fail;
    }}

    json_object_put(root);
    return dbi->transaction_commit(db);

fail                     : dbi->transaction_rollback(db);
transaction_begin_failed : json_object_put(root);
unsupported_version      : return -1;
}

int import_rfr_metadata_1_7_into_db(struct rf_db_interface* dbi, struct rf_db* db, struct json_object* root)
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
            tournament_id = dbi->tournament_add_or_get(db, rf_cstr_view(name), rf_cstr_view(website));
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
                sponsor_id = dbi->sponsor_add_or_get(db, rf_cstr_view(""), rf_cstr_view(name), rf_cstr_view(website));
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
                        rf_cstr_view(name),
                        rf_cstr_view(name),
                        rf_cstr_view(social ? social : ""),
                        rf_cstr_view(pronouns ? pronouns : ""));
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
                        rf_cstr_view(name),
                        rf_cstr_view(name),
                        rf_cstr_view(social ? social : ""),
                        rf_cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament_commentator_add(db, tournament_id, person_id) != 0)
                    return -1;
        }

    struct json_object* event = json_object_object_get(root, "event");
    const char* bracket_type = json_object_get_string(json_object_object_get(event, "type"));
    int bracket_type_id;
    if (bracket_type == NULL || !bracket_type)
        bracket_type = "Friendlies";  /* fallback to friendlies */
    if ((bracket_type_id = dbi->bracket_type_add_or_get(db, rf_cstr_view(bracket_type))) < 0)
        return -1;

    const char* bracket_url = json_object_get_string(json_object_object_get(event, "url"));
    int bracket_id;
    if (bracket_url == NULL)
        bracket_url = "";  /* Default is empty string for URL */
    if ((bracket_id = dbi->bracket_add_or_get(db, bracket_type_id, rf_cstr_view(bracket_url))) < 0)
        return -1;

    struct json_object* game_info = json_object_object_get(root, "gameinfo");

    struct json_object* time_started = json_object_object_get(game_info, "timestampstart");
    struct json_object* time_ended = json_object_object_get(game_info, "timestampend");
    struct json_object* stage_id = json_object_object_get(game_info, "stageid");
    struct json_object* winner = json_object_object_get(game_info, "winner");

    /*
    struct json_object* player_info = json_object_object_get(root, "playerinfo");
    if (player_info && json_object_get_type(player_info) == json_type_array)
        for (int i = 0; i != json_object_array_length(player_info); ++i)
        {
            int person_id = -1;
            struct json_object* player = json_object_array_get_idx(player_info, i);
            const char* name = json_object_get_string(json_object_object_get(player, "name"));
            const char* social = json_object_get_string(json_object_object_get(player, "social"));
            const char* pronouns = json_object_get_string(json_object_object_get(player, "pronouns"));
            if (name && *name)
            {
                person_id = dbi->person_add_or_get(db,
                    -1,
                    rf_cstr_view(name),
                    rf_cstr_view(name),
                    rf_cstr_view(social ? social : ""),
                    rf_cstr_view(pronouns ? pronouns : ""));
                if (person_id < 0)
                    return -1;
            }

            if (tournament_id != -1 && person_id != -1)
                if (dbi->tournament_commentator_add(db, tournament_id, person_id) != 0)
                    return -1;
        }*/

    return 0;
}

int import_rfr_metadata_into_db(struct rf_db_interface* dbi, struct rf_db* db, struct rf_mstream* ms)
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

    if (strcmp(version_str, "1.5") == 0)
    {}
    else if (strcmp(version_str, "1.6") == 0)
    {}
    else if (strcmp(version_str, "1.7") == 0)
    {
        if (import_rfr_metadata_1_7_into_db(dbi, db, root) != 0)
            goto fail;
    }
    else
        goto fail;

    json_object_put(root);
    return 0;

    fail         : json_object_put(root);
    parse_failed : return -1;
}

int import_rfr_framedata_1_5_into_db(struct rf_db_interface* dbi, struct rf_db* db, struct rf_mstream* ms)
{
    int frame_count = rf_mstream_read_lu32(ms);
    int fighter_count = rf_mstream_read_u8(ms);

    for (int fighter_idx = 0; fighter_idx != fighter_count; ++fighter_idx)
        for (int frame = 0; frame != frame_count; ++frame)
        {
            uint64_t timestamp = rf_mstream_read_lu64(ms);
            uint32_t frames_left = rf_mstream_read_lu32(ms);
            float posx = rf_mstream_read_lf32(ms);
            float posy = rf_mstream_read_lf32(ms);
            float damage = rf_mstream_read_lf32(ms);
            float hitstun = rf_mstream_read_lf32(ms);
            float shield = rf_mstream_read_lf32(ms);
            uint16_t status = rf_mstream_read_lu16(ms);
            uint32_t motion_l = rf_mstream_read_lu32(ms);
            uint8_t motion_h = rf_mstream_read_u8(ms);
            uint8_t hit_status = rf_mstream_read_u8(ms);
            uint8_t stocks = rf_mstream_read_u8(ms);
            uint8_t flags = rf_mstream_read_u8(ms);

            if (rf_mstream_past_end(ms))
                return -1;

            //printf("frame: %d, x: %f, y: %f, stocks: %d\n", frames_left, posx, posy, stocks);
        }

    if (!rf_mstream_at_end(ms))
        return -1;

    return 0;
}

int import_rfr_framedata_into_db(struct rf_db_interface* dbi, struct rf_db* db, struct rf_mstream* ms)
{
    uint8_t major = rf_mstream_read_u8(ms);
    uint8_t minor = rf_mstream_read_u8(ms);
    if (major == 1 && minor == 5)
    {
        uLongf uncompressed_size = rf_mstream_read_lu32(ms);
        if (uncompressed_size == 0 || uncompressed_size > 128*1024*1024)
            return -1;

        void* uncompressed_data = rf_malloc(uncompressed_size);
        if (uncompress(
            (uint8_t*)uncompressed_data, &uncompressed_size,
            (const uint8_t*)rf_mstream_ptr(ms), rf_mstream_bytes_left(ms)) != Z_OK)
        {
            rf_free(uncompressed_data);
            return -1;
        }

        struct rf_mstream uncompressed_stream = rf_mstream_from_memory(
                uncompressed_data, uncompressed_size);
        int result = import_rfr_framedata_1_5_into_db(dbi, db, &uncompressed_stream);
        rf_free(uncompressed_data);
        return result;
    }

    return -1;
}

int import_rfr_video_metadata_into_db(struct rf_db_interface* dbi, struct rf_db* db, struct rf_mstream* ms)
{
    return 0;
}

int import_rfr_into_db(struct rf_db_interface* dbi, struct rf_db* db, const char* file_name)
{
    struct rf_mfile mf;
    struct rf_mstream ms;

    if (rf_mfile_map(&mf, file_name) != 0)
    {
        rf_log_err("Failed to open file '%s'\n", file_name);
        goto mmap_failed;
    }

    rf_log_info("Importing replay '%s'\n", file_name);

    ms = rf_mstream_from_mfile(&mf);
    if (memcmp(rf_mstream_read(&ms, 4), "RFR1", 4) != 0)
    {
        puts("File has invalid header");
        goto invalid_header;
    }

    uint8_t num_entries = rf_mstream_read_u8(&ms);
    for (int i = 0; i != num_entries; ++i)
    {
        const void* type = rf_mstream_read(&ms, 4);
        int offset = rf_mstream_read_lu32(&ms);
        int size = rf_mstream_read_lu32(&ms);
        struct rf_mstream blob = rf_mstream_from_mstream(&ms, offset, size);

        if (memcmp(type, "META", 4) == 0)
        {
            if (import_rfr_metadata_into_db(dbi, db, &blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "FDAT", 4) == 0)
        {
            if (import_rfr_framedata_into_db(dbi, db, &blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "VIDM", 4) == 0)
        {
            if (import_rfr_video_metadata_into_db(dbi, db, &blob) != 0)
                goto fail;
        }
    }

    rf_mfile_unmap(&mf);
    return 0;

    fail           :
    invalid_header : rf_mfile_unmap(&mf);
    mmap_failed    : return -1;
}

int main(int argc, char** argv)
{
    struct rf_db_interface* dbi = rf_db("sqlite");
    struct rf_db* db = dbi->open_and_prepare("rf.db");
    if (db == NULL)
        goto open_db_failed;

    import_mapping_info(dbi, db, "migrations/mappingInfo.json");
    import_hash40(dbi, db, "ParamLabels.csv");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-09-51 -  - Bo3 (Pools 1) - TheComet (Pikachu) vs Aff (Donkey Kong) - Game 1 (0-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-13-39 -  - Bo3 (Pools 1) - TheComet (Pikachu) vs Aff (Donkey Kong) - Game 2 (1-0) - Town and City.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-19-12 -  - Bo3 (Pools 2) - TheComet (Pikachu) vs Keppler (Roy) - Game 1 (0-0) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-23-38 -  - Bo3 (Pools 2) - TheComet (Pikachu) vs Keppler (Roy) - Game 2 (0-1) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-39-28 -  - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 1 (0-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-44-17 -  - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 2 (1-0) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_19-52-03 -  - Bo3 (Pools 3) - TaDavidID (Villager) vs TheComet (Pikachu) - Game 3 (1-1) - Hollow Bastion.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_20-06-46 -  - Bo3 (Pools 4) - TheComet (Pikachu) vs karsten187 (Wolf) - Game 1 (0-0) - Small Battlefield.rfr");
    import_rfr_into_db(dbi, db, "reframed/2023-09-20_20-11-47 -  - Bo3 (Pools 4) - TheComet (Pikachu) vs karsten187 (Wolf) - Game 2 (0-1) - Small Battlefield.rfr");

    dbi->close(db);
    return 0;

open_db_failed: return -1;
}
