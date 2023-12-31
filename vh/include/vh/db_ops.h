#pragma once

#include "vh/config.h"
#include "vh/fs.h"

C_BEGIN

struct olddb;
struct vec;

struct olddb_interface
{
    struct olddb* (*open_and_prepare)(const char* uri, int reinit_db);
    void (*close)(struct olddb* db);

    struct {
        int (*begin)(struct olddb* db);
        int (*commit)(struct olddb* db);
        int (*rollback)(struct olddb* db);
        int (*begin_nested)(struct olddb* db, struct str_view name);
        int (*commit_nested)(struct olddb* db, struct str_view name);
        int (*rollback_nested)(struct olddb* db, struct str_view name);
    } transaction;

    struct {
        int (*add)(struct olddb* db, uint64_t hash40, struct str_view string);
        int (*exists)(struct olddb* db, uint64_t hash40);
    } motion;

    struct {
        int (*add_or_get_group)(struct olddb* db, struct str_view name);
        int (*add_or_get_layer)(struct olddb* db, int group_id, struct str_view name);
        int (*add_or_get_category)(struct olddb* db, struct str_view name);
        int (*add_or_get_usage)(struct olddb* db, struct str_view name);
        int (*add_or_get_label)(struct olddb* db, uint64_t motion, int fighter_id, int layer_id, int category_id, int usage_id, struct str_view name);
        int (*to_motions)(struct olddb* db, int fighter_id, struct str_view label, struct vec* motions_out);
        int (*to_notation_label)(struct olddb* db, int fighter_id, uint64_t motion, struct str* label);
    } motion_label;

    struct {
        int (*add)(struct olddb* db, int fighter_id, struct str_view name);
        int (*get_name)(struct olddb* db, int fighter_id, struct str* name);
    } fighter;

    struct {
        int (*add)(struct olddb* db, int stage_id, struct str_view name);
    } stage;

    struct {
        int (*add)(struct olddb* db, int fighter_id, int status_id, struct str_view name);
    } status_enum;

    struct {
        int (*add)(struct olddb* db, int hit_status_id, struct str_view name);
    } hit_status_enum;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view name, struct str_view website);
        int (*add_sponsor)(struct olddb* db, int tournament_id, int sponsor_id);
        int (*add_organizer)(struct olddb* db, int tournament_id, int person_id);
        int (*add_commentator)(struct olddb* db, int tournament_id, int person_id);
    } tournament;

    struct {
        int (*add_or_get_type)(struct olddb* db, struct str_view name);
        int (*add_or_get)(struct olddb* db, int event_type_id, struct str_view url);
    } event;

    struct {
        int (*add_or_get_type)(struct olddb* db, struct str_view short_name, struct str_view long_name);
    } round;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view short_name, struct str_view long_name);
    } set_format;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view name, struct str_view url);
        int (*add_member)(struct olddb* db, int team_id, int person_id);
    } team;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view short_name, struct str_view full_name, struct str_view website);
    } sponsor;

    struct {
        int (*add_or_get)(struct olddb* db,
            int sponsor_id, struct str_view name, struct str_view tag, struct str_view social, struct str_view pronouns,
            int (*on_person)(
                int id, int sponsor_id, const char* name, const char* tag, const char* social, const char* pronouns,
                void* user),
            void* user);
        int (*get_id_from_name)(struct olddb* db, struct str_view name);
        int (*get_team_id_from_name)(struct olddb* db, struct str_view name);
        int (*set_tag)(struct olddb* db, int person_id, struct str_view tag);
        int (*set_social)(struct olddb* db, int person_id, struct str_view social);
        int (*set_pronouns)(struct olddb* db, int person_id, struct str_view pronouns);
    } person;

    struct game {
        int (*add_or_get)(struct olddb* db, int round_type_id, int round_number, int set_format_id, int winner_team_id, int stage_id, uint64_t time_started, int duration);
        /*! 
         * Iterates over all available games.
         * Return 1 to successfuly stop iteration. Return -1 to stop iteration and return an error. Return 0 to continue iteration. */
        int (*get_all)(struct olddb* db,
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
            void* user);
        /*!
         * Iterates over all unique events. Since events can be shared between games
         * from different tournaments, the grouping is achieved by date and by event
         * name. For example, date "2022-04-11" event "Singles Bracket" is a unique
         * event.
         * 
         * Note that games exist that are not associated with any event or tournament.
         * In this case, event_id is set to -1 in the callback.
         * 
         * Return 1 to successfuly stop iteration. Return -1 to stop iteration and
         * return an error. Return 0 to continue iteration.
         */
        int (*get_events)(struct olddb* db,
            int (*on_game_event)(
                const char* date,  /* Date string will be "YYYY-MM-DD" */
                const char* name,  /* Name of the event, such as "Singles Bracket" */
                int event_id,      /* Event ID. Can be -1, indicating no association to an event. */
                void* user),
            void* user);
        int (*get_all_in_event)(struct olddb* db,
            struct str_view date,  /* Date of the event, formatted as "YYYY-MM-DD" */
            int event_id,          /* Event ID. Can be -1, which will return all games NOT associated with any event. */
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
            void* user);

        int (*associate_tournament)(struct olddb* db, int game_id, int tournament_id);
        int (*associate_event)(struct olddb* db, int game_id, int event_id);
        int (*associate_video)(struct olddb* db, int game_id, int video_id, int64_t frame_offset);
        int (*unassociate_video)(struct olddb* db, int game_id, int video_id);
        int (*get_videos)(struct olddb* db, int game_id,
            int (*on_video)(
                const char* file_name,
                const char* path_hint,
                int64_t frame_offset,
                void* user),
            void* user);
        int (*add_player)(struct olddb* db, int person_id, int game_id, int slot, int team_id, int fighter_id, int costume, int is_loser_side);
    } game;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view name);
        int (*add_game)(struct olddb* db, int group_id, int game_id);
    } group;

    struct {
        int (*add_or_get)(struct olddb* db, struct str_view file_name, struct str_view path_hint);
        int (*set_path_hint)(struct olddb* db, struct str_view file_name, struct str_view path_hint);
        int (*add_path)(struct olddb* db, struct str_view path);
        int (*query_paths)(struct olddb* db, int (*on_video_path)(const char* path, void* user), void* user);
    } video;

    struct {
        int (*add)(struct olddb* db, int game_id, int team_id, int score);
    } score;

    struct {
        int (*add)(struct olddb* db, struct str_view name, struct str_view ip, uint16_t port);
    } switch_info;

    struct {
        int (*add)(struct olddb* db, struct str_view path, int frame_offset);
    } stream_recording_sources;
};

VH_PUBLIC_API struct olddb_interface*
olddb(const char* type);

VH_PRIVATE_API int
olddb_init(void);

VH_PRIVATE_API void
olddb_deinit(void);

C_END
