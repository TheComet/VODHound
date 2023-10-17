#include "application/import.h"

#include "vh/fs.h"
#include "vh/db_ops.h"
#include "vh/log.h"

#include "json-c/json.h"

int
import_reframed_replay(
        struct db_interface* dbi,
        struct db* db,
        const char* file_name);

int
import_reframed_player_details(
        struct db_interface* dbi,
        struct db* db,
        const char* file_path);

int
import_reframed_motion_labels(
        struct db_interface* dbi,
        struct db* db,
        const char* file_name);

struct on_game_path_file_ctx
{
    struct db_interface* dbi;
    struct db* db;
    struct path path;
};

static int
on_game_path_file(const char* name_cstr, void* user)
{
    int ret;
    struct on_game_path_file_ctx* ctx = user;
    struct str_view name = cstr_view(name_cstr);

    if (!cstr_ends_with(name, ".rfr"))
        return 0;

    if (path_join(&ctx->path, name) < 0)
        return -1;

    /* Be aggressive and continue, regardless of whether import succeeded or not */
    path_terminate(&ctx->path);
    import_reframed_replay(ctx->dbi, ctx->db, ctx->path.str.data);
    path_dirname(&ctx->path);

    return 0;
}

static int
import_reframed_config(struct db_interface* dbi, struct db* db, const char* file_path)
{
    struct json_object* root = json_object_from_file(file_path);
    if (root == NULL)
    {
        log_err("Failed to open file '%s'\n", file_path);
        goto load_config_failed;
    }

    struct json_object* autoassociatevideos = json_object_object_get(
                json_object_object_get(root, "activesessionmanager"), "autoassociatevideos");
    const char* path = json_object_get_string(json_object_object_get(autoassociatevideos, "dir"));
    int frame_offset = json_object_get_int(json_object_object_get(autoassociatevideos, "offset"));
    if (path && *path)
    {
        if (dbi->stream_recording_sources.add(db, cstr_view(path), frame_offset) < 0)
            goto fail;
    }

    struct json_object* connectinfo = json_object_object_get(root, "connectview");
    const char* lastip = json_object_get_string(json_object_object_get(connectinfo, "lastip"));
    const char* lastport = json_object_get_string(json_object_object_get(connectinfo, "lastport"));
    if (lastip && lastport && *lastip && *lastport)
    {
        if (dbi->switch_info.add(db, cstr_view("Nintendo Switch"), cstr_view(lastip), atoi(lastport)) < 0)
            goto fail;
    }

    struct json_object* videopaths = json_object_object_get(json_object_object_get(root, "replaymanager"), "videopaths");
    if (json_object_get_type(videopaths) != json_type_array)
        goto fail;
    for (int i = 0; i != json_object_array_length(videopaths); ++i)
    {
        const char* path = json_object_get_string(json_object_array_get_idx(videopaths, i));
        if (!path || !*path)
            continue;
        if (dbi->video.add_path(db, cstr_view(path)) != 0)
            goto fail;
    }

    struct json_object* gamepaths = json_object_object_get(json_object_object_get(root, "replaymanager"), "gamepaths");
    if (json_object_get_type(gamepaths) != json_type_array)
        goto fail;

    struct on_game_path_file_ctx gamepaths_ctx = { dbi, db };
    path_init(&gamepaths_ctx.path);

    for (int i = 0; i != json_object_array_length(gamepaths); ++i)
    {
        const char* path = json_object_get_string(json_object_array_get_idx(gamepaths, i));
        if (!path || !*path)
            continue;

        if (path_set(&gamepaths_ctx.path, cstr_view(path)) < 0)
            goto path_error;
        if (fs_list(cstr_view(path), on_game_path_file, &gamepaths_ctx) < 0)
            goto path_error;
        continue;

        path_error : path_deinit(&gamepaths_ctx.path);
        goto fail;
    }
    path_deinit(&gamepaths_ctx.path);

    json_object_put(root);
    return 0;

    fail                     : json_object_put(root);
    load_config_failed       : return -1;
}

int
import_reframed_all(struct db_interface* dbi, struct db* db)
{
    struct path file_path;
    path_init(&file_path);

    if (dbi->transaction.begin(db) != 0)
        goto transaction_begin_failed;

    /*
     * Begin with config file. This contains a list of all paths to all replays,
     * which we will gather and load
     */
    if (path_set(&file_path, fs_appdata_dir()) < 0) goto fail;
    if (path_join(&file_path, cstr_view("ReFramed")) < 0) goto fail;
    if (path_join(&file_path, cstr_view("config.json")) < 0) goto fail;
    path_terminate(&file_path);
    if (import_reframed_config(dbi, db, file_path.str.data) < 0)
        goto fail;

    /*
     * Some information on players might still be in the player details json.
     * Can't hurt to parse it as well. Most likely, all of the information is
     * already in the DB from all of the replay files.
     */
    if (path_set(&file_path, fs_appdata_dir()) < 0) goto fail;
    if (path_join(&file_path, cstr_view("ReFramed")) < 0) goto fail;
    if (path_join(&file_path, cstr_view("playerDetails.json")) < 0) goto fail;
    path_terminate(&file_path);
    if (import_reframed_player_details(dbi, db, file_path.str.data) < 0)
        goto fail;

    /*
    if (path_set(&file_path, fs_appdata_dir()) < 0) goto fail;
    if (path_join(&file_path, cstr_view("ReFramed")) < 0) goto fail;
    if (path_join(&file_path, cstr_view("motionLabels.dat")) < 0) goto fail;
    path_terminate(&file_path);
    if (import_reframed_motion_labels(dbi, db, file_path.str.data) < 0)
        goto fail;*/

    if (dbi->transaction.commit(db) != 0)
        goto fail;

    path_deinit(&file_path);
    return 0;

    fail                     : dbi->transaction.rollback(db);
    transaction_begin_failed : path_deinit(&file_path);
    return -1;
}