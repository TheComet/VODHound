#include "vh/db_ops.h"
#include "vh/log.h"

#include "json-c/json.h"

int
import_reframed_mapping_info(struct db_interface* dbi, struct db* db, const char* file_name)
{
    json_object* root = json_object_from_file(file_name);
    json_object* jversion = json_object_object_get(root, "version");

    json_object* statuses = json_object_object_get(root, "fighterstatus");
    json_object* fighter_ids = json_object_object_get(root, "fighterid");
    json_object* stage_ids = json_object_object_get(root, "stageid");
    json_object* hit_statuses = json_object_object_get(root, "hitstatus");

    json_object* base_statuses = json_object_object_get(statuses, "base");
    json_object* specific_statuses = json_object_object_get(statuses, "specific");

    if (root == NULL)
    {
        log_err("File '%s' not found\n", file_name);
        return -1;
    }

    log_info("Importing mapping info from '%s'\n", file_name);
    log_dbg("mapping info version: %s\n", json_object_get_string(jversion));

    if (dbi->transaction_begin(db) != 0)
        goto transaction_begin_failed;

    { json_object_object_foreach(fighter_ids, fighter_id_str, fighter_name)
    {
        int fighter_id = atoi(fighter_id_str);
        const char* name = json_object_get_string(fighter_name);
        if (dbi->fighter_add(db, fighter_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(stage_ids, stage_id_str, stage_name)
    {
        int stage_id = atoi(stage_id_str);
        const char* name = json_object_get_string(stage_name);
        if (dbi->stage_add(db, stage_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(base_statuses, status_str, enum_name)
    {
        int status_id = atoi(status_str);
        const char* name = json_object_get_string(enum_name);
        if (dbi->status_enum_add(db, -1, status_id, cstr_view(name)) != 0)
            goto fail;
    }}

    { json_object_object_foreach(specific_statuses, fighter_id_str, fighter)
    {
        int fighter_id = atoi(fighter_id_str);
        json_object_object_foreach(fighter, status_str, enum_name)
        {
            int status_id = atoi(status_str);
            const char* name = json_object_get_string(enum_name);
            if (dbi->status_enum_add(db, fighter_id, status_id, cstr_view(name)) != 0)
                goto fail;
        }
    }}

    { json_object_object_foreach(hit_statuses, hit_status_str, hit_status_name)
    {
        int hit_status_id = atoi(hit_status_str);
        const char* name = json_object_get_string(hit_status_name);
        if (dbi->hit_status_enum_add(db, hit_status_id, cstr_view(name)) != 0)
            goto fail;
    }}

    json_object_put(root);
    return dbi->transaction_commit(db);

    fail                     : dbi->transaction_rollback(db);
    transaction_begin_failed : json_object_put(root);
    unsupported_version      : return -1;
}
