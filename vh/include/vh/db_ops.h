#pragma once

#include "vh/config.h"
#include "vh/fs.h"

C_BEGIN

struct db;

struct db_interface
{
    struct db* (*open_and_prepare)(const char* uri, int reinit_db);
    void (*close)(struct db* db);

    struct {
        int (*begin)(struct db* db);
        int (*commit)(struct db* db);
        int (*rollback)(struct db* db);
        int (*begin_nested)(struct db* db, struct str_view name);
        int (*commit_nested)(struct db* db, struct str_view name);
        int (*rollback_nested)(struct db* db, struct str_view name);
    } transaction;

    /* Static tables */
    struct {
        int (*add)(struct db* db, uint64_t hash40, struct str_view string);
    } motion;

    struct {
        int (*add)(struct db* db, int fighter_id, struct str_view name);
        const char* (*name)(struct db* db, int fighter_id);
    } fighter;

    struct {
        int (*add)(struct db* db, int stage_id, struct str_view name);
    } stage;

    struct {
        int (*add)(struct db* db, int fighter_id, int status_id, struct str_view name);
    } status_enum;

    struct {
        int (*add)(struct db* db, int hit_status_id, struct str_view name);
    } hit_status_enum;

    struct {
        int (*add_or_get)(struct db* db, struct str_view name, struct str_view website);
        int (*add_sponsor)(struct db* db, int tournament_id, int sponsor_id);
        int (*add_organizer)(struct db* db, int tournament_id, int person_id);
        int (*add_commentator)(struct db* db, int tournament_id, int person_id);
    } tournament;

    struct {
        int (*add_or_get_type)(struct db* db, struct str_view name);
        int (*add_or_get)(struct db* db, int event_type_id, struct str_view url);
    } event;

    struct {
        int (*add_or_get)(struct db* db, struct str_view short_name, struct str_view long_name);
    } round_type;

    struct {
        int (*add_or_get)(struct db* db, struct str_view short_name, struct str_view long_name);
    } set_format;

    struct {
        int (*add_or_get)(struct db* db, struct str_view name, struct str_view url);
        int (*add_member)(struct db* db, int team_id, int person_id);
    } team;

    struct {
        int (*add_or_get)(struct db* db, struct str_view short_name, struct str_view full_name, struct str_view website);
    } sponsor;

    struct {
        int (*add_or_get)(struct db* db, int sponsor_id, struct str_view name, struct str_view tag, struct str_view social, struct str_view pronouns);
        int (*get_id)(struct db* db, struct str_view name);
        int (*get_team_id)(struct db* db, struct str_view name);
    } person;

    struct game {
        int (*add_or_get)(struct db* db, int round_type_id, int round_number, int set_format_id, int winner_team_id, int stage_id, uint64_t time_started, uint64_t time_ended);
        int (*query)(struct db* db,
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
            void* user);
        int (*associate_tournament)(struct db* db, int game_id, int tournament_id);
        int (*associate_event)(struct db* db, int game_id, int event_id);
        int (*associate_video)(struct db* db, int game_id, int video_id, int64_t frame_offset);
        int (*unassociate_video)(struct db* db, int game_id, int video_id);
        int (*get_video)(struct db* db, int game_id, const char** file_name, const char** path_hint, int64_t* frame_offset);
        int (*player_add)(struct db* db, int person_id, int game_id, int slot, int team_id, int fighter_id, int costume, int is_loser_side);
    } game;

    struct {
        int (*add_or_get)(struct db* db, struct str_view name);
        int (*add_game)(struct db* db, int group_id, int game_id);
    } group;

    struct {
        int (*add)(struct db* db, struct str_view path);
        int (*paths_query)(struct db* db, int (*on_video_path)(const char* path, void* user), void* user);
    } video_path;

    struct {
        int (*add_or_get)(struct db* db, struct str_view file_name, struct str_view path_hint);
        int (*set_path_hint)(struct db* db, struct str_view file_name, struct str_view path_hint);
    } video;

    struct {
        int (*add)(struct db* db, int game_id, int team_id, int score);
    } score;

    struct {
        int (*add)(struct db* db, int game_id, int slot, uint64_t time_stamp, int frame_number, int frames_left, float posx, float posy, float damage, float hitstun, float shield, int status_id, int hit_status_id, uint64_t hash40, int stocks, int attack_connected, int facing_left, int opponent_in_hitlag);
    } frame;

    struct {
        int (*add)(struct db* db, struct str_view name, struct str_view ip, uint16_t port);
    } switch_info;

    struct {
        int (*add)(struct db* db, struct str_view path, int frame_offset);
    } stream_recording_sources;
};

VH_PUBLIC_API struct db_interface*
db(const char* type);

C_END
