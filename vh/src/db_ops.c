#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/mfile.h"

#include "sqlite/sqlite3.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#if !defined(min)
#   define min(a, b) ((a) < (b) ? (a) : (b))
#endif

struct db
{
    sqlite3* db;

    sqlite3_stmt* motion_add;
    sqlite3_stmt* fighter_add;
    sqlite3_stmt* fighter_name;
    sqlite3_stmt* stage_add;
    sqlite3_stmt* status_enum_add;
    sqlite3_stmt* hit_status_enum_add;

    sqlite3_stmt* tournament_add_or_get;
    sqlite3_stmt* tournament_sponsor_add;
    sqlite3_stmt* tournament_organizer_add;
    sqlite3_stmt* tournament_commentator_add;

    sqlite3_stmt* event_type_add_or_get;
    sqlite3_stmt* event_add_or_get;

    sqlite3_stmt* round_type_add_or_get;
    sqlite3_stmt* round_add_or_get;

    sqlite3_stmt* set_format_add_or_get;

    sqlite3_stmt* team_member_add;
    sqlite3_stmt* team_add_or_get;

    sqlite3_stmt* sponsor_add_or_get;
    sqlite3_stmt* person_add_or_get;
    sqlite3_stmt* person_get_id;
    sqlite3_stmt* person_get_team_id;

    sqlite3_stmt* game_add_or_get;
    sqlite3_stmt* game_players_add;

    sqlite3_stmt* score_add;

    sqlite3_stmt* frame_add;

    sqlite3_stmt* query_games;
    sqlite3_stmt* query_game_teams;
    sqlite3_stmt* query_game_players;
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

    log_info("Running migration script '%s'\n", file_name);

    if (mfile_map(&mf, file_name) != 0)
        goto open_script_failed;
    sql = mf.address;
    sql_len = mf.size;

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
            sql_len -= sql_next - sql;
            sql = sql_next;
            for (; sql_len && isspace(*sql); ++sql, --sql_len) {}
            if (sql_len <= 0)
                break;
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
check_version_and_migrate(sqlite3* db)
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

#if 0
    switch (version)
    {
        case 1: if (run_migration_script(db, "migrations/1-schema.down.sql") != 0) goto migrate_failed;
        case 0: break;
    }
    version = 0;
#endif

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

    log_info("Successfully migrated from version %d to version 1\n", version);

    return 0;

    migrate_failed : exec_sql_wrapper(db, "ROLLBACK TRANSACTION");
    return -1;
}

static struct db*
open_and_prepare(const char* uri)
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

    if (check_version_and_migrate(ctx->db) != 0)
        goto migrate_db_failed;

    return ctx;

    migrate_db_failed             :
    open_db_failed                : mem_free(ctx);
    oom                           : return NULL;
}

