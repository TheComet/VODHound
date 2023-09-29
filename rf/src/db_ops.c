#include "rf/db_ops.h"
#include "rf/log.h"
#include "rf/mem.h"
#include "rf/mfile.h"

#include "sqlite/sqlite3.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if !defined(min)
#   define min(a, b) ((a) < (b) ? (a) : (b))
#endif

struct rf_db
{
    sqlite3* db;

    sqlite3_stmt* motion_add;
    sqlite3_stmt* fighter_add;
    sqlite3_stmt* stage_add;
    sqlite3_stmt* status_enum_add;
    sqlite3_stmt* hit_status_enum_add;

    sqlite3_stmt* tournament_add_or_get;
    sqlite3_stmt* sponsor_add_or_get;
    sqlite3_stmt* tournament_sponsor_add;
    sqlite3_stmt* tournament_organizer_add;
    sqlite3_stmt* tournament_commentator_add;

    sqlite3_stmt* person_add_or_get;
};

static int
exec_sql_wrapper(sqlite3* db, const char* sql)
{
    char* error_message;
    int ret = sqlite3_exec(db, sql, NULL, NULL, &error_message);
    if (ret != SQLITE_OK)
    {
        rf_log_sqlite_err(ret, error_message, sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

static int
prepare_stmt_wrapper(sqlite3* db, sqlite3_stmt** stmt, struct rf_str_view sql)
{
    int ret = sqlite3_prepare_v2(db, sql.data, sql.len, stmt, NULL);
    if (ret != SQLITE_OK)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

static int
step_stmt_wrapper(sqlite3* db, sqlite3_stmt* stmt)
{
    int ret = sqlite3_step(stmt);
    if (ret != SQLITE_DONE)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }

    /* TODO: Required? */
    sqlite3_reset(stmt);
    return 0;
}

static int
run_migration_script(sqlite3* db, const char* file_name)
{
    int ret;
    struct rf_mfile mf;
    sqlite3_stmt* stmt;
    const char* sql;
    const char* sql_next;
    int sql_len;

    rf_log_info("Running migration script '%s'\n", file_name);

    if (rf_mfile_map(&mf, file_name) != 0)
        goto open_script_failed;
    sql = mf.address;
    sql_len = mf.size;

next_step:
    ret = sqlite3_prepare_v2(db, sql, sql_len, &stmt, &sql_next);
    if (ret != SQLITE_OK)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        goto prepare_failed;
    }
retry_step:
    switch (ret = sqlite3_step(stmt))
    {
        case SQLITE_BUSY:
            goto retry_step;
        case SQLITE_ROW:
        case SQLITE_DONE:
            sql_len -= sql_next - sql;
            sql = sql_next;
            for (; sql_len && isspace(*sql); ++sql, --sql_len) {}
            if (sql_len <= 0)
                break;
            goto next_step;
        default:
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
            goto exec_failed;
    }

    sqlite3_finalize(stmt);
    rf_mfile_unmap(&mf);
    return 0;

exec_failed        : sqlite3_finalize(stmt);
prepare_failed     : rf_mfile_unmap(&mf);
open_script_failed : return -1;
}

static int
check_version_and_migrate(sqlite3* db)
{
    int ret;
    int version;
    sqlite3_stmt* stmt;

    ret = sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &stmt, NULL);
    if (ret != SQLITE_OK)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        return -1;
    }

    ret = sqlite3_step(stmt);
    if (ret != SQLITE_ROW)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    version = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    rf_log_info("db version: %d\n", version);

    if (exec_sql_wrapper(db, "BEGIN TRANSACTION") != 0)
        return -1;

    switch (version)
    {
        case 1: if (run_migration_script(db, "migrations/1-schema.down.sql") != 0) goto migrate_failed;
        case 0: break;
    }
    version = 0;

    switch (version)
    {
        case 0: if (run_migration_script(db, "migrations/1-schema.up.sql") != 0) goto migrate_failed;
        /*case 1: if (upgrade_to_v2(db) != 0) goto migrate_failed;
        case 2: if (upgrade_to_v3(db) != 0) goto migrate_failed;*/
        case 1: break;

        default:
            rf_log_err("Unknown database version %d. Aborting all operations\n", version);
            goto migrate_failed;
    }

    if (exec_sql_wrapper(db, "PRAGMA user_version=1") != 0)
        goto migrate_failed;

    if (exec_sql_wrapper(db, "COMMIT TRANSACTION") != 0)
        goto migrate_failed;

    rf_log_info("Successfully migrated to db version 1\n");

    return 0;

migrate_failed : exec_sql_wrapper(db, "ROLLBACK TRANSACTION");
    return -1;
}

