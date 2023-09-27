#include "rf/log.h"
#include "rf/mem.h"
#include "rf/mfile.h"
#include "rf/mstream.h"
#include "rf/str.h"

#include "sqlite/sqlite3.h"
#include "json-c/json.h"
#include "zlib.h"

#include <stdio.h>

static void insert_mapping_info(sqlite3* db)
{
    sqlite3_stmt* stmt_insert_fighter;
    sqlite3_prepare_v2(db, "INSERT INTO fighters (id, name) VALUES (?, ?);", -1, &stmt_insert_fighter, NULL);

    sqlite3_stmt* stmt_insert_stage;
    sqlite3_prepare_v2(db, "INSERT INTO stages (id, name) VALUES (?, ?);", -1, &stmt_insert_stage, NULL);

    sqlite3_stmt* stmt_insert_status_enum;
    sqlite3_prepare_v2(db, "INSERT INTO status_enums (fighter_id, value, name) VALUES (?, ?, ?);", -1, &stmt_insert_status_enum, NULL);

    sqlite3_stmt* stmt_insert_hit_status_enum;
    sqlite3_prepare_v2(db, "INSERT INTO hit_status_enums (id, name) VALUES (?, ?);", -1, &stmt_insert_hit_status_enum, NULL);

    json_object* root = json_object_from_file("/home/thecomet/.local/share/ReFramed/mappingInfo.json");
    json_object* jversion = json_object_object_get(root, "version");
    fprintf(stderr, "mapping info version: %s\n", json_object_get_string(jversion));

    json_object* statuses = json_object_object_get(root, "fighterstatus");
    json_object* fighter_ids = json_object_object_get(root, "fighterid");
    json_object* stage_ids = json_object_object_get(root, "stageid");
    json_object* hit_statuses = json_object_object_get(root, "hitstatus");

    char* error_message;
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &error_message);
    printf("BEGIN TRANSACTION;\n");

    {
        json_object_object_foreach(fighter_ids, fighter_id_str, fighter_name)
        {
            int fighter_id = atoi(fighter_id_str);
            const char* name = json_object_get_string(fighter_name);
            printf("INSERT INTO fighters (id, name) VALUES (%d, '%s');\n", fighter_id, name);

            sqlite3_bind_int(stmt_insert_fighter, 1, fighter_id);
            sqlite3_bind_text(stmt_insert_fighter, 2, name, -1, SQLITE_STATIC);
            sqlite3_step(stmt_insert_fighter);

            sqlite3_reset(stmt_insert_fighter);
        }
    }

    {
        json_object_object_foreach(stage_ids, stage_id_str, stage_name)
        {
            int stage_id = atoi(stage_id_str);
            const char* name = json_object_get_string(stage_name);
            printf("INSERT INTO stages (id, name) VALUES (%d, '%s');\n", stage_id, name);

            sqlite3_bind_int(stmt_insert_stage, 1, stage_id);
            sqlite3_bind_text(stmt_insert_stage, 2, name, -1, SQLITE_STATIC);
            sqlite3_step(stmt_insert_stage);

            sqlite3_reset(stmt_insert_stage);
        }
    }

    {
        json_object* base_statuses = json_object_object_get(statuses, "base");
        json_object_object_foreach(base_statuses, status_str, enum_name)
        {
            int status = atoi(status_str);
            const char* name = json_object_get_string(enum_name);
            printf("INSERT INTO status_enums (value, name) VALUES (%d, '%s');\n", status, name);

            sqlite3_bind_null(stmt_insert_status_enum, 1);
            sqlite3_bind_int(stmt_insert_status_enum, 2, status);
            sqlite3_bind_text(stmt_insert_status_enum, 3, name, -1, SQLITE_STATIC);
            sqlite3_step(stmt_insert_status_enum);

            sqlite3_reset(stmt_insert_status_enum);
        }
    }

    {
        json_object* specific_statuses = json_object_object_get(statuses, "specific");
        json_object_object_foreach(specific_statuses, fighter_id_str, fighter)
        {
            int fighter_id = atoi(fighter_id_str);
            json_object_object_foreach(fighter, status_str, enum_name)
            {
                int status = atoi(status_str);
                const char* name = json_object_get_string(enum_name);
                printf("INSERT INTO status_enums (fighter_id, value, name) VALUES (%d, %d, '%s');\n", fighter_id, status, name);

                sqlite3_bind_int(stmt_insert_status_enum, 1, fighter_id);
                sqlite3_bind_int(stmt_insert_status_enum, 2, status);
                sqlite3_bind_text(stmt_insert_status_enum, 3, name, -1, SQLITE_STATIC);
                sqlite3_step(stmt_insert_status_enum);

                sqlite3_reset(stmt_insert_status_enum);
            }
        }
    }

    {
        json_object_object_foreach(hit_statuses, hit_status_str, hit_status_name)
        {
            int hit_status_id = atoi(hit_status_str);
            const char* name = json_object_get_string(hit_status_name);
            printf("INSERT INTO hit_status_enums (id, name) VALUES (%d, '%s');\n", hit_status_id, name);

            sqlite3_bind_int(stmt_insert_hit_status_enum, 1, hit_status_id);
            sqlite3_bind_text(stmt_insert_hit_status_enum, 2, name, -1, SQLITE_STATIC);
            sqlite3_step(stmt_insert_hit_status_enum);

            sqlite3_reset(stmt_insert_hit_status_enum);
        }
    }

    printf("COMMIT TRANSACTION;\n");
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &error_message);

    sqlite3_finalize(stmt_insert_hit_status_enum);
    sqlite3_finalize(stmt_insert_status_enum);
    sqlite3_finalize(stmt_insert_stage);
    sqlite3_finalize(stmt_insert_fighter);

    json_object_put(root);
}

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }
static void insert_hash40(sqlite3* db)
{
    sqlite3_stmt* stmt_insert;
    sqlite3_prepare_v2(db, "INSERT INTO motions (hash40, string) VALUES (?, ?);", -1, &stmt_insert, NULL);

    char* error_message;
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &error_message);
    printf("BEGIN TRANSACTION;\n");

    struct rf_mfile mf;
    rf_mfile_map(&mf, "ParamLabels.csv");

    struct rf_mstream ms = rf_mstream_from_mfile(&mf);

    while (!rf_mstream_at_end(&ms))
    {
        /* Extract hash40 and label, splitting on comma */
        struct rf_str h40_str, label;
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

        sqlite3_bind_int64(stmt_insert, 1, h40);
        sqlite3_bind_text(stmt_insert, 2, label.data, label.len, SQLITE_STATIC);
        sqlite3_step(stmt_insert);

        sqlite3_reset(stmt_insert);
    }

    rf_mfile_unmap(&mf);

    printf("COMMIT TRANSACTION;\n");
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &error_message);

    sqlite3_finalize(stmt_insert);
}