static void
close_db(struct db* ctx)
{
    sqlite3_finalize(ctx->query_game_players);
    sqlite3_finalize(ctx->query_game_teams);
    sqlite3_finalize(ctx->query_games);

    sqlite3_finalize(ctx->frame_add);

    sqlite3_finalize(ctx->score_add);

    sqlite3_finalize(ctx->game_players_add);
    sqlite3_finalize(ctx->game_add_or_get);

    sqlite3_finalize(ctx->person_get_team_id);
    sqlite3_finalize(ctx->person_get_id);
    sqlite3_finalize(ctx->person_add_or_get);
    sqlite3_finalize(ctx->sponsor_add_or_get);

    sqlite3_finalize(ctx->team_add_or_get);
    sqlite3_finalize(ctx->team_member_add);

    sqlite3_finalize(ctx->set_format_add_or_get);

    sqlite3_finalize(ctx->round_add_or_get);
    sqlite3_finalize(ctx->round_type_add_or_get);

    sqlite3_finalize(ctx->event_add_or_get);
    sqlite3_finalize(ctx->event_type_add_or_get);

    sqlite3_finalize(ctx->tournament_commentator_add);
    sqlite3_finalize(ctx->tournament_organizer_add);
    sqlite3_finalize(ctx->tournament_sponsor_add);
    sqlite3_finalize(ctx->tournament_add_or_get);

    sqlite3_finalize(ctx->hit_status_enum_add);
    sqlite3_finalize(ctx->status_enum_add);
    sqlite3_finalize(ctx->stage_add);
    sqlite3_finalize(ctx->fighter_name);
    sqlite3_finalize(ctx->fighter_add);
    sqlite3_finalize(ctx->motion_add);

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
    strncat(buf, name.data, min(name.len, 64 - sizeof("SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_commit_nested(struct db* ctx, struct str_view name)
{
    char buf[64] = "RELEASE SAVEPOINT ";
    strncat(buf, name.data, min(name.len, 64 - sizeof("RELEASE SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
transaction_rollback_nested(struct db* ctx, struct str_view name)
{
    char buf[64] = "ROLLBACK TO SAVEPOINT ";
    strncat(buf, name.data, min(name.len, 64 - sizeof("ROLLBACK TO SAVEPOINT ")));
    return exec_sql_wrapper(ctx->db, buf);
}

static int
motion_add(struct db* ctx, uint64_t hash40, struct str_view string)
{
    int ret;
    if (ctx->motion_add)
        sqlite3_reset(ctx->motion_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->motion_add,  cstr_view(
            "INSERT OR IGNORE INTO motions (hash40, string) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int64(ctx->motion_add, 1, (int64_t)hash40) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->motion_add, 2, string.data, string.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->motion_add);
}

static int
fighter_add(struct db* ctx, int fighter_id, struct str_view name)
{
    int ret;
    if (ctx->fighter_add)
        sqlite3_reset(ctx->fighter_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->fighter_add, cstr_view(
            "INSERT OR IGNORE INTO fighters (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->fighter_add, 1, fighter_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->fighter_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->fighter_add);
}

static const char*
fighter_name(struct db* ctx, int fighter_id)
{
    int ret;
    if (ctx->fighter_name)
        sqlite3_reset(ctx->fighter_name);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->fighter_name, cstr_view(
            "SELECT name FROM fighters WHERE id=?;")) != 0)
            return NULL;

    if ((ret = sqlite3_bind_int(ctx->fighter_name, 1, fighter_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return NULL;
    }

next_step:
    ret = sqlite3_step(ctx->fighter_name);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW: return (const char*)sqlite3_column_text(ctx->fighter_name, 0);
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return NULL;
}

static int
stage_add(struct db* ctx, int stage_id, struct str_view name)
{
    int ret;
    if (ctx->stage_add)
        sqlite3_reset(ctx->stage_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->stage_add, cstr_view(
            "INSERT OR IGNORE INTO stages (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->stage_add, 1, stage_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->stage_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
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
    if (ctx->status_enum_add)
        sqlite3_reset(ctx->status_enum_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->status_enum_add, cstr_view(
            "INSERT OR IGNORE INTO status_enums (fighter_id, value, name) VALUES (?, ?, ?);")) != 0)
            return -1;

    if (fighter_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->status_enum_add, 1) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->status_enum_add, 1, fighter_id) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_int(ctx->status_enum_add, 2, status_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->status_enum_add, 3, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->status_enum_add);
}

static int
hit_status_enum_add(struct db* ctx, int id, struct str_view name)
{
    int ret;
    if (ctx->hit_status_enum_add)
        sqlite3_reset(ctx->hit_status_enum_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->hit_status_enum_add, cstr_view(
            "INSERT OR IGNORE INTO hit_status_enums (id, name) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->hit_status_enum_add, 1, id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->hit_status_enum_add, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
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
    if (ctx->tournament_add_or_get)
        sqlite3_reset(ctx->tournament_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_add_or_get, cstr_view(
            "INSERT INTO tournaments (name, website) VALUES (?, ?) ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->tournament_add_or_get, 1, name.data, name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->tournament_add_or_get, 2, website.data, website.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
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
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return tournament_id;
}

static int
tournament_sponsor_add(struct db* ctx, int tournament_id, int sponsor_id)
{
    int ret;
    if (ctx->tournament_sponsor_add)
        sqlite3_reset(ctx->tournament_sponsor_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_sponsor_add, cstr_view(
            "INSERT OR IGNORE INTO tournament_sponsors (tournament_id, sponsor_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 2, sponsor_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_sponsor_add);
}

static int
tournament_organizer_add(struct db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_organizer_add)
        sqlite3_reset(ctx->tournament_organizer_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_organizer_add, cstr_view(
            "INSERT OR IGNORE INTO tournament_organizers (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_organizer_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_organizer_add, 2, person_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_organizer_add);
}

static int
tournament_commentator_add(struct db* ctx, int tournament_id, int person_id)
{
    int ret;
    if (ctx->tournament_commentator_add)
        sqlite3_reset(ctx->tournament_commentator_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->tournament_commentator_add, cstr_view(
            "INSERT OR IGNORE INTO tournament_commentators (tournament_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->tournament_commentator_add, 1, tournament_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->tournament_commentator_add, 2, person_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->tournament_commentator_add);
}

static int
event_type_add_or_get(struct db* ctx, struct str_view name)
{
    int ret, event_type_id = -1;
    if (ctx->event_type_add_or_get)
        sqlite3_reset(ctx->event_type_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->event_type_add_or_get, cstr_view(
            "INSERT INTO event_types (name) VALUES (?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->event_type_add_or_get, 1, name.data, name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->event_type_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            event_type_id = sqlite3_column_int(ctx->event_type_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return event_type_id;
}

static int
event_add_or_get(struct db* ctx, int event_type_id, struct str_view url)
{
    int ret, event_id = -1;
    if (ctx->event_add_or_get)
        sqlite3_reset(ctx->event_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->event_add_or_get, cstr_view(
            "INSERT INTO events (event_type_id, url) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET event_type_id=excluded.event_type_id RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->event_add_or_get, 1, event_type_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->event_add_or_get, 2, url.data, url.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->event_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            event_id = sqlite3_column_int(ctx->event_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return event_id;
}

static int
round_type_add_or_get(struct db* ctx, struct str_view short_name, struct str_view long_name)
{
    int ret, round_type_id = -1;
    if (ctx->round_type_add_or_get)
        sqlite3_reset(ctx->round_type_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->round_type_add_or_get, cstr_view(
            "INSERT INTO round_types (short_name, long_name) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->round_type_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->round_type_add_or_get, 2, long_name.data, long_name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->round_type_add_or_get);
    switch (ret)
    {
    case SQLITE_BUSY: goto next_step;
    case SQLITE_DONE: break;
    case SQLITE_ROW:
        round_type_id = sqlite3_column_int(ctx->round_type_add_or_get, 0);
        goto next_step;
    default:
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        break;
    }

    return round_type_id;
}

static int
round_add_or_get(struct db* ctx, int round_type_id, int number)
{
    int ret, round_id = -1;
    if (ctx->round_add_or_get)
        sqlite3_reset(ctx->round_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->round_add_or_get, cstr_view(
            "INSERT INTO rounds (round_type_id, number) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET number=excluded.number RETURNING id;")) != 0)
            return -1;

    if (round_type_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->round_add_or_get, 1) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->round_add_or_get, 1, round_type_id) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_int(ctx->round_add_or_get, 2, number) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->round_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            round_id = sqlite3_column_int(ctx->round_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return round_id;
}

static int
set_format_add_or_get(struct db* ctx, struct str_view short_name, struct str_view long_name)
{
    int ret, set_format_id = -1;
    if (ctx->set_format_add_or_get)
        sqlite3_reset(ctx->set_format_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->set_format_add_or_get, cstr_view(
            "INSERT INTO set_formats (short_name, long_name) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->set_format_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->set_format_add_or_get, 2, long_name.data, long_name.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->set_format_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            set_format_id = sqlite3_column_int(ctx->set_format_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return set_format_id;
}

static int
team_add_or_get(struct db* ctx, struct str_view name, struct str_view url)
{
    int ret, team_id = -1;
    if (ctx->team_add_or_get)
        sqlite3_reset(ctx->team_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->team_add_or_get, cstr_view(
            "INSERT INTO teams (name, url) VALUES (?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->team_add_or_get, 1, name.data, name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->team_add_or_get, 2, url.data, url.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->team_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            team_id = sqlite3_column_int(ctx->team_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return team_id;
}

static int
team_member_add(struct db* ctx, int team_id, int person_id)
{
    int ret;
    if (ctx->team_member_add)
        sqlite3_reset(ctx->team_member_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->team_member_add, cstr_view(
            "INSERT OR IGNORE INTO team_members (team_id, person_id) VALUES (?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->team_member_add, 1, team_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->team_member_add, 2, person_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->team_member_add);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            team_id = sqlite3_column_int(ctx->team_member_add, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return 0;
}

static int
sponsor_add_or_get(struct db* ctx, struct str_view short_name, struct str_view full_name, struct str_view website)
{
    int ret, sponsor_id = -1;
    if (ctx->sponsor_add_or_get)
        sqlite3_reset(ctx->sponsor_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->sponsor_add_or_get, cstr_view(
            "INSERT INTO sponsors (short_name, full_name, website) VALUES (?, ?, ?) "
            "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 2, full_name.data, full_name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 3, website.data, website.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
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
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return sponsor_id;
}

static int
person_add_or_get(
        struct db* ctx,
        int sponsor_id,
        struct str_view name,
        struct str_view tag,
        struct str_view social,
        struct str_view pronouns)
{
    int ret, person_id = -1;
    if (ctx->person_add_or_get)
        sqlite3_reset(ctx->person_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_add_or_get, cstr_view(
            "INSERT INTO people (sponsor_id, name, tag, social, pronouns) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;")) != 0)
            return -1;

    if (sponsor_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->person_add_or_get, 1) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->person_add_or_get, 1, sponsor_id) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_text(ctx->person_add_or_get, 2, name.data, name.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 3, tag.data, tag.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 4, social.data, social.len, SQLITE_STATIC) != SQLITE_OK) ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 5, pronouns.data, pronouns.len, SQLITE_STATIC) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
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
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return person_id;
}

static int
person_get_id(struct db* ctx, struct str_view name)
{
    int ret;
    if (ctx->person_get_id)
        sqlite3_reset(ctx->person_get_id);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_get_id, cstr_view(
            "SELECT id FROM people WHERE name=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_get_id, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->person_get_id);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW: return sqlite3_column_int(ctx->person_get_id, 0);
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return -1;
}

static int
person_get_team_id(struct db* ctx, struct str_view name)
{
    int ret;
    if (ctx->person_get_team_id)
        sqlite3_reset(ctx->person_get_team_id);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->person_get_team_id, cstr_view(
            "SELECT team_id "
            "FROM team_members "
            "JOIN people ON (team_members.person_id=people.id)"
            "WHERE name=?;")) != 0)
            return -1;

    if ((ret = sqlite3_bind_text(ctx->person_get_team_id, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->person_get_team_id);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW: return sqlite3_column_int(ctx->person_get_team_id, 0);
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return -1;
}

static int
game_add_or_get(
        struct db* ctx,
        int video_id,
        int tournament_id,
        int event_id,
        int round_id,
        int set_format_id,
        int winner_team_id,
        int stage_id,
        uint64_t time_started,
        uint64_t time_ended)
{
    int ret, game_id = -1;
    if (ctx->game_add_or_get)
        sqlite3_reset(ctx->game_add_or_get);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_add_or_get, cstr_view(
            "INSERT INTO games (video_id, tournament_id, event_id, round_id, set_format_id, winner_team_id, stage_id, time_started, time_ended) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT DO UPDATE SET stage_id=excluded.stage_id RETURNING id;")) != 0)
            return -1;

    if (video_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->game_add_or_get, 1) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 1, video_id) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if (tournament_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->game_add_or_get, 2) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 2, tournament_id) != SQLITE_OK))
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_int(ctx->game_add_or_get, 3, event_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 4, round_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 5, set_format_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 6, winner_team_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_add_or_get, 7, stage_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int64(ctx->game_add_or_get, 8, (int64_t)time_started) != SQLITE_OK) ||
        (ret = sqlite3_bind_int64(ctx->game_add_or_get, 9, (int64_t)time_ended) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->game_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            game_id = sqlite3_column_int(ctx->game_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return game_id;
}

static int
game_player_add(
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
    if (ctx->game_players_add)
        sqlite3_reset(ctx->game_players_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->game_players_add, cstr_view(
            "INSERT OR IGNORE INTO game_players (person_id, game_id, slot, team_id, fighter_id, costume, is_loser_side) VALUES (?, ?, ?, ?, ?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->game_players_add, 1, person_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 2, game_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 3, slot) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 4, team_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 5, fighter_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 6, costume) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->game_players_add, 7, is_loser_side) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_players_add);
}

static int
score_add(struct db* ctx, int game_id, int team_id, int score)
{
    int ret;
    if (ctx->score_add)
        sqlite3_reset(ctx->score_add);
    else
        if (prepare_stmt_wrapper(ctx->db, &ctx->score_add, cstr_view(
            "INSERT OR IGNORE INTO scores (game_id, team_id, score) VALUES (?, ?, ?);")) != 0)
            return -1;

    if ((ret = sqlite3_bind_int(ctx->score_add, 1, game_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->score_add, 2, team_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->score_add, 3, score) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->score_add);
}

static int
frame_add(
        struct db* ctx,
        int game_id,
        int slot,
        uint64_t time_stamp,
        int frame_number,
        int frames_left,
        float posx,
        float posy,
        float damage,
        float hitstun,
        float shield,
        int status_id,
        int hit_status_id,
        uint64_t hash40,
        int stocks,
        int attack_connected,
        int facing_left,
        int opponent_in_hitlag)
{
    int ret;
    if (ctx->frame_add)
        sqlite3_reset(ctx->frame_add);
    else if (prepare_stmt_wrapper(ctx->db, &ctx->frame_add, cstr_view(
        "INSERT OR IGNORE INTO frames ("
        "    game_id, slot, time_stamp, frame_number, frames_left, "
        "    posx, posy, damage, hitstun, shield, status_id, "
        "    hit_status_id, hash40, stocks, attack_connected, facing_left, "
        "    opponent_in_hitlag) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);")) != 0)
        return -1;

    if ((ret = sqlite3_bind_int(ctx->frame_add, 1, game_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 2, slot) != SQLITE_OK) ||
        (ret = sqlite3_bind_int64(ctx->frame_add, 3, (int64_t)time_stamp) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 4, frame_number) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 5, frames_left) != SQLITE_OK) ||
        (ret = sqlite3_bind_double(ctx->frame_add, 6, posx) != SQLITE_OK) ||
        (ret = sqlite3_bind_double(ctx->frame_add, 7, posy) != SQLITE_OK) ||
        (ret = sqlite3_bind_double(ctx->frame_add, 8, damage) != SQLITE_OK) ||
        (ret = sqlite3_bind_double(ctx->frame_add, 9, hitstun) != SQLITE_OK) ||
        (ret = sqlite3_bind_double(ctx->frame_add, 10, shield) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 11, status_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 12, hit_status_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int64(ctx->frame_add, 13, (int64_t)hash40) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 14, stocks) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 15, attack_connected) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 16, facing_left) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->frame_add, 17, opponent_in_hitlag) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->frame_add);
}

static int
query_games(struct db* ctx,
    int (*on_game)(
        int game_id,
        const char* tournament,
        const char* event,
        uint64_t time_started,
        int duration,
        const char* round_type,
        int round_number,
        const char* format,
        const char* stage,
        void* user),
    void* user)
{
    int ret;
    if (ctx->query_games)
        sqlite3_reset(ctx->query_games);
    else if (prepare_stmt_wrapper(ctx->db, &ctx->query_games, cstr_view(
        "SELECT "
        "    games.id, "
        "    tournaments.name tournament, "
        "    event_types.name event, "
        "    time_started, "
        "    time_ended - time_started duration, "
        "    round_types.short_name round, "
        "    rounds.number number, "
        "    set_formats.short_name format, "
        "    stages.name stage "
        "FROM games "
        "JOIN events ON games.event_id = events.id "
        "JOIN event_types ON events.event_type_id = event_types.id "
        "JOIN rounds ON games.round_id = rounds.id "
        "JOIN round_types ON rounds.round_type_id = round_types.id "
        "JOIN set_formats ON games.set_format_id = set_formats.id "
        "JOIN stages ON stages.id = games.stage_id "
        "LEFT JOIN tournaments ON games.tournament_id = tournaments.id;"
        )) != 0)
        return -1;

next_step:
    ret = sqlite3_step(ctx->query_games);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            ret = on_game(
                sqlite3_column_int(ctx->query_games, 0),
                (const char*)sqlite3_column_text(ctx->query_games, 1),
                (const char*)sqlite3_column_text(ctx->query_games, 2),
                (uint64_t)sqlite3_column_int64(ctx->query_games, 3),
                sqlite3_column_int(ctx->query_games, 4),
                (const char*)sqlite3_column_text(ctx->query_games, 5),
                sqlite3_column_int(ctx->query_games, 6),
                (const char*)sqlite3_column_text(ctx->query_games, 7),
                (const char*)sqlite3_column_text(ctx->query_games, 8),
                user);
            if (ret) return ret;
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return 0;
}

static int
query_game_teams(struct db* ctx, int game_id,
    int (*on_game_team)(
        int game_id,
        int team_id,
        const char* team,
        int score,
        void* user),
    void* user)
{
    int ret;
    if (ctx->query_game_teams)
        sqlite3_reset(ctx->query_game_teams);
    else if (prepare_stmt_wrapper(ctx->db, &ctx->query_game_teams, cstr_view(
        "SELECT "
        "    game_players.team_id, "
        "    teams.name team, "
        "    score "
        "FROM game_players "
        "JOIN teams ON teams.id = game_players.team_id "
        "JOIN scores ON scores.team_id = game_players.team_id "
        "WHERE game_players.game_id = ? "
        "GROUP BY game_players.team_id "
        "ORDER BY game_players.team_id;"
        )) != 0)
        return -1;

    if ((ret = sqlite3_bind_int(ctx->query_game_teams, 1, game_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->query_game_teams);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            ret = on_game_team(
                game_id,
                sqlite3_column_int(ctx->query_game_teams, 0),
                (const char*)sqlite3_column_text(ctx->query_game_teams, 1),
                sqlite3_column_int(ctx->query_game_teams, 2),
                user);
            if (ret) return ret;
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return 0;
}

static int
query_game_players(struct db* ctx, int game_id, int team_id,
    int (*on_game_player)(
        int slot,
        const char* sponsor,
        const char* name,
        const char* fighter,
        int costume,
        void* user),
    void* user)
{
    int ret;
    if (ctx->query_game_players)
        sqlite3_reset(ctx->query_game_players);
    else if (prepare_stmt_wrapper(ctx->db, &ctx->query_game_players, cstr_view(
        "SELECT "
        "    game_players.slot, "
        "    sponsors.short_name sponsor, "
        "    people.name name, "
        "    fighters.name fighter, "
        "    costume "
        "FROM game_players "
        "JOIN people ON people.id = game_players.person_id "
        "JOIN fighters ON fighters.id = game_players.fighter_id "
        "LEFT JOIN sponsors ON people.sponsor_id = sponsors.id "
        "WHERE game_players.game_id = ? AND game_players.team_id = ? "
        "ORDER BY game_players.slot;"
        )) != 0)
        return -1;

    if ((ret = sqlite3_bind_int(ctx->query_game_players, 1, game_id) != SQLITE_OK) ||
        (ret = sqlite3_bind_int(ctx->query_game_players, 2, team_id) != SQLITE_OK))
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->query_game_players);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            ret = on_game_player(
                sqlite3_column_int(ctx->query_game_players, 0),
                (const char*)sqlite3_column_text(ctx->query_game_players, 1),
                (const char*)sqlite3_column_text(ctx->query_game_players, 2),
                (const char*)sqlite3_column_text(ctx->query_game_players, 3),
                sqlite3_column_int(ctx->query_game_players, 4),
                user);
            if (ret) return ret;
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return 0;
}

struct db_interface db_sqlite = {
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
    fighter_name,
    stage_add,
    status_enum_add,
    hit_status_enum_add,

    tournament_add_or_get,
    tournament_sponsor_add,
    tournament_organizer_add,
    tournament_commentator_add,

    event_type_add_or_get,
    event_add_or_get,

    round_type_add_or_get,
    round_add_or_get,

    set_format_add_or_get,

    team_add_or_get,
    team_member_add,

    sponsor_add_or_get,
    person_add_or_get,
    person_get_id,
    person_get_team_id,

    game_add_or_get,
    game_player_add,

    score_add,

    frame_add,

    query_games,
    query_game_teams,
    query_game_players
};

struct db_interface* db(const char* type)
{
    if (strcmp("sqlite", type) == 0)
        return &db_sqlite;
    return NULL;
}
