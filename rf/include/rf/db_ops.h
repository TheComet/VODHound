#pragma once

#include "rf/config.h"
#include "rf/str.h"

C_BEGIN

struct rf_db;

struct rf_db_interface
{
    struct rf_db* (*open_and_prepare)(const char* uri);
    void (*close)(struct rf_db* db);

    int (*transaction_begin)(struct rf_db* db);
    int (*transaction_commit)(struct rf_db* db);
    int (*transaction_rollback)(struct rf_db* db);
    int (*transaction_begin_nested)(struct rf_db* db, struct rf_str_view name);
    int (*transaction_commit_nested)(struct rf_db* db, struct rf_str_view name);
    int (*transaction_rollback_nested)(struct rf_db* db, struct rf_str_view name);

    /* Static tables */
    int (*motion_add)(struct rf_db* db, uint64_t hash40, struct rf_str_view string);
    int (*fighter_add)(struct rf_db* db, int fighter_id, struct rf_str_view name);
    int (*stage_add)(struct rf_db* db, int stage_id, struct rf_str_view name);
    int (*status_enum_add)(struct rf_db* db, int fighter_id, int status_id, struct rf_str_view name);
    int (*hit_status_enum_add)(struct rf_db* db, int hit_status_id, struct rf_str_view name);

    int (*tournament_add_or_get)(struct rf_db* db, struct rf_str_view name, struct rf_str_view website);
    int (*sponsor_add_or_get)(struct rf_db* db, struct rf_str_view short_name, struct rf_str_view full_name, struct rf_str_view website);
    int (*tournament_sponsor_add)(struct rf_db* db, int tournament_id, int sponsor_id);
    int (*tournament_organizer_add)(struct rf_db* db, int tournament_id, int person_id);
    int (*tournament_commentator_add)(struct rf_db* db, int tournament_id, int person_id);

    int (*person_add_or_get)(struct rf_db* db, int sponsor_id, struct rf_str_view name, struct rf_str_view tag, struct rf_str_view social, struct rf_str_view pronouns);
};

RF_PUBLIC_API struct rf_db_interface*
rf_db(const char* type);

C_END
