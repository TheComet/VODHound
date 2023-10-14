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
    const char* (*fighter_name)(struct db* db, int fighter_id);
    int (*stage_add)(struct db* db, int stage_id, struct str_view name);
    int (*status_enum_add)(struct db* db, int fighter_id, int status_id, struct str_view name);
    int (*hit_status_enum_add)(struct db* db, int hit_status_id, struct str_view name);

    int (*tournament_add_or_get)(struct db* db, struct str_view name, struct str_view website);
    int (*tournament_sponsor_add)(struct db* db, int tournament_id, int sponsor_id);
    int (*tournament_organizer_add)(struct db* db, int tournament_id, int person_id);
    int (*tournament_commentator_add)(struct db* db, int tournament_id, int person_id);

    int (*event_type_add_or_get)(struct db* db, struct str_view name);
    int (*event_add_or_get)(struct db* db, int event_type_id, struct str_view url);

    int (*round_type_add_or_get)(struct db* db, struct str_view short_name, struct str_view long_name);
    int (*round_add_or_get)(struct db* db, int round_type_id, int number);

    int (*set_format_add_or_get)(struct db* db, struct str_view short_name, struct str_view long_name);

    int (*team_add_or_get)(struct db* db, struct str_view name, struct str_view url);
    int (*team_member_add)(struct db* db, int team_id, int person_id);

    int (*sponsor_add_or_get)(struct db* db, struct str_view short_name, struct str_view full_name, struct str_view website);
    int (*person_add_or_get)(struct db* db, int sponsor_id, struct str_view name, struct str_view tag, struct str_view social, struct str_view pronouns);
    int (*person_get_id)(struct db* db, struct str_view name);
    int (*person_get_team_id)(struct db* db, struct str_view name);

    int (*game_add_or_get)(struct db* db, int video_id, int tournament_id, int event_id, int round_id, int set_format_id, int winner_team_id, int stage_id, uint64_t time_started, uint64_t time_ended);
    int (*game_player_add)(struct db* db, int person_id, int game_id, int slot, int team_id, int fighter_id, int costume, int is_loser_side);

    int (*score_add)(struct db* db, int game_id, int team_id, int score);

    int (*frame_add)(struct db* db, int game_id, int slot, uint64_t time_stamp, int frame_number, int frames_left, float posx, float posy, float damage, float hitstun, float shield, int status_id, int hit_status_id, uint64_t hash40, int stocks, int attack_connected, int facing_left, int opponent_in_hitlag);

    int (*query_games)(struct db* db,
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
        void* user);
    int (*query_game_teams)(struct db* db, int game_id,
        int (*on_game_team)(
            int game_id,
            int team_id,
            const char* team,
            int score,
            void* user),
        void* user);
    int (*query_game_players)(struct db* db, int game_id, int team_id,
        int (*on_game_player)(
            int slot,
            const char* sponsor,
            const char* name,
            const char* fighter,
            int costume,
            void* user),
        void* user);
};

VH_PUBLIC_API struct db_interface*
db(const char* type);

C_END