int import_rfr_metadata_1_7_into_db(sqlite3* db, struct json_object* root)
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
            /*
             * The "tournaments" table holds a list of all existing tournaments.
             * We want to create a new entry if this tournament is not yet
             * recorded, otherwise we want to get the existing id.
             */
            sqlite3_stmt* stmt;
            ret = sqlite3_prepare_v2(db,
                "INSERT INTO tournaments (name, website) VALUES (?, ?) ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;", -1, &stmt, NULL);
            if (ret != SQLITE_OK)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                return -1;
            }

            if ((ret = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC) != SQLITE_OK) ||
                (ret = sqlite3_bind_text(stmt, 2, website, -1, SQLITE_STATIC) != SQLITE_OK))
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }
            tournament_id = sqlite3_column_int(stmt, 0);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            sqlite3_finalize(stmt);
        }
    }

    struct json_object* tournament_sponsors = json_object_object_get(tournament, "sponsors");
    for (int i = 0; i != json_object_array_length(tournament_sponsors); ++i)
    {
        int sponsor_id = -1;
        struct json_object* sponsor = json_object_array_get_idx(tournament_sponsors, i);
        const char* name = json_object_get_string(json_object_object_get(sponsor, "name"));
        const char* website = json_object_get_string(json_object_object_get(sponsor, "website"));
        if (name && website && *name)
        {
            sqlite3_stmt* stmt;
            ret = sqlite3_prepare_v2(db,
                "INSERT INTO sponsors (short, name, website) VALUES ('', ?, ?) ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;", -1, &stmt, NULL);
            if (ret != SQLITE_OK)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                return -1;
            }

            if ((ret = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC) != SQLITE_OK) ||
                (ret = sqlite3_bind_text(stmt, 2, website, -1, SQLITE_STATIC) != SQLITE_OK))
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            if (sqlite3_step(stmt) != SQLITE_ROW)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }
            sponsor_id = sqlite3_column_int(stmt, 0);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            sqlite3_finalize(stmt);
        }

        if (sponsor_id != -1)
        {
            /*
             * The "tournament_sponsors" table forms a many-to-many relation
             * between tournaments and their sponsors. Make sure each sponsor
             * is associated with the current tournament_id.
             */
            sqlite3_stmt* stmt;
            ret = sqlite3_prepare_v2(db,
                "INSERT OR IGNORE INTO tournament_sponsors (tournament_id, sponsor_id) VALUES (?, ?);", -1, &stmt, NULL);
            if (ret != SQLITE_OK)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                return -1;
            }

            if ((ret = sqlite3_bind_int(stmt, 1, tournament_id) != SQLITE_OK) ||
                (ret = sqlite3_bind_int(stmt, 2, sponsor_id) != SQLITE_OK))
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                rf_log_sqlite_err(ret, sqlite3_errstr(ret));
                sqlite3_finalize(stmt);
                return -1;
            }

            sqlite3_finalize(stmt);
        }
    }

    struct json_object* player_info = json_object_object_get(root, "playerinfo");
    struct json_object* game_info = json_object_object_get(root, "gameinfo");

    struct json_object* time_started = json_object_object_get(game_info, "timestampstart");
    struct json_object* time_ended = json_object_object_get(game_info, "timestampend");
    struct json_object* stage_id = json_object_object_get(game_info, "stageid");
    struct json_object* winner = json_object_object_get(game_info, "winner");

    return 0;
}

