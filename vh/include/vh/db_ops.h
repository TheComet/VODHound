#pragma once

#include "vh/config.h"
#include "vh/str.h"

C_BEGIN

struct db;

struct db_interface
{
    struct db* (*open_and_prepare)(const char* uri);
    void (*close)(struct db* db);

    int (*transaction_begin)(struct db* db);
    int (*transaction_commit)(struct db* db);
    int (*transaction_rollback)(struct db* db);
    int (*transaction_begin_nested)(struct db* db, struct str_view name);
    int (*transaction_commit_nested)(struct db* db, struct str_view name);
    int (*transaction_rollback_nested)(struct db* db, struct str_view name);

    /* Static tables */
    int (*motion_add)(struct db* db, uint64_t hash40, struct str_view string);
    int (*fighter_add)(struct db* db, int fighter_id, struct str_view name);
    int (*stage_add)(struct db* db, int stage_id, struct str_view name);
    int (*status_enum_add)(struct db* db, int fighter_id, int status_id, struct str_view name);
    int (*hit_status_enum_add)(struct db* db, int hit_status_id, struct str_view name);

    int (*tournament_add_or_get)(struct db* db, struct str_view name, struct str_view website);
    int (*tournament_sponsor_add)(struct db* db, int tournament_id, int sponsor_id);
    int (*tournament_organizer_add)(struct db* db, int tournament_id, int person_id);
    int (*tournament_commentator_add)(struct db* db, int tournament_id, int person_id);

    int (*bracket_type_add_or_get)(struct db* db, struct str_view name);
    int (*bracket_add_or_get)(struct db* db, int bracket_type_id, struct str_view url);

    int (*round_type_add_or_get)(struct db* db, struct str_view short_name, struct str_view long_name);
    int (*round_add_or_get)(struct db* db, struct str_view name);

    int (*sponsor_add_or_get)(struct db* db, struct str_view short_name, struct str_view full_name, struct str_view website);
    int (*person_add_or_get)(struct db* db, int sponsor_id, struct str_view name, struct str_view tag, struct str_view social, struct str_view pronouns);
};

VH_PUBLIC_API struct db_interface*
db(const char* type);

C_END
