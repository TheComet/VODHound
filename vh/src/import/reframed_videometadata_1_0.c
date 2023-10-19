#include "vh/db_ops.h"

#include "json-c/json.h"

int
import_reframed_videometadata_1_0(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root,
        int game_id)
{
    int video_id;
    const char* file_name = json_object_get_string(json_object_object_get(root, "filename"));
    int64_t offset = json_object_get_int64(json_object_object_get(root, "offset"));
    if (file_name == NULL || !*file_name)
        return 0;

    video_id = dbi->video.add_or_get(db, cstr_view(file_name), cstr_view(""));
    if (video_id < 0)
        return -1;
    if (dbi->game.associate_video(db, game_id, video_id, offset) < 0)
        return -1;

    return 0;
}