int import_rfr_metadata_into_db(sqlite3* db, struct rf_mstream* ms)
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

    printf("version: %s\n", version_str);
    if (strcmp(version_str, "1.5") == 0)
    {}
    else if (strcmp(version_str, "1.6") == 0)
    {}
    else if (strcmp(version_str, "1.7") == 0)
    {
        if (import_rfr_metadata_1_7_into_db(db, root) != 0)
            goto fail;
    }
    else
        goto fail;

    json_object_put(root);
    return 0;

    fail         : json_object_put(root);
    parse_failed : return -1;
}

int import_rfr_framedata_1_5_into_db(sqlite3* db, struct rf_mstream* ms)
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

int import_rfr_framedata_into_db(sqlite3* db, struct rf_mstream* ms)
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
        int result = import_rfr_framedata_1_5_into_db(db, &uncompressed_stream);
        rf_free(uncompressed_data);
        return result;
    }

    return -1;
}

int import_rfr_video_metadata_into_db(sqlite3* db, struct rf_mstream* ms)
{
    return 0;
}

int import_rfr_into_db(sqlite3* db, const char* file_name)
{
    struct rf_mfile mf;
    if (rf_mfile_map(&mf, file_name) != 0)
        goto mmap_failed;

    struct rf_mstream ms = rf_mstream_from_mfile(&mf);
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
            if (import_rfr_metadata_into_db(db, &blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "FDAT", 4) == 0)
        {
            if (import_rfr_framedata_into_db(db, &blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "VIDM", 4) == 0)
        {
            if (import_rfr_video_metadata_into_db(db, &blob) != 0)
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
    sqlite3* db;
    int result = sqlite3_open_v2("rf.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (result != SQLITE_OK)
    {
        rf_log_sqlite_err(result, sqlite3_errstr(result));
        goto open_db_failed;
    }

    sqlite3_stmt* stmt_version;
    sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt_version, NULL);

    if (sqlite3_step(stmt_version) == SQLITE_ROW)
        fprintf(stderr, "db version: %s\n", sqlite3_column_text(stmt_version, 0));

    sqlite3_finalize(stmt_version);

    /*insert_mapping_info(db);
    insert_hash40(db);*/
    import_rfr_into_db(db, "/home/thecomet/videos/ssbu/2023-09-20 - SBZ Bi-Weekly/reframed/2023-09-20_19-09-51 - Singles Bracket - Bo3 (Pools 1) - TheComet (Pikachu) vs Aff (Donkey Kong) - Game 1 (0-0) - Hollow Bastion.rfr");

    sqlite3_close(db);
    return 0;

open_db_failed: return -1;
}
