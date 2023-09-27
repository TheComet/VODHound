#include "sqlite/sqlite3.h"
#include "json-c/json.h"

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

static void insert_hash40(sqlite3* db)
{
    sqlite3_stmt* stmt_insert;
    sqlite3_prepare_v2(db, "INSERT INTO motions (hash40, string) VALUES (?, ?);", -1, &stmt_insert, NULL);

    char* error_message;
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &error_message);
    printf("BEGIN TRANSACTION;\n");



    printf("COMMIT TRANSACTION;\n");
    sqlite3_exec(db, "COMMIT TRANSACTION", NULL, NULL, &error_message);

    sqlite3_finalize(stmt_insert);
}

#include "rf/str.h"

struct rf_person
{
    struct rf_str tag;
    struct rf_str name;
    struct rf_str sponsor;
    struct rf_str social;
    struct rf_str pronouns;
    char is_loser_side;
};

void rf_person_init(struct rf_person* p,
    struct rf_str tag,
    struct rf_str name,
    struct rf_str sponsor,
    struct rf_str social,
    struct rf_str pronouns,
    char is_loser_side)
{
    struct arena a = string_arena_alloc(
        tag.len + name.len + sponsor.len + social.len + pronouns.len);
    p->tag = string_dup(&a, tag);
    p->name = string_dup(&a, name);
    p->sponsor = string_dup(&a, sponsor);
    p->social = string_dup(&a, social);
    p->pronouns = string_dup(&a, pronouns);

    p->is_loser_side = is_loser_side;
}

void rf_person_deinit(struct rf_person* p)
{
    free(p->tag.begin);
}

struct rf_metadata
{
    uint64_t time_started;
    uint64_t time_ended;
    uint8_t fighter_ids[8];
    uint8_t costumes[8];

    uint16_t stage_id;
};

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }

int import_rfr_metadata_into_db(struct mstream* ms)
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
    {}
    else
        goto fail;

    json_object_put(root);
    return 0;

    fail         : json_object_put(root);
    parse_failed : return -1;
}

int import_rfr_framedata_1_5_into_db(struct mstream* ms)
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

            if (mstream_past_end(ms))
                return -1;

            //printf("frame: %d, x: %f, y: %f, stocks: %d\n", frames_left, posx, posy, stocks);
        }

    if (!mstream_at_end(ms))
        return -1;

    return 0;
}

int import_rfr_framedata_into_db(struct mstream* ms)
{
    uint8_t major = mstream_read_u8(ms);
    uint8_t minor = mstream_read_u8(ms);
    if (major == 1 && minor == 5)
    {
        uLongf uncompressed_size = mstream_read_lu32(ms);
        if (uncompressed_size == 0 || uncompressed_size > 128*1024*1024)
            return -1;

        void* uncompressed_data = malloc(uncompressed_size);
        if (uncompress(
            (uint8_t*)uncompressed_data, &uncompressed_size,
            (const uint8_t*)mstream_ptr(ms), mstream_bytes_left(ms)) != Z_OK)
        {
            free(uncompressed_data);
            return -1;
        }

        struct mstream uncompressed_stream = mstream_from_memory(
                uncompressed_data, uncompressed_size);
        int result = import_rfr_framedata_1_5_into_db(&uncompressed_stream);
        free(uncompressed_data);
        return result;
    }

    return -1;
}

int import_rfr_video_metadata_into_db(struct mstream* ms)
{
    return 0;
}

int import_rfr_into_db(const char* file_name)
{
    struct mfile mf;
    if (map_file(&mf, file_name) != 0)
        goto mmap_failed;

    struct mstream ms = mstream_from_mfile(mf);
    if (memcmp(mstream_read(&ms, 4), "RFR1", 4) != 0)
    {
        puts("File has invalid header");
        goto invalid_header;
    }

    uint8_t num_entries = mstream_read_u8(&ms);
    for (int i = 0; i != num_entries; ++i)
    {
        const void* type = mstream_read(&ms, 4);
        int offset = mstream_read_lu32(&ms);
        int size = mstream_read_lu32(&ms);
        struct mstream blob = mstream_from_mstream(&ms, offset, size);

        if (memcmp(type, "META", 4) == 0)
        {
            if (import_rfr_metadata_into_db(&blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "FDAT", 4) == 0)
        {
            if (import_rfr_framedata_into_db(&blob) != 0)
                goto fail;
        }
        else if (memcmp(type, "VIDM", 4) == 0)
        {
            if (import_rfr_video_metadata_into_db(&blob) != 0)
                goto fail;
        }
    }

    unmap_file(&mf);
    return 0;

    fail           :
    invalid_header : unmap_file(&mf);
    mmap_failed    : return -1;
}

int main(int argc, char** argv)
{
    sqlite3* db;
    sqlite3_open_v2("rf.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

    sqlite3_stmt* stmt_version;
    sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &stmt_version, NULL);

    if (sqlite3_step(stmt_version) == SQLITE_ROW)
        fprintf(stderr, "db version: %s\n", sqlite3_column_text(stmt_version, 0));

    sqlite3_finalize(stmt_version);

    insert_mapping_info(db);
    insert_hash40(db);

    struct mfile mf;
    map_file(&mf, "ParamLabels.csv");

    struct mstream ms = mstream_from_mfile(mf);

    while (!mstream_at_end(&ms))
    {
        /* Extract hash40 and label, splitting on comma */
        struct rf_str h40_str, label;
        if (mstream_read_string_until_delim(&ms, ',', &h40_str) != 0)
            break;
        if (mstream_read_string_until_condition(&ms, newline_or_end, &label) != 0)
            break;

        /* Convert hash40 into value */
        uint64_t h40;
        if (string_hex_to_u64(h40_str, &h40) != 0)
            continue;

        if (h40 == 0 || label.len == 0)
            continue;

/*
        char buf[256];
        memcpy(buf, label.begin, label.len);
        buf[label.len] = '\0';
        printf("0x%lx: %s\n", h40, buf);*/
    }

    unmap_file(&mf);

    import_rfr_into_db("games/2022-08-24 - Coaching (1) - TheComet (Pikachu) vs Mitch (Mythra) Game 1.rfr");

    sqlite3_close(db);
    return 0;
}
