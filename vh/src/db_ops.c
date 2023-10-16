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

#define STMT_LIST                  \
    X(motion_add)                  \
    X(fighter_add)                 \
    X(fighter_name)                \
    X(stage_add)                   \
    X(status_enum_add)             \
    X(hit_status_enum_add)         \
                                   \
    X(tournament_add_or_get)       \
    X(tournament_sponsor_add)      \
    X(tournament_organizer_add)    \
    X(tournament_commentator_add)  \
                                   \
    X(event_type_add_or_get)       \
    X(event_add_or_get)            \
                                   \
    X(round_type_add_or_get)       \
                                   \
    X(set_format_add_or_get)       \
                                   \
    X(team_member_add)             \
    X(team_add_or_get)             \
                                   \
    X(sponsor_add_or_get)          \
    X(person_add_or_get)           \
    X(person_get_id)               \
    X(person_get_team_id)          \
                                   \
    X(game_add_or_get)             \
    X(games_query)                 \
    X(game_associate_tournament)   \
    X(game_associate_event)        \
    X(game_associate_video)        \
    X(game_unassociate_video)      \
    X(game_get_video)              \
    X(game_player_add)             \
                                   \
    X(video_path_add)              \
    X(video_paths_query)           \
    X(video_add_or_get)            \
    X(video_set_path_hint)         \
                                   \
    X(score_add)                   \
                                   \
    X(frame_add)

#define STMT_PREPARE_OR_RESET(stmt, error_return, text)                       \
    if (ctx->stmt)                                                            \
        sqlite3_reset(ctx->stmt);                                             \
    else if (prepare_stmt_wrapper(ctx->db, &ctx->stmt, cstr_view(text)) != 0) \
        return error_return

struct db
{
    sqlite3* db;

#define X(stmt) sqlite3_stmt* stmt;
    STMT_LIST
#undef X
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

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s': %s\n", file_name, mfile_last_error());
        mfile_last_error_free();
        goto open_script_failed;
    }
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

    migrate_db_failed             :
    open_db_failed                : mem_free(ctx);
    oom                           : return NULL;
}