static struct rf_db*
open_and_prepare(const char* uri)
{
    int ret;
    struct rf_db* ctx = rf_malloc(sizeof *ctx);
    if (ctx == NULL)
        goto oom;
    memset(ctx, 0, sizeof *ctx);

    ret = sqlite3_open_v2(uri, &ctx->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        goto open_db_failed;
    }

    if (check_version_and_migrate(ctx->db) != 0)
        goto migrate_db_failed;

    return ctx;

migrate_db_failed             :
open_db_failed                : rf_free(ctx);
oom                           : return NULL;
}

static void
close_db(struct rf_db* ctx)
{
    sqlite3_finalize(ctx->person_add_or_get);
    sqlite3_finalize(ctx->tournament_commentator_add);
    sqlite3_finalize(ctx->tournament_organizer_add);
    sqlite3_finalize(ctx->tournament_sponsor_add);
    sqlite3_finalize(ctx->sponsor_add_or_get);
    sqlite3_finalize(ctx->tournament_add_or_get);
    sqlite3_finalize(ctx->hit_status_enum_add);
    sqlite3_finalize(ctx->status_enum_add);
    sqlite3_finalize(ctx->stage_add);
    sqlite3_finalize(ctx->fighter_add);
    sqlite3_finalize(ctx->motion_add);

    sqlite3_close(ctx->db);
    rf_free(ctx);
}

static int
transaction_begin(struct rf_db* ctx)
{
    return exec_sql_wrapper(ctx->db, "BEGIN TRANSACTION");
}

static int
transaction_commit(struct rf_db* ctx)
{
    return exec_sql_wrapper(ctx->db, "COMMIT TRANSACTION");
}

static int
transaction_rollback(struct rf_db* ctx)
{
    return exec_sql_wrapper(ctx->db, "ROLLBACK TRANSACTION");
}

