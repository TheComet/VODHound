#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/mfile.h"
#include "vh/vec.h"

#include "sqlite/sqlite3.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if !defined(min)
#   define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define STMT_LIST                           \
    X(motion, add)                          \
    X(motion, exists)                       \
                                            \
    X(motion_label, add_or_get_group)       \
    X(motion_label, add_or_get_layer)       \
    X(motion_label, add_or_get_category)    \
    X(motion_label, add_or_get_usage)       \
    X(motion_label, add_or_get_label)       \
    X(motion_label, to_motions)             \
    X(motion_label, to_notation_label)      \
                                            \
    X(fighter, add)                         \
    X(fighter, get_name)                    \
    X(stage, add)                           \
    X(status_enum, add)                     \
    X(hit_status_enum, add)                 \
                                            \
    X(tournament, add_or_get)               \
    X(tournament, add_sponsor)              \
    X(tournament, add_organizer)            \
    X(tournament, add_commentator)          \
                                            \
    X(event, add_or_get_type)               \
    X(event, add_or_get)                    \
                                            \
    X(round, add_or_get_type)               \
                                            \
    X(set_format, add_or_get)               \
                                            \
    X(team, add_or_get)                     \
    X(team, add_member)                     \
                                            \
    X(sponsor, add_or_get)                  \
                                            \
    X(person, add_or_get)                   \
    X(person, get_id_from_name)             \
    X(person, get_team_id_from_name)        \
    X(person, set_tag)                      \
    X(person, set_social)                   \
    X(person, set_pronouns)                 \
                                            \
    X(game, add_or_get)                     \
    X(game, get_all)                        \
    X(game, get_events)                     \
    X(game, get_all_in_event)               \
    X(game, associate_tournament)           \
    X(game, associate_event)                \
    X(game, associate_video)                \
    X(game, unassociate_video)              \
    X(game, get_videos)                     \
    X(game, add_player)                     \
                                            \
    X(group, add_or_get)                    \
    X(group, add_game)                      \
                                            \
    X(video, add_or_get)                    \
    X(video, set_path_hint)                 \
    X(video, add_path)                      \
    X(video, query_paths)                   \
                                            \
    X(score, add)                           \
                                            \
    X(switch_info, add)                     \
                                            \
    X(stream_recording_sources, add)

#define STMT_PREPARE_OR_RESET(stmt, error_return, text)                       \
    if (ctx->stmt)                                                            \
        sqlite3_reset(ctx->stmt);                                             \
    else if (prepare_stmt_wrapper(ctx->db, &ctx->stmt, cstr_view(text)) != 0) \
        return error_return

struct db
{
    sqlite3* db;

#define X(group, stmt) sqlite3_stmt* group##_##stmt;
    STMT_LIST
#undef X
    sqlite3_stmt* motion_layer_add_or_get_layer_priority;
};

static int
exec_sql_wrapper(sqlite3* db, const char* sql)
{
    char* error_message;
    int ret = sqlite3_exec(db, sql, NULL, NULL, &error_message);
    if (ret != SQLITE_OK)
    {
        log_sqlite_err(ret, error_message, sqlite3_errmsg(db));
        sqlite3_free(error_message);
        return -1;
    }
    return 0;
}