static void
close_db(struct db* ctx)
{
#define X(stmt) sqlite3_finalize(ctx->stmt);
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
    STMT_PREPARE_OR_RESET(motion_add, -1, "INSERT OR IGNORE INTO motions (hash40, string) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int64(ctx->motion_add, 1, (int64_t)hash40)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->motion_add, 2, string.data, string.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(fighter_add, -1, "INSERT OR IGNORE INTO fighters (id, name) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int(ctx->fighter_add, 1, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->fighter_add, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(fighter_name, NULL, "SELECT name FROM fighters WHERE id=?;");

    if ((ret = sqlite3_bind_int(ctx->fighter_name, 1, fighter_id)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(stage_add, -1, "INSERT OR IGNORE INTO stages (id, name) VALUES (?, ?);");

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
    STMT_PREPARE_OR_RESET(status_enum_add, -1,
        "INSERT OR IGNORE INTO status_enums (fighter_id, value, name) VALUES (?, ?, ?);");

    if (fighter_id == -1)
    {
        if ((ret = sqlite3_bind_null(ctx->status_enum_add, 1)) != SQLITE_OK)
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->status_enum_add, 1, fighter_id)) != SQLITE_OK)
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_int(ctx->status_enum_add, 2, status_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->status_enum_add, 3, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(hit_status_enum_add, -1,
        "INSERT OR IGNORE INTO hit_status_enums (id, name) VALUES (?, ?);");

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
    STMT_PREPARE_OR_RESET(tournament_add_or_get, -1,
        "INSERT INTO tournaments (name, website) VALUES (?, ?) "
        "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->tournament_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->tournament_add_or_get, 2, website.data, website.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(tournament_sponsor_add, -1,
        "INSERT OR IGNORE INTO tournament_sponsors (tournament_id, sponsor_id) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_sponsor_add, 2, sponsor_id)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(tournament_organizer_add, -1,
        "INSERT OR IGNORE INTO tournament_organizers (tournament_id, person_id) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int(ctx->tournament_organizer_add, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_organizer_add, 2, person_id)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(tournament_commentator_add, -1,
        "INSERT OR IGNORE INTO tournament_commentators (tournament_id, person_id) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int(ctx->tournament_commentator_add, 1, tournament_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->tournament_commentator_add, 2, person_id)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(event_type_add_or_get, -1,
        "INSERT INTO event_types (name) VALUES (?) "
        "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->event_type_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(event_add_or_get, -1,
        "INSERT INTO events (event_type_id, url) VALUES (?, ?) "
        "ON CONFLICT DO UPDATE SET event_type_id=excluded.event_type_id RETURNING id;");

    if ((ret = sqlite3_bind_int(ctx->event_add_or_get, 1, event_type_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->event_add_or_get, 2, url.data, url.len, SQLITE_STATIC)) != SQLITE_OK)
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
set_format_add_or_get(struct db* ctx, struct str_view short_name, struct str_view long_name)
{
    int ret, set_format_id = -1;
    STMT_PREPARE_OR_RESET(set_format_add_or_get, -1,
        "INSERT INTO set_formats (short_name, long_name) VALUES (?, ?) "
        "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->set_format_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->set_format_add_or_get, 2, long_name.data, long_name.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(team_add_or_get, -1,
        "INSERT INTO teams (name, url) VALUES (?, ?) "
        "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->team_add_or_get, 1, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->team_add_or_get, 2, url.data, url.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(team_member_add, -1,
        "INSERT OR IGNORE INTO team_members (team_id, person_id) VALUES (?, ?);");

    if ((ret = sqlite3_bind_int(ctx->team_member_add, 1, team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->team_member_add, 2, person_id)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(sponsor_add_or_get, -1,
        "INSERT INTO sponsors (short_name, full_name, website) VALUES (?, ?, ?) "
        "ON CONFLICT DO UPDATE SET short_name=excluded.short_name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 1, short_name.data, short_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 2, full_name.data, full_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->sponsor_add_or_get, 3, website.data, website.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(person_add_or_get, -1,
        "INSERT INTO people (sponsor_id, name, tag, social, pronouns) VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT DO UPDATE SET name=excluded.name RETURNING id;");

    if (sponsor_id < 0)
    {
        if ((ret = sqlite3_bind_null(ctx->person_add_or_get, 1)) != SQLITE_OK)
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }
    else
    {
        if ((ret = sqlite3_bind_int(ctx->person_add_or_get, 1, sponsor_id)) != SQLITE_OK)
        {
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            return -1;
        }
    }

    if ((ret = sqlite3_bind_text(ctx->person_add_or_get, 2, name.data, name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 3, tag.data, tag.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 4, social.data, social.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->person_add_or_get, 5, pronouns.data, pronouns.len, SQLITE_STATIC)) != SQLITE_OK)
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
    STMT_PREPARE_OR_RESET(person_get_id, -1,
        "SELECT id FROM people WHERE name=?;");

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
    STMT_PREPARE_OR_RESET(person_get_team_id, -1,
        "SELECT team_id "
        "FROM team_members "
        "JOIN people ON (team_members.person_id=people.id)"
        "WHERE name=?;");

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
        int round_type_id,
        int round_number,
        int set_format_id,
        int winner_team_id,
        int stage_id,
        uint64_t time_started,
        uint64_t time_ended)
{
    int ret, game_id = -1;
    STMT_PREPARE_OR_RESET(game_add_or_get, -1,
        "INSERT INTO games (round_type_id, round_number, set_format_id, winner_team_id, stage_id, time_started, time_ended) VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT DO UPDATE SET stage_id=excluded.stage_id RETURNING id;");

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
        (ret = sqlite3_bind_int64(ctx->game_add_or_get, 7, (int64_t)time_ended)) != SQLITE_OK)
    {
        goto error;
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

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    return -1;
}

static int
game_associate_tournament(struct db* ctx, int game_id, int tournament_id)
{
    int ret;
    STMT_PREPARE_OR_RESET(game_associate_tournament, -1,
        "INSERT OR IGNORE INTO tournament_games (game_id, tournament_id) VALUES (?, ?);");

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
    STMT_PREPARE_OR_RESET(game_associate_event, -1,
        "INSERT OR IGNORE INTO event_games (game_id, event_id) VALUES (?, ?);");

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
    STMT_PREPARE_OR_RESET(game_associate_video, -1,
        "INSERT OR IGNORE INTO game_videos (game_id, video_id, frame_offset) VALUES (?, ?, ?);");

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
    STMT_PREPARE_OR_RESET(game_unassociate_video, -1,
        "DELETE FROM game_videos WHERE game_id = ? AND video_id = ?;");

    if ((ret = sqlite3_bind_int(ctx->game_unassociate_video, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_unassociate_video, 2, video_id)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_unassociate_video);
}

static int
game_get_video(struct db* ctx, int game_id, const char** file_name, const char** path_hint, int64_t* frame_offset)
{
    int ret;
    STMT_PREPARE_OR_RESET(game_get_video, -1,
        "SELECT file_name, path_hint, frame_offset FROM game_videos JOIN videos ON game_videos.video_id = videos.id WHERE game_videos.game_id = ?;");

    if ((ret = sqlite3_bind_int(ctx->game_get_video, 1, game_id)) != SQLITE_OK)
        goto error;

    if ((ret = sqlite3_step(ctx->game_get_video)) != SQLITE_ROW)
        return 0;

    *file_name = (const char*)sqlite3_column_text(ctx->game_get_video, 0);
    *path_hint = (const char*)sqlite3_column_text(ctx->game_get_video, 1);
    *frame_offset = sqlite3_column_int64(ctx->game_get_video, 2);
    return 1;

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    return -1;
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
    STMT_PREPARE_OR_RESET(game_player_add, -1,
        "INSERT OR IGNORE INTO game_players (person_id, game_id, slot, team_id, fighter_id, costume, is_loser_side) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);");

    if ((ret = sqlite3_bind_int(ctx->game_player_add, 1, person_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 2, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 3, slot)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 4, team_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 5, fighter_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 6, costume)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->game_player_add, 7, is_loser_side)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->game_player_add);
}

static int
score_add(struct db* ctx, int game_id, int team_id, int score)
{
    int ret;
    STMT_PREPARE_OR_RESET(score_add, -1,
        "INSERT OR IGNORE INTO scores (game_id, team_id, score) VALUES (?, ?, ?);");

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
    STMT_PREPARE_OR_RESET(frame_add, -1,
        "INSERT OR IGNORE INTO frames ("
        "    game_id, slot, time_stamp, frame_number, frames_left, "
        "    posx, posy, damage, hitstun, shield, status_id, "
        "    hit_status_id, hash40, stocks, attack_connected, facing_left, "
        "    opponent_in_hitlag) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");

    if ((ret = sqlite3_bind_int(ctx->frame_add, 1, game_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 2, slot)) != SQLITE_OK ||
        (ret = sqlite3_bind_int64(ctx->frame_add, 3, (int64_t)time_stamp)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 4, frame_number)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 5, frames_left)) != SQLITE_OK ||
        (ret = sqlite3_bind_double(ctx->frame_add, 6, posx)) != SQLITE_OK ||
        (ret = sqlite3_bind_double(ctx->frame_add, 7, posy)) != SQLITE_OK ||
        (ret = sqlite3_bind_double(ctx->frame_add, 8, damage)) != SQLITE_OK ||
        (ret = sqlite3_bind_double(ctx->frame_add, 9, hitstun)) != SQLITE_OK ||
        (ret = sqlite3_bind_double(ctx->frame_add, 10, shield)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 11, status_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 12, hit_status_id)) != SQLITE_OK ||
        (ret = sqlite3_bind_int64(ctx->frame_add, 13, (int64_t)hash40)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 14, stocks)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 15, attack_connected)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 16, facing_left)) != SQLITE_OK ||
        (ret = sqlite3_bind_int(ctx->frame_add, 17, opponent_in_hitlag)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->frame_add);
}

static int
video_path_add(struct db* ctx, struct str_view path)
{
    int ret;
    STMT_PREPARE_OR_RESET(video_path_add, -1,
        "INSERT OR IGNORE INTO video_paths (path) VALUES (?);");

    if ((ret = sqlite3_bind_text(ctx->video_path_add, 1, path.data, path.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->video_path_add);
}

static int
video_paths_query(struct db* ctx, int (*on_video_path)(const char* path, void* user), void* user)
{
    int ret;
    STMT_PREPARE_OR_RESET(video_paths_query, -1,
        "SELECT path FROM video_paths;");

next_step:
    ret = sqlite3_step(ctx->video_paths_query);
    switch (ret)
    {
    case SQLITE_BUSY: goto next_step;
    case SQLITE_DONE: return 0;
    case SQLITE_ROW:
        ret = on_video_path((const char*)sqlite3_column_text(ctx->video_paths_query, 0), user);
        if (ret) return ret;
        goto next_step;
    default: goto error;
    }

error:
    log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
    return -1;
}

static int
video_add_or_get(struct db* ctx, struct str_view file_name, struct str_view path_hint)
{
    int ret, video_id = -1;
    STMT_PREPARE_OR_RESET(video_add_or_get, -1,
        "INSERT INTO videos (file_name, path_hint) VALUES (?, ?) "
        "ON CONFLICT DO UPDATE SET file_name=excluded.file_name RETURNING id;");

    if ((ret = sqlite3_bind_text(ctx->video_add_or_get, 1, file_name.data, file_name.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->video_add_or_get, 2, path_hint.data, path_hint.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

next_step:
    ret = sqlite3_step(ctx->video_add_or_get);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            video_id = sqlite3_column_int(ctx->video_add_or_get, 0);
            goto next_step;
        default:
            log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
            break;
    }

    return video_id;
}

static int
video_set_path_hint(struct db* ctx, struct str_view file_name, struct str_view path_hint)
{
    int ret;
    STMT_PREPARE_OR_RESET(video_set_path_hint, -1,
        "UPDATE videos SET path_hint = ? WHERE file_name = ?;");

    if ((ret = sqlite3_bind_text(ctx->video_set_path_hint, 1, path_hint.data, path_hint.len, SQLITE_STATIC)) != SQLITE_OK ||
        (ret = sqlite3_bind_text(ctx->video_set_path_hint, 2, file_name.data, file_name.len, SQLITE_STATIC)) != SQLITE_OK)
    {
        log_sqlite_err(ret, sqlite3_errstr(ret), sqlite3_errmsg(ctx->db));
        return -1;
    }

    return step_stmt_wrapper(ctx->db, ctx->video_set_path_hint);
}

static int
games_query(struct db* ctx,
    int (*on_game)(
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
        void* user),
    void* user)
{
    int ret;
    STMT_PREPARE_OR_RESET(games_query, -1,
        "WITH grouped_games AS ( "
        "    SELECT "
        "        games.id, "
        "        time_started, "
        "        time_ended, "
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
        "    JOIN games ON games.id = game_players.game_id "
        "    JOIN teams ON teams.id = game_players.team_id "
        "    LEFT JOIN scores ON scores.team_id = game_players.team_id AND scores.game_id = game_players.game_id "
        "    JOIN people ON people.id = game_players.person_id "
        "    JOIN fighters ON fighters.id = game_players.fighter_id "
        "    LEFT JOIN sponsors ON sponsors.id = people.sponsor_id "
        "    GROUP BY games.id, game_players.team_id "
        "    ORDER BY game_players.slot) "
        "SELECT "
        "    grouped_games.id, "
        "    time_started, "
        "    time_ended, "
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
        "JOIN set_formats ON grouped_games.set_format_id = set_formats.id "
        "GROUP BY grouped_games.id "
        "ORDER BY time_started;");

next_step:
    ret = sqlite3_step(ctx->games_query);
    switch (ret)
    {
        case SQLITE_BUSY: goto next_step;
        case SQLITE_DONE: break;
        case SQLITE_ROW:
            ret = on_game(
                sqlite3_column_int(ctx->games_query, 0),
                (uint64_t)sqlite3_column_int64(ctx->games_query, 1),
                (uint64_t)sqlite3_column_int64(ctx->games_query, 2),
                (const char*)sqlite3_column_text(ctx->games_query, 3),
                (const char*)sqlite3_column_text(ctx->games_query, 4),
                (const char*)sqlite3_column_text(ctx->games_query, 5),
                (const char*)sqlite3_column_text(ctx->games_query, 6),
                (const char*)sqlite3_column_text(ctx->games_query, 7),
                (const char*)sqlite3_column_text(ctx->games_query, 8),
                (const char*)sqlite3_column_text(ctx->games_query, 9),
                (const char*)sqlite3_column_text(ctx->games_query, 10),
                (const char*)sqlite3_column_text(ctx->games_query, 11),
                (const char*)sqlite3_column_text(ctx->games_query, 12),
                (const char*)sqlite3_column_text(ctx->games_query, 13),
                (const char*)sqlite3_column_text(ctx->games_query, 14),
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

#define X(stmt) stmt,
    STMT_LIST
#undef X
};

struct db_interface* db(const char* type)
{
    if (strcmp("sqlite", type) == 0)
        return &db_sqlite;
    return NULL;
}