static int
transaction_begin_nested(struct rf_db* ctx, struct rf_str_view name)
{
    char buf[64] = "SAVEPOINT ";
    strncat(buf, name.data, min(name.len, 64 - sizeof("SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_commit_nested(struct rf_db* ctx, struct rf_str_view name)
{
    char buf[64] = "RELEASE SAVEPOINT ";
    strncat(buf, name.data, min(name.len, 64 - sizeof("RELEASE SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_rollback_nested(struct rf_db* ctx, struct rf_str_view name)
{
    char buf[64] = "ROLLBACK TO SAVEPOINT ";
    strncat(buf, name.data, min(name.len, 64 - sizeof("ROLLBACK TO SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
motion_add(struct rf_db* ctx, uint64_t hash40, struct rf_str_view string)
{
    int ret;
    if (ctx->motion_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_add,  rf_cstr_view(
            "INSERT OR IGNORE INTO motions (hash40, string) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_add, 1, (int64_t)hash40) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->motion_add, 2, string.data, string.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->motion_add);
}

static int
fighter_add(struct rf_db* ctx, int fighter_id, struct rf_str_view name)
{
    int ret;
    if (ctx->fighter_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->fighter_add, rf_cstr_view(
            "INSERT OR IGNORE INTO fighters (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->fighter_add, 1, fighter_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->fighter_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->fighter_add);
}

static int
stage_add(struct rf_db* ctx, int stage_id, struct rf_str_view name)
{
    int ret;
    if (ctx->stage_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->stage_add, rf_cstr_view(
            "INSERT OR IGNORE INTO stages (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->stage_add, 1, stage_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->stage_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->stage_add);
}

static int
status_enum_add(struct rf_db* ctx, int fighter_id, int status_id, struct rf_str_view name)
{
    int ret;
    if (ctx->status_enum_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->status_enum_add, rf_cstr_view(
            "INSERT OR IGNORE INTO status_enums (fighter_id, value, name) VALUES (?, ?, ?);")) != 0)
            return -1;

    if (fighter_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->status_enum_add, 1) != SQLITE_OK))
        {
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->status_enum_add, 1, fighter_id) != SQLITE_OK))
        {
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_int(ctx->status_enum_add, 2, status_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->status_enum_add, 3, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->status_enum_add);
}

static int
hit_status_enum_add(struct rf_db* ctx, int id, struct rf_str_view name)
{
    int ret;
    if (ctx->hit_status_enum_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->hit_status_enum_add, rf_cstr_view(
            "INSERT OR IGNORE INTO hit_status_enums (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->hit_status_enum_add, 1, id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->hit_status_enum_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->hit_status_enum_add);
}

static int
tournament_add_or_get(struct rf_db* ctx, struct rf_str_view name, struct rf_str_view website)
{
    int ret, tournament_id = -1;
    if (ctx->tournament_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_or_get, rf_cstr_view(
            "INSERT INTO tournaments (name, website) VALUES (?, ?) ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->tournament_add_or_get, 1, name.data, name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->tournament_add_or_get, 2, website.data, website.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->tournament_add_or_get);
    switch (ret)
    {
    case SQLITE_BUSY: goto next_step;
    case SQLITE_DONE: break;
    case SQLITE_ROW:
        tournament_id = sqlite3_column_int(ctx->tournament_add_or_get, 0);
        goto next_step;
    default:
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        break;
    }

    /* TODO: Required? */
    sqlite3_reset(ctx->tournament_add_or_get);

    return tournament_id;
}

static int
sponsor_add_or_get(struct rf_db* ctx, struct rf_str_view short_name, struct rf_str_view full_name, struct rf_str_view website)
{
    int ret, sponsor_id = -1;
    if (ctx->sponsor_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->sponsor_add_or_get, rf_cstr_view(
            "INSERT INTO sponsors (short_name, full_name, website) VALUES (?, ?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 2, full_name.data, full_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 3, website.data, website.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->sponsor_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            sponsor_id = sqlite3_column_int(ctx->sponsor_add_or_get, 0);
            goto next_step;
        default:
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    /* TODO: Required? */
    sqlite3_reset(ctx->sponsor_add_or_get);

    return sponsor_id;
}

static int
tournament_sponsor_add(struct rf_db* ctx, int tournament_id, int sponsor_id)
{
    int ret;
    if (ctx->tournament_sponsor_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_sponsor_add, rf_cstr_view(
            "INSERT OR IGNORE INTO tournament_sponsors (tournament_id, sponsor_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 2, sponsor_id) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_sponsor_add);
}

static int
tournament_organizer_add(struct rf_db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_organizer_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_organizer_add, rf_cstr_view(
            "INSERT OR IGNORE INTO tournament_organizers (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_organizer_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_organizer_add, 2, person_id) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_organizer_add);
}

static int
tournament_commentator_add(struct rf_db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_commentator_add == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_commentator_add, rf_cstr_view(
            "INSERT OR IGNORE INTO tournament_commentators (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_commentator_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_commentator_add, 2, person_id) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_commentator_add);
}

static int
person_add_or_get(
        struct rf_db* ctx,
        int sponsor_id,
        struct rf_str_view name,
        struct rf_str_view tag,
        struct rf_str_view social,
        struct rf_str_view pronouns)
{
    int ret, person_id = -1;
    if (ctx->person_add_or_get == NULL)
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_add_or_get, rf_cstr_view(
            "INSERT INTO people (sponsor_id, name, tag, social, pronouns) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if (sponsor_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->person_add_or_get, 1) != SQLITE_OK))
        {
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->person_add_or_get, 1, sponsor_id) != SQLITE_OK))
        {
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_text(ctx->person_add_or_get, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 3, tag.data, tag.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 4, social.data, social.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 5, pronouns.data, pronouns.len, SQLITE_STATIC) != SQLITE_OK))
    {
        rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->person_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            person_id = sqlite3_column_int(ctx->person_add_or_get, 0);
            goto next_step;
        default:
            rf_log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    /* TODO: Required? */
    sqlite3_reset(ctx->person_add_or_get);

    return person_id;
}

struct rf_db_interface rf_db_sqlite = {
    open_and_prepare,
    close_db,

    transaction_begin,
    transaction_commit,
    transaction_rollback,
    transaction_begin_nested,
    transaction_commit_nested,
    transaction_rollback_nested,

    motion_add,
    fighter_add,
    stage_add,
    status_enum_add,
    hit_status_enum_add,

    tournament_add_or_get,
    sponsor_add_or_get,
    tournament_sponsor_add,
    tournament_organizer_add,
    tournament_commentator_add,

    person_add_or_get
};

struct rf_db_interface* rf_db(const char* type)
{
    if (strcmp("sqlite", type) == 0)
        return &rf_db_sqlite;
    return NULL;
}