static int
prepare_stmt_wrapper(sqlite3* db, sqlite3_stmt** stmt, struct str_view sql)
{
    int ret = sqlite3_prepare_v2(db, sql.data, sql.len, stmt, NULL);
    if (ret != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

static int
step_stmt_wrapper(sqlite3* db, sqlite3_stmt* stmt)
{
    int ret = sqlite3_step(stmt);
    sqlite3_reset(stmt);
    if (ret != SQLITE_DONE)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}

static int
run_migration_script(sqlite3* db, const char* file_name)
{
    int ret;
    struct mfile mf;
    sqlite3_stmt* stmt;
    const char* sql;
    const char* sql_next;
    int sql_len;

    if (mfile_map(&mf, file_name) != 0)
        goto open_script_failed;

    sql = mf.address;
    sql_len = mf.size;

    log_info("Running migration script '%s'\n", file_name);

next_step:
    ret = sqlite3_prepare_v2(db, sql, sql_len, &stmt, &sql_next);
    if (ret != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        goto prepare_failed;
    }
retry_step:
    switch (ret = sqlite3_step(stmt))
    {
        case SQLITE_BUSY:
            goto retry_step;
        case SQLITE_ROW:
        case SQLITE_DONE:
            sql_len -= (int)(sql_next - sql);
            sql = sql_next;
            for (; sql_len && isspace(*sql); ++sql, --sql_len) {}
            if (sql_len <= 0)
                break;
            sqlite3_finalize(stmt);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
            goto exec_failed;
    }

    sqlite3_finalize(stmt);
    mfile_unmap(&mf);
    return 0;

    exec_failed        : sqlite3_finalize(stmt);
    prepare_failed     : mfile_unmap(&mf);
    open_script_failed : return -1;
}

static int
check_version_and_migrate(sqlite3* db, int reinit_db)
{
    int ret;
    int version;
    sqlite3_stmt* stmt;

    ret = sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, NULL);
    if (ret != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    log_dbg("db version: %d\n", version);

    if (exec_sql_wrapper(db, "BEGIN TRANSACTION") != 0)
        return -1;

    if (reinit_db)
    {
        log_note("Downgrading db to version 0\n");
        switch (version)
        {
            case 1: if (run_migration_script(db, "migrations/1-schema.down.sql") != 0) goto migrate_failed;
            case 0: break;
        }
        version = 0;
    }

    switch (version)
    {
        case 0: if (run_migration_script(db, "migrations/1-schema.up.sql") != 0) goto migrate_failed;
        /*case 1: if (upgrade_to_v2(db) != 0) goto migrate_failed;
        case 2: if (upgrade_to_v3(db) != 0) goto migrate_failed;*/
        case 1: break;

        default:
            log_err("Unknown database version %d. Aborting all operations\n", version);
            goto migrate_failed;
    }

    if (exec_sql_wrapper(db, "PRAGMA user_version=1") != 0)
        goto migrate_failed;

    if (exec_sql_wrapper(db, "COMMIT TRANSACTION") != 0)
        goto migrate_failed;

    log_note("Successfully migrated from version %d to version 1\n", version);

    return 0;

    migrate_failed : exec_sql_wrapper(db, "ROLLBACK TRANSACTION");
    return -1;
}

static struct db*
open_and_prepare(const char* uri, int reinit_db)
{
    int ret;
    struct db* ctx = mem_alloc(sizeof *ctx);
    if (ctx == NULL)
        goto oom;
    memset(ctx, 0, sizeof *ctx);

    ret = sqlite3_open_v2(uri, &ctx->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        goto open_db_failed;
    }

    if (check_version_and_migrate(ctx->db, reinit_db) != 0)
        goto migrate_db_failed;

    return ctx;

    migrate_db_failed             : sqlite3_close(ctx->db);
    open_db_failed                : mem_free(ctx);
    oom                           : return NULL;
}

static void
close_db(struct db* ctx)
{
    sqlite3_finalize(ctx->motion_layer_add_or_get_layer_priority);
#define X(group, stmt) sqlite3_finalize(ctx->group##_##stmt);
    STMT_LIST
#undef X

    sqlite3_close(ctx->db);
    mem_free(ctx);
}

static int
transaction_begin(struct db* ctx)
{
    return exec_sql_wrapper(ctx->db, "BEGIN TRANSACTION");
}

static int
transaction_commit(struct db* ctx)
{
    return exec_sql_wrapper(ctx->db, "COMMIT TRANSACTION");
}

static int
transaction_rollback(struct db* ctx)
{
    return exec_sql_wrapper(ctx->db, "ROLLBACK TRANSACTION");
}

static int
transaction_begin_nested(struct db* ctx, struct str_view name)
{
    char buf[64] = "SAVEPOINT ";
    strncat(buf, name.data, min((size_t)name.len, 64 - sizeof("SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_commit_nested(struct db* ctx, struct str_view name)
{
    char buf[64] = "RELEASE SAVEPOINT ";
    strncat(buf, name.data, min((size_t)name.len, 64 - sizeof("RELEASE SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_rollback_nested(struct db* ctx, struct str_view name)
{
    char buf[64] = "ROLLBACK TO SAVEPOINT ";
    strncat(buf, name.data, min((size_t)name.len, 64 - sizeof("ROLLBACK TO SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
motion_add(struct db* ctx, uint64_t hash40, struct str_view string)
{
    int ret;
    if (ctx->motion_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_add, cstr_view(
            "INSERT OR IGNORE INTO motions (hash40, string) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_add, 1, (int64_t)hash40)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->motion_add, 2, string.data, string.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->motion_add);
}

static int
motion_exists(struct db* ctx, uint64_t hash40)
{
    int ret;
    if (ctx->motion_exists == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_exists, cstr_view(
            "SELECT 1 FROM motions WHERE hash40=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_exists, 1, (int64_t)hash40)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->motion_exists);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_ROW:
            sqlite3_reset(ctx->motion_exists);
            return 1;
        case SQLITE_DONE:
            sqlite3_reset(ctx->motion_exists);
            return 0;
    }

    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->motion_exists);
    return -1;
}

static int
motion_label_add_or_get_group(struct db* ctx, struct str_view name)
{
    int ret, group_id = -1;
    if (ctx->motion_label_add_or_get_group == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_add_or_get_group, cstr_view(
            "INSERT INTO motion_groups (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->motion_label_add_or_get_group, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->motion_label_add_or_get_group);
    switch (ret)
    {
        case SQLITE_ROW:
            group_id = sqlite3_column_int(ctx->motion_label_add_or_get_group, 0);
            goto done;
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->motion_label_add_or_get_group);
    return group_id;
}

static int
motion_label_add_or_get_layer(struct db* ctx, int group_id, struct str_view name)
{
    int ret, layer_id = -1, priority = -1;
    if (ctx->motion_layer_add_or_get_layer_priority == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_layer_add_or_get_layer_priority, cstr_view(
            "SELECT priority FROM motion_layers "
            "JOIN motion_groups ON motion_groups.id = motion_layers.group_id "
            "ORDER BY priority ASC LIMIT 1;")) != 0)
            return -1;

next_step_priority:
    ret = sqlite3_step(ctx->motion_layer_add_or_get_layer_priority);
    switch (ret)
    {
        case SQLITE_ROW:
            priority = sqlite3_column_int(ctx->motion_layer_add_or_get_layer_priority, 0);
            break;
        case SQLITE_BUSY: goto next_step_priority;
        case SQLITE_DONE:
            priority = 0;
            break;
    }
    sqlite3_reset(ctx->motion_layer_add_or_get_layer_priority);
    if (priority < 0)
        goto error;
    priority++;

    if (ctx->motion_label_add_or_get_layer == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_add_or_get_layer, cstr_view(
            "INSERT INTO motion_layers (group_id, priority, name) VALUES (?, ?, ?) "
            "ON CONFLICT DO UPDATE SET group_id=excluded.group_id RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->motion_label_add_or_get_layer, 1, group_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_add_or_get_layer, 2, priority)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->motion_label_add_or_get_layer, 3, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->motion_label_add_or_get_layer);
    switch (ret)
    {
        case SQLITE_ROW:
            layer_id = sqlite3_column_int(ctx->motion_label_add_or_get_layer, 0);
            goto done;
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->motion_label_add_or_get_layer);
    return layer_id;
}

static int
motion_label_add_or_get_category(struct db* ctx, struct str_view name)
{
    int ret, category_id = -1;
    if (ctx->motion_label_add_or_get_category == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_add_or_get_category, cstr_view(
            "INSERT INTO motion_categories (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->motion_label_add_or_get_category, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->motion_label_add_or_get_category);
    switch (ret)
    {
        case SQLITE_ROW:
            category_id = sqlite3_column_int(ctx->motion_label_add_or_get_category, 0);
            goto done;
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->motion_label_add_or_get_category);
    return category_id;
}

static int
motion_label_add_or_get_usage(struct db* ctx, struct str_view name)
{
    int ret, usage_id = -1;
    if (ctx->motion_label_add_or_get_usage == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_add_or_get_usage, cstr_view(
            "INSERT INTO motion_usages (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->motion_label_add_or_get_usage, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->motion_label_add_or_get_usage);
    switch (ret)
    {
        case SQLITE_ROW:
            usage_id = sqlite3_column_int(ctx->motion_label_add_or_get_usage, 0);
            goto done;
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->motion_label_add_or_get_usage);
    return usage_id;
}

static int
motion_label_add_or_get_label(struct db* ctx, uint64_t motion, int fighter_id, int layer_id, int category_id, int usage_id, struct str_view name)
{
    int ret, label_id = -1;
    if (ctx->motion_label_add_or_get_label == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_add_or_get_label, cstr_view(
            "INSERT INTO motion_labels (hash40, fighter_id, layer_id, category_id, usage_id, label) VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET group_id=excluded.group_id RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_label_add_or_get_label, 1, (int64_t)motion)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_add_or_get_label, 2, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_add_or_get_label, 3, layer_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_add_or_get_label, 4, category_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_add_or_get_label, 5, usage_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->motion_label_add_or_get_label, 6, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->motion_label_add_or_get_label);
    switch (ret)
    {
        case SQLITE_ROW:
            label_id = sqlite3_column_int(ctx->motion_label_add_or_get_label, 0);
            goto done;
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->motion_label_add_or_get_label);
    return label_id;
}

static int
motion_label_to_motions(struct db* ctx, int fighter_id, struct str_view label, struct vec* motions_out)
{
    int ret;
    if (ctx->motion_label_to_motions == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_to_motions, cstr_view(
            "SELECT hash40 FROM motion_labels "
            "WHERE fighter_id=? AND label=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->motion_label_to_motions, 1, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->motion_label_to_motions, 2, label.data, label.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->motion_label_to_motions);
    switch (ret)
    {
        case SQLITE_ROW : {
            uint64_t motion = (uint64_t)sqlite3_column_int64(ctx->motion_label_to_motions, 0);
            if (vec_push(motions_out, &motion) < 0)
                goto error;
        }
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->motion_label_to_motions);
            if (vec_count(motions_out) > 0)
                return 1;
            return 0;
    }
error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->motion_label_to_motions);
    return -1;
}

static int
motion_label_to_notation_label(struct db* ctx, int fighter_id, uint64_t motion, struct str* label)
{
    int ret;
    if (ctx->motion_label_to_notation_label == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_label_to_notation_label, cstr_view(
            "SELECT label FROM motion_labels "
            "JOIN motion_layers ON motion_layers.id=motion_labels.layer_id "
            "WHERE hash40=? AND fighter_id=? AND usage_id=? AND label <> '' "
            "ORDER BY priority ASC "
            "LIMIT 1;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_label_to_notation_label, 1, (int64_t)motion)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_to_notation_label, 2, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->motion_label_to_notation_label, 3, 1 /* XXX: hard coded for now */)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->motion_label_to_notation_label);
    switch (ret)
    {
        case SQLITE_ROW  :
            if (str_set(label, cstr_view((const char*)sqlite3_column_text(ctx->motion_label_to_notation_label, 0))) < 0)
            {
                sqlite3_reset(ctx->motion_label_to_notation_label);
                return -1;
            }
            sqlite3_reset(ctx->motion_label_to_notation_label);
            return 1;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->motion_label_to_notation_label);
            return 0;
    }

    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->motion_label_to_notation_label);
    return -1;
}

static int
fighter_add(struct db* ctx, int fighter_id, struct str_view name)
{
    int ret;
    if (ctx->fighter_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->fighter_add, cstr_view(
            "INSERT OR IGNORE INTO fighters (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->fighter_add, 1, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->fighter_add, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->fighter_add);
}

static int
fighter_get_name(struct db* ctx, int fighter_id, struct str* name)
{
    int ret;
    str_clear(name);
    if (ctx->fighter_get_name == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->fighter_get_name, cstr_view(
            "SELECT name FROM fighters WHERE id=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->fighter_get_name, 1, fighter_id)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->fighter_get_name);
    switch (ret)
    {
        case SQLITE_ROW  :
            if (str_set(name, cstr_view((const char*)sqlite3_column_text(ctx->fighter_get_name, 0))) < 0)
            {
                sqlite3_reset(ctx->fighter_get_name);
                return -1;
            }
            sqlite3_reset(ctx->fighter_get_name);
            return 1;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->fighter_get_name);
            return 0;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->fighter_get_name);
    return -1;
}

static int
stage_add(struct db* ctx, int stage_id, struct str_view name)
{
    int ret;
    if (ctx->stage_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->stage_add, cstr_view(
            "INSERT OR IGNORE INTO stages (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->stage_add, 1, stage_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->stage_add, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->stage_add);
}

static int
status_enum_add(struct db* ctx, int fighter_id, int status_id, struct str_view name)
{
    int ret;
    if (ctx->status_enum_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->status_enum_add, cstr_view(
            "INSERT OR IGNORE INTO status_enums (fighter_id, value, name) VALUES (?, ?, ?);")) != 0)
            return -1;

    if (fighter_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->status_enum_add, 1)) != SQLITE_OK)
            goto error;
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->status_enum_add, 1, fighter_id)) != SQLITE_OK)
            goto error;
    }

    if ((ret = sqlite3_bind_int(ctx->status_enum_add, 2, status_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->status_enum_add, 3, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

    return step_stmt_wrapper(ctx->db, ctx->status_enum_add);

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    return -1;
}

static int
hit_status_enum_add(struct db* ctx, int id, struct str_view name)
{
    int ret;
    if (ctx->hit_status_enum_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->hit_status_enum_add, cstr_view(
            "INSERT OR IGNORE INTO hit_status_enums (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->hit_status_enum_add, 1, id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->hit_status_enum_add, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->hit_status_enum_add);
}

static int
tournament_add_or_get(struct db* ctx, struct str_view name, struct str_view website)
{
    int ret, tournament_id = -1;
    if (ctx->tournament_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_or_get, cstr_view(
            "INSERT INTO tournaments (name, website) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->tournament_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->tournament_add_or_get, 2, website.data, website.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->tournament_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            tournament_id = sqlite3_column_int(ctx->tournament_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->tournament_add_or_get);
    return tournament_id;
}

static int
tournament_add_sponsor(struct db* ctx, int tournament_id, int sponsor_id)
{
    int ret;
    if (ctx->tournament_add_sponsor == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_sponsor, cstr_view(
            "INSERT OR IGNORE INTO tournament_sponsors (tournament_id, sponsor_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_add_sponsor, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_add_sponsor, 2, sponsor_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_add_sponsor);
}

static int
tournament_add_organizer(struct db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_add_organizer == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_organizer, cstr_view(
            "INSERT OR IGNORE INTO tournament_organizers (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_add_organizer, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_add_organizer, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_add_organizer);
}

static int
tournament_add_commentator(struct db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_add_commentator == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_commentator, cstr_view(
            "INSERT OR IGNORE INTO tournament_commentators (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_add_commentator, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_add_commentator, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_add_commentator);
}

static int
event_add_or_get_type(struct db* ctx, struct str_view name)
{
    int ret, event_type_id = -1;
    if (ctx->event_add_or_get_type == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->event_add_or_get_type, cstr_view(
            "INSERT INTO event_types (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->event_add_or_get_type, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->event_add_or_get_type);
    switch (ret)
    {
        case SQLITE_ROW  :
            event_type_id = sqlite3_column_int(ctx->event_add_or_get_type, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->event_add_or_get_type);
    return event_type_id;
}

static int
event_add_or_get(struct db* ctx, int event_type_id, struct str_view url)
{
    int ret, event_id = -1;
    if (ctx->event_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->event_add_or_get, cstr_view(
            "INSERT INTO events (event_type_id, url) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET event_type_id=excluded.event_type_id RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->event_add_or_get, 1, event_type_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->event_add_or_get, 2, url.data, url.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->event_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            event_id = sqlite3_column_int(ctx->event_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->event_add_or_get);
    return event_id;
}

static int
round_add_or_get_type(struct db* ctx, struct str_view short_name, struct str_view long_name)
{
    int ret, round_type_id = -1;
    if (ctx->round_add_or_get_type == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->round_add_or_get_type, cstr_view(
            "INSERT INTO round_types (short_name, long_name) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->round_add_or_get_type, 1, short_name.data, short_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->round_add_or_get_type, 2, long_name.data, long_name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->round_add_or_get_type);
    switch (ret)
    {
        case SQLITE_ROW  :
            round_type_id = sqlite3_column_int(ctx->round_add_or_get_type, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->round_add_or_get_type);
    return round_type_id;
}

static int
set_format_add_or_get(struct db* ctx, struct str_view short_name, struct str_view long_name)
{
    int ret, set_format_id = -1;
    if (ctx->set_format_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->set_format_add_or_get, cstr_view(
            "INSERT INTO set_formats (short_name, long_name) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->set_format_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->set_format_add_or_get, 2, long_name.data, long_name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->set_format_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            set_format_id = sqlite3_column_int(ctx->set_format_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->set_format_add_or_get);
    return set_format_id;
}

static int
team_add_or_get(struct db* ctx, struct str_view name, struct str_view url)
{
    int ret, team_id = -1;
    if (ctx->team_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->team_add_or_get, cstr_view(
            "INSERT INTO teams (name, url) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->team_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->team_add_or_get, 2, url.data, url.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->team_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            team_id = sqlite3_column_int(ctx->team_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->team_add_or_get);
    return team_id;
}

static int
team_add_member(struct db* ctx, int team_id, int person_id)
{
    int ret;
    if (ctx->team_add_member == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->team_add_member, cstr_view(
            "INSERT OR IGNORE INTO team_members (team_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->team_add_member, 1, team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->team_add_member, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->team_add_member);
}

static int
sponsor_add_or_get(struct db* ctx, struct str_view short_name, struct str_view full_name, struct str_view website)
{
    int ret, sponsor_id = -1;
    if (ctx->sponsor_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->sponsor_add_or_get, cstr_view(
            "INSERT INTO sponsors (short_name, full_name, website) VALUES (?, ?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 2, full_name.data, full_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 3, website.data, website.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->sponsor_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            sponsor_id = sqlite3_column_int(ctx->sponsor_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->sponsor_add_or_get);
    return sponsor_id;
}

static int
person_add_or_get(
    struct db* ctx,
    int sponsor_id, struct str_view name, struct str_view tag, struct str_view social, struct str_view pronouns,
    int (*on_person)(
        int id, int sponsor_id, const char* name, const char* tag, const char* social, const char* pronouns,
        void* user),
    void* user)
{
    int ret, person_id = -1;
    if (ctx->person_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_add_or_get, cstr_view(
            "INSERT INTO people (sponsor_id, name, tag, social, pronouns) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id, sponsor_id, name, tag, social, pronouns;")) != 0)
            return -1;

    if (sponsor_id < 0)
    {
        if ((ret = sqlite3_bind_null(ctx->person_add_or_get, 1)) != SQLITE_OK)
            goto error;
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->person_add_or_get, 1, sponsor_id)) != SQLITE_OK)
            goto error;
    }

    if ((ret = sqlite3_bind_text(ctx->person_add_or_get, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 3, tag.data, tag.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 4, social.data, social.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 5, pronouns.data, pronouns.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->person_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            person_id = sqlite3_column_int(ctx->person_add_or_get, 0);
            ret = on_person(
                sqlite3_column_int(ctx->person_add_or_get, 0),
                sqlite3_column_int(ctx->person_add_or_get, 1),
                (const char*)sqlite3_column_text(ctx->person_add_or_get, 2),
                (const char*)sqlite3_column_text(ctx->person_add_or_get, 3),
                (const char*)sqlite3_column_text(ctx->person_add_or_get, 4),
                (const char*)sqlite3_column_text(ctx->person_add_or_get, 5),
                user);
            if (ret < 0)
                person_id = -1;
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->person_add_or_get);
    return person_id;
}

static int
person_get_id_from_name(struct db* ctx, struct str_view name)
{
    int ret, person_id = -1;
    if (ctx->person_get_id_from_name == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_get_id_from_name, cstr_view(
            "SELECT id FROM people WHERE name=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_get_id_from_name, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->person_get_id_from_name);
    switch (ret)
    {
        case SQLITE_ROW  :
            person_id = sqlite3_column_int(ctx->person_get_id_from_name, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->person_get_id_from_name);
    return person_id;
}

static int
person_get_team_id_from_name(struct db* ctx, struct str_view name)
{
    int ret, team_id = -11;
    if (ctx->person_get_team_id_from_name == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_get_team_id_from_name, cstr_view(
            "SELECT team_id "
            "FROM team_members "
            "JOIN people ON team_members.person_id=people.id "
            "WHERE name=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_get_team_id_from_name, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->person_get_team_id_from_name);
    switch (ret)
    {
        case SQLITE_ROW  :
            team_id = sqlite3_column_int(ctx->person_get_team_id_from_name, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->person_get_team_id_from_name);
    return team_id;
}

static int
person_set_tag(struct db* ctx, int person_id, struct str_view tag)
{
    int ret;
    if (ctx->person_set_tag == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_set_tag, cstr_view(
            "UPDATE OR IGNORE people SET tag=? WHERE id=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_set_tag, 1, tag.data, tag.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->person_set_tag, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->person_set_tag);
}

static int
person_set_social(struct db* ctx, int person_id, struct str_view social)
{
    int ret;
    if (ctx->person_set_social == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_set_social, cstr_view(
            "UPDATE OR IGNORE people SET social=? WHERE id=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_set_social, 1, social.data, social.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->person_set_social, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->person_set_social);
}

static int
person_set_pronouns(struct db* ctx, int person_id, struct str_view pronouns)
{
    int ret;
    if (ctx->person_set_pronouns == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_set_pronouns, cstr_view(
            "UPDATE OR IGNORE people SET social=? WHERE id=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_set_pronouns, 1, pronouns.data, pronouns.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->person_set_pronouns, 2, person_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->person_set_pronouns);
}

static int
game_add_or_get(
        struct db* ctx,
        int round_type_id,
        int round_number,
        int set_format_id,
        int winner_team_id,
        int stage_id,
        uint64_t time_started,
        int duration)
{
    int ret, game_id = -1;
    if (ctx->game_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_add_or_get, cstr_view(
            "INSERT INTO games (round_type_id, round_number, set_format_id, winner_team_id, stage_id, time_started, duration) VALUES (?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET stage_id=excluded.stage_id RETURNING id;")) != 0)
            return -1;

    if (round_type_id > 0)
    {
        if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 1, round_type_id)) != SQLITE_OK)
            goto error;
    }
    else
    {
        if ((ret = sqlite3_bind_null(ctx->game_add_or_get, 1)) != SQLITE_OK)
            goto error;
    }

    if (round_number > 0)
    {
        if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 2, round_number)) != SQLITE_OK)
            goto error;
    }
    else
    {
        if ((ret = sqlite3_bind_null(ctx->game_add_or_get, 2)) != SQLITE_OK)
            goto error;
    }

    if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 3, set_format_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 4, winner_team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 5, stage_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int64(ctx->game_add_or_get, 6, (int64_t)time_started)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 7, duration)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->game_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            game_id = sqlite3_column_int(ctx->game_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->game_add_or_get);
    return game_id;
}

static int
game_get_all(struct db* ctx,
    int (*on_game)(
        int game_id,
        uint64_t time_started,
        int duration,
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
        void* user),
    void* user)
{
    int ret;
    if (ctx->game_get_all == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_get_all, cstr_view(
            "WITH grouped_games AS ( "
            "    SELECT "
            "        games.id, "
            "        time_started, "
            "        duration, "
            "        round_type_id, "
            "        round_number, "
            "        set_format_id, "
            "        winner_team_id, "
            "        stage_id, "
            "        teams.name team_name, "
            "        IFNULL(scores.score, '') scores, "
            "        group_concat(game_players.slot, '+') slots, "
            "        group_concat(REPLACE(IFNULL(sponsors.short_name, ''), '+', '\\+'), '+') sponsors, "
            "        group_concat(REPLACE(people.name, '+', '\\+'), '+') players, "
            "        group_concat(REPLACE(IFNULL(fighters.name, game_players.fighter_id), '+', '\\+'), '+') fighters, "
            "        group_concat(game_players.costume, '+') costumes "
            "    FROM game_players "
            "    INNER JOIN games ON games.id = game_players.game_id "
            "    INNER JOIN teams ON teams.id = game_players.team_id "
            "    LEFT JOIN scores ON scores.team_id = game_players.team_id AND scores.game_id = game_players.game_id "
            "    INNER JOIN people ON people.id = game_players.person_id "
            "    LEFT JOIN fighters ON fighters.id = game_players.fighter_id "
            "    LEFT JOIN sponsors ON sponsors.id = people.sponsor_id "
            "    GROUP BY games.id, game_players.team_id "
            "    ORDER BY game_players.slot) "
            "SELECT "
            "    grouped_games.id, "
            "    time_started, "
            "    duration, "
            "    IFNULL(tournaments.name, '') tourney, "
            "    IFNULL(event_types.name, '') event, "
            "    IFNULL(stages.name, grouped_games.stage_id) stage, "
            "    IFNULL(round_types.short_name, '') || IFNULL(round_number, '') round, "
            "    set_formats.short_name format, "
            "    group_concat(REPLACE(grouped_games.team_name, ',', '\\,')) teams, "
            "    group_concat(grouped_games.scores) score, "
            "    group_concat(grouped_games.slots) slots, "
            "    group_concat(REPLACE(IFNULL(grouped_games.sponsors, ''), ',', '\\,')) sponsors, "
            "    group_concat(REPLACE(grouped_games.players, ',', '\\,')) players, "
            "    group_concat(REPLACE(grouped_games.fighters, ',', '\\,')) fighters, "
            "    group_concat(grouped_games.costumes) costumes "
            "FROM grouped_games "
            "LEFT JOIN tournament_games ON tournament_games.game_id = grouped_games.id "
            "LEFT JOIN tournaments ON tournament_games.tournament_id = tournaments.id "
            "LEFT JOIN event_games ON event_games.game_id = grouped_games.id "
            "LEFT JOIN events ON event_games.event_id = events.id "
            "LEFT JOIN event_types ON event_types.id = events.event_type_id "
            "LEFT JOIN stages ON stages.id = grouped_games.stage_id "
            "LEFT JOIN round_types ON grouped_games.round_type_id = round_types.id "
            "INNER JOIN set_formats ON grouped_games.set_format_id = set_formats.id "
            "GROUP BY grouped_games.id "
            "ORDER BY time_started DESC;")) != 0)
            return -1;

next_step:
    ret = sqlite3_step(ctx->game_get_all);
    switch (ret)
    {
        case SQLITE_ROW:
            ret = on_game(
                sqlite3_column_int(ctx->game_get_all, 0),
                (uint64_t)sqlite3_column_int64(ctx->game_get_all, 1),
                sqlite3_column_int(ctx->game_get_all, 2),
                (const char*)sqlite3_column_text(ctx->game_get_all, 3),
                (const char*)sqlite3_column_text(ctx->game_get_all, 4),
                (const char*)sqlite3_column_text(ctx->game_get_all, 5),
                (const char*)sqlite3_column_text(ctx->game_get_all, 6),
                (const char*)sqlite3_column_text(ctx->game_get_all, 7),
                (const char*)sqlite3_column_text(ctx->game_get_all, 8),
                (const char*)sqlite3_column_text(ctx->game_get_all, 9),
                (const char*)sqlite3_column_text(ctx->game_get_all, 10),
                (const char*)sqlite3_column_text(ctx->game_get_all, 11),
                (const char*)sqlite3_column_text(ctx->game_get_all, 12),
                (const char*)sqlite3_column_text(ctx->game_get_all, 13),
                (const char*)sqlite3_column_text(ctx->game_get_all, 14),
                user);
            if (ret)
            {
                sqlite3_reset(ctx->game_get_all);
                return ret;
            }
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->game_get_all);
            return 0;
    }

    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->game_get_all);
    return -1;
}

static int
game_get_events(struct db* ctx,
    int (*on_game_event)(
        const char* date,
        const char* name,
        int event_id,
        void* user),
    void* user)
{
    int ret;
    if (ctx->game_get_events == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_get_events, cstr_view(
            "SELECT DATE(time_started/1000, 'unixepoch') date, IFNULL(event_types.name, ''), event_id "
            "FROM games "
            "LEFT JOIN event_games ON event_games.game_id=games.id "
            "LEFT JOIN events ON events.id=event_id "
            "LEFT JOIN event_types ON event_types.id=event_type_id "
            "GROUP BY date, event_types.name "
            "ORDER BY event_types.name;")) != 0)
            return -1;

next_step:
    ret = sqlite3_step(ctx->game_get_events);
    switch (ret)
    {
        case SQLITE_ROW:
            ret = on_game_event(
                (const char*)sqlite3_column_text(ctx->game_get_events, 0),
                (const char*)sqlite3_column_text(ctx->game_get_events, 1),
                sqlite3_column_type(ctx->game_get_events, 2) == SQLITE_NULL ?
                    -1 : sqlite3_column_int(ctx->game_get_events, 2),
                user);
            if (ret)
            {
                sqlite3_reset(ctx->game_get_events);
                return ret;
            }
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE:
            sqlite3_reset(ctx->game_get_events);
            return 0;
    }

    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->game_get_events);
    return -1;
}

static int
game_get_all_in_event(struct db* ctx,
    struct str_view date, int event_id,
    int (*on_game)(
        int game_id,
        uint64_t time_started,
        int duration,
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
        void* user),
    void* user)
{
    int ret;
    if (ctx->game_get_all_in_event == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_get_all_in_event, cstr_view(
            "WITH grouped_games AS ( "
            "    SELECT "
            "        games.id, "
            "        time_started, "
            "        duration, "
            "        round_type_id, "
            "        round_number, "
            "        set_format_id, "
            "        winner_team_id, "
            "        stage_id, "
            "        teams.name team_name, "
            "        IFNULL(scores.score, '') scores, "
            "        group_concat(game_players.slot, '+') slots, "
            "        group_concat(REPLACE(IFNULL(sponsors.short_name, ''), '+', '\\+'), '+') sponsors, "
            "        group_concat(REPLACE(people.name, '+', '\\+'), '+') players, "
            "        group_concat(REPLACE(IFNULL(fighters.name, game_players.fighter_id), '+', '\\+'), '+') fighters, "
            "        group_concat(game_players.costume, '+') costumes "
            "    FROM game_players "
            "    INNER JOIN games ON games.id = game_players.game_id "
            "    INNER JOIN teams ON teams.id = game_players.team_id "
            "    LEFT JOIN scores ON scores.team_id = game_players.team_id AND scores.game_id = game_players.game_id "
            "    INNER JOIN people ON people.id = game_players.person_id "
            "    LEFT JOIN fighters ON fighters.id = game_players.fighter_id "
            "    LEFT JOIN sponsors ON sponsors.id = people.sponsor_id "
            "    GROUP BY games.id, game_players.team_id "
            "    ORDER BY game_players.slot) "
            "SELECT "
            "    event_id, "
            "    grouped_games.id, "
            "    time_started, "
            "    duration, "
            "    IFNULL(tournaments.name, '') tourney, "
            "    IFNULL(event_types.name, '') event, "
            "    IFNULL(stages.name, grouped_games.stage_id) stage, "
            "    IFNULL(round_types.short_name, '') || IFNULL(round_number, '') round, "
            "    set_formats.short_name format, "
            "    group_concat(REPLACE(grouped_games.team_name, ',', '\\,')) teams, "
            "    group_concat(grouped_games.scores) score, "
            "    group_concat(grouped_games.slots) slots, "
            "    group_concat(REPLACE(IFNULL(grouped_games.sponsors, ''), ',', '\\,')) sponsors, "
            "    group_concat(REPLACE(grouped_games.players, ',', '\\,')) players, "
            "    group_concat(REPLACE(grouped_games.fighters, ',', '\\,')) fighters, "
            "    group_concat(grouped_games.costumes) costumes "
            "FROM grouped_games "
            "LEFT JOIN tournament_games ON tournament_games.game_id = grouped_games.id "
            "LEFT JOIN tournaments ON tournament_games.tournament_id = tournaments.id "
            "LEFT JOIN event_games ON event_games.game_id = grouped_games.id "
            "LEFT JOIN events ON event_games.event_id = events.id "
            "LEFT JOIN event_types ON event_types.id = events.event_type_id "
            "LEFT JOIN stages ON stages.id = grouped_games.stage_id "
            "LEFT JOIN round_types ON grouped_games.round_type_id = round_types.id "
            "INNER JOIN set_formats ON grouped_games.set_format_id = set_formats.id "
            "WHERE DATE(time_started/1000, 'unixepoch') = ?  "
            "GROUP BY grouped_games.id "
            "ORDER BY time_started DESC;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->game_get_all_in_event, 1, date.data, date.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->game_get_all_in_event);
    switch (ret)
    {
        case SQLITE_ROW:
            /* 
             * Because the SQL statement would require a different syntax "IS NULL" for finding
             * games with no associated event vs "= id" for finding games with associated events,
             * it's simpler to return the event_id and compare it here, than to compile two
             * statements.
             */
            if (event_id > 0 && sqlite3_column_type(ctx->game_get_all_in_event, 0) == SQLITE_NULL)
                goto next_step;
            if (event_id < 0 && sqlite3_column_type(ctx->game_get_all_in_event, 0) != SQLITE_NULL)
                goto next_step;
            if (event_id != sqlite3_column_int(ctx->game_get_all_in_event, 0))
                goto next_step;

            ret = on_game(
                sqlite3_column_int(ctx->game_get_all_in_event, 1),
                (uint64_t)sqlite3_column_int64(ctx->game_get_all_in_event, 2),
                sqlite3_column_int(ctx->game_get_all_in_event, 3),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 4),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 5),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 6),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 7),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 8),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 9),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 10),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 11),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 12),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 13),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 14),
                (const char*)sqlite3_column_text(ctx->game_get_all_in_event, 15),
                user);
            if (ret)
            {
                sqlite3_reset(ctx->game_get_all_in_event);
                return ret;
            }
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE:
            sqlite3_reset(ctx->game_get_all_in_event);
            return 0;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->game_get_all_in_event);
    return -1;
}

static int
game_associate_tournament(struct db* ctx, int game_id, int tournament_id)
{
    int ret;
    if (ctx->game_associate_tournament == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_associate_tournament, cstr_view(
            "INSERT OR IGNORE INTO tournament_games (game_id, tournament_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_associate_tournament, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_associate_tournament, 2, tournament_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_associate_tournament);
}

static int
game_associate_event(struct db* ctx, int game_id, int event_id)
{
    int ret;
    if (ctx->game_associate_event == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_associate_event, cstr_view(
            "INSERT OR IGNORE INTO event_games (game_id, event_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_associate_event, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_associate_event, 2, event_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_associate_event);
}

static int
game_associate_video(struct db* ctx, int game_id, int video_id, int64_t frame_offset)
{
    int ret;
    if (ctx->game_associate_video == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_associate_video, cstr_view(
            "INSERT OR IGNORE INTO game_videos (game_id, video_id, frame_offset) VALUES (?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_associate_video, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_associate_video, 2, video_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int64(ctx->game_associate_video, 3, frame_offset)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_associate_video);
}

static int
game_unassociate_video(struct db* ctx, int game_id, int video_id)
{
    int ret;
    if (ctx->game_unassociate_video == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_unassociate_video, cstr_view(
            "DELETE FROM game_videos WHERE game_id = ? AND video_id = ?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_unassociate_video, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_unassociate_video, 2, video_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_unassociate_video);
}

static int
game_get_videos(struct db* ctx, int game_id,
    int (*on_video)(
        const char* file_name,
        const char* path_hint,
        int64_t frame_offset,
        void* user),
    void* user)
{
    int ret;
    if (ctx->game_get_videos == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_get_videos, cstr_view(
            "SELECT file_name, path_hint, frame_offset "
            "FROM game_videos JOIN videos ON game_videos.video_id = videos.id "
            "WHERE game_videos.game_id = ?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_get_videos, 1, game_id)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->game_get_videos);
    switch (ret)
    {
        case SQLITE_ROW:
            ret = on_video(
                (const char*)sqlite3_column_text(ctx->game_get_videos, 0),
                (const char*)sqlite3_column_text(ctx->game_get_videos, 1),
                sqlite3_column_int64(ctx->game_get_videos, 2),
                user);
            if (ret)
            {
                sqlite3_reset(ctx->game_get_videos);
                return ret;
            }
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->game_get_videos);
            return 0;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->game_get_videos);
    return -1;
}

static int
game_add_player(
    struct db* ctx,
    int person_id,
    int game_id,
    int slot,
    int team_id,
    int fighter_id,
    int costume,
    int is_loser_side)
{
    int ret;
    if (ctx->game_add_player == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_add_player, cstr_view(
            "INSERT OR IGNORE INTO game_players (person_id, game_id, slot, team_id, fighter_id, costume, is_loser_side) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_add_player, 1, person_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 2, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 3, slot)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 4, team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 5, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 6, costume)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_add_player, 7, is_loser_side)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_add_player);
}

static int
group_add_or_get(struct db* ctx, struct str_view name)
{
    int ret, group_id = -1;
    if (ctx->group_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->group_add_or_get, cstr_view(
            "INSERT INTO groups (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->group_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
        goto error;

next_step:
    ret = sqlite3_step(ctx->group_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            group_id = sqlite3_column_int(ctx->group_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->group_add_or_get);
    return group_id;
}

static int
group_add_game(struct db* ctx, int group_id, int game_id)
{
    int ret;
    if (ctx->group_add_game == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->group_add_game, cstr_view(
            "INSERT OR IGNORE INTO game_groups (group_id, game_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->group_add_game, 1, group_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->group_add_game, 2, game_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->group_add_game);
}

static int
video_add_or_get(struct db* ctx, struct str_view file_name, struct str_view path_hint)
{
    int ret, video_id = -1;
    if (ctx->video_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->video_add_or_get, cstr_view(
            "INSERT INTO videos (file_name, path_hint) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET file_name=excluded.file_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->video_add_or_get, 1, file_name.data, file_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->video_add_or_get, 2, path_hint.data, path_hint.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        goto error;
    }

next_step:
    ret = sqlite3_step(ctx->video_add_or_get);
    switch (ret)
    {
        case SQLITE_ROW  :
            video_id = sqlite3_column_int(ctx->video_add_or_get, 0);
            goto done;
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE : goto done;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
done:
    sqlite3_reset(ctx->video_add_or_get);
    return video_id;
}

static int
video_set_path_hint(struct db* ctx, struct str_view file_name, struct str_view path_hint)
{
    int ret;
    if (ctx->video_set_path_hint == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->video_set_path_hint, cstr_view(
            "UPDATE videos SET path_hint = ? WHERE file_name = ?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->video_set_path_hint, 1, path_hint.data, path_hint.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->video_set_path_hint, 2, file_name.data, file_name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->video_set_path_hint);
}

static int
video_add_path(struct db* ctx, struct str_view path)
{
    int ret;
    if (ctx->video_add_path == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->video_add_path, cstr_view(
            "INSERT OR IGNORE INTO video_paths (path) VALUES (?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->video_add_path, 1, path.data, path.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->video_add_path);
}

static int
video_query_paths(struct db* ctx, int (*on_video_path)(const char* path, void* user), void* user)
{
    int ret;
    if (ctx->video_query_paths == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->video_query_paths, cstr_view(
            "SELECT path FROM video_paths;")) != 0)
            return -1;

next_step:
    ret = sqlite3_step(ctx->video_query_paths);
    switch (ret)
    {
        case SQLITE_ROW:
            ret = on_video_path((const char*)sqlite3_column_text(ctx->video_query_paths, 0), user);
            if (ret)
            {
                sqlite3_reset(ctx->video_query_paths);
                return ret;
            }
        case SQLITE_BUSY : goto next_step;
        case SQLITE_DONE :
            sqlite3_reset(ctx->video_query_paths);
            return 0;
    }

    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    sqlite3_reset(ctx->video_query_paths);
    return -1;
}

static int
score_add(struct db* ctx, int game_id, int team_id, int score)
{
    int ret;
    if (ctx->score_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->score_add, cstr_view(
            "INSERT OR IGNORE INTO scores (game_id, team_id, score) VALUES (?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->score_add, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->score_add, 2, team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->score_add, 3, score)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->score_add);
}

static int
switch_info_add(struct db* ctx, struct str_view name, struct str_view ip, uint16_t port)
{
    int ret;
    if (ctx->switch_info_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->switch_info_add, cstr_view(
            "INSERT OR IGNORE INTO switch_info (name, ip, port) VALUES (?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->switch_info_add, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->switch_info_add, 2, ip.data, ip.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->switch_info_add, 3, (int)port)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->switch_info_add);
}

static int
stream_recording_sources_add(struct db* ctx, struct str_view path, int frame_offset)
{
    int ret;
    if (ctx->stream_recording_sources_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->stream_recording_sources_add, cstr_view(
            "INSERT OR IGNORE INTO stream_recording_sources (path, frame_offset) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->stream_recording_sources_add, 1, path.data, path.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->stream_recording_sources_add, 2, frame_offset)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->stream_recording_sources_add);
}

struct db_interface db_sqlite = {
    open_and_prepare,
    close_db,

    {
        transaction_begin,
        transaction_commit,
        transaction_rollback,
        transaction_begin_nested,
        transaction_commit_nested,
        transaction_rollback_nested,
    },

#define X(group, stmt) group##_##stmt,
    STMT_LIST
#undef X
};

struct db_interface* db(const char* type)
{
    if (strcmp("sqlite", type) == 0)
        return &db_sqlite;
    return NULL;
}

#if defined(VH_MEM_DEBUGGING)
static int
vh_mem_roundup(int size) { return size; }
static int
vh_mem_init(void* user) { (void)user; return 0; }
static void
vh_mem_deinit(void* user) { (void)user; }

static struct sqlite3_mem_methods vh_mem_sqlite = {
    mem_alloc,
    mem_free,
    mem_realloc,
    mem_allocated_size,
    vh_mem_roundup,
    vh_mem_init,
    vh_mem_deinit,
    NULL
};
#endif

int
db_init(void)
{
#if defined(VH_MEM_DEBUGGING)
    sqlite3_config(SQLITE_CONFIG_MALLOC, &vh_mem_sqlite);
#endif

    if (sqlite3_initialize() != SQLITE_OK)
        return -1;
    return 0;
}

void
db_deinit(void)
{
    sqlite3_shutdown();
}
