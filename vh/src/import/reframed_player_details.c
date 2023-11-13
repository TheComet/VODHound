#include "vh/db_ops.h"
#include "vh/log.h"

#include "json-c/json.h"

int
reframed_add_person_to_db(
    struct db_interface* dbi, struct db* db,
    int sponsor_id,
    struct str_view name, struct str_view tag,
    struct str_view social, struct str_view pronouns);

int
import_reframed_player_details(
        struct db_interface* dbi,
        struct db* db,
        const char* file_path)
{
    struct json_object* root = json_object_from_file(file_path);
    if (root == NULL)
    {
        log_err("Failed to open file '%s'\n", file_path);
        return -1;
    }
    log_info("Loading player details from '%s'\n", file_path);

    struct json_object* players = json_object_object_get(root, "players");
    if (json_object_get_type(players) != json_type_object)
        goto fail;

    json_object_object_foreach(players, tag_cstr, data)
    {
        const char* sponsor = json_object_get_string(json_object_object_get(data, "sponsor"));
        const char* name = json_object_get_string(json_object_object_get(data, "name"));
        const char* social = json_object_get_string(json_object_object_get(data, "social"));
        const char* pronouns = json_object_get_string(json_object_object_get(data, "pronouns"));
        if (sponsor && name && social && pronouns)
        {
            /* The player tag is lost if the data comes from a saved replay instead of a live game */
            struct str_view tag = cstr_view(tag_cstr);
            if (cstr_starts_with(tag, "Player "))
                tag = cstr_view(name);  /* Best guess */

            int sponsor_id = -1;
            if (*sponsor)
            {
                sponsor_id = dbi->sponsor.add_or_get(db, cstr_view(sponsor), cstr_view(""), cstr_view(""));
                if (sponsor_id < 0)
                    goto fail;
            }
            if (reframed_add_person_to_db(dbi, db,
                sponsor_id,
                cstr_view(name),
                tag,
                cstr_view(social ? social : ""),
                cstr_view(pronouns ? pronouns : "")) < 0)
            {
                goto fail;
            }
        }
    }

    json_object_put(root);
    return 0;

fail:
    log_err("Invalid JSON encountered in file '%s'\n", file_path);
    json_object_put(root);
    return -1;
}
