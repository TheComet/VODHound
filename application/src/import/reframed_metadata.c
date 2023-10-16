#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mstream.h"

#include "json-c/json.h"

int
import_reframed_metadata_1_5(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root);

int
import_reframed_metadata_1_6(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root);

int
import_reframed_metadata_1_7(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root);

int
import_reframed_metadata(
        struct db_interface* dbi,
        struct db* db,
        struct mstream* ms)
{
    int game_id;
    struct json_tokener* tok = json_tokener_new();
    struct json_object* root = json_tokener_parse_ex(tok, ms->address, ms->size);
    json_tokener_free(tok);

    if (root == NULL)
        goto parse_failed;

    struct json_object* version = json_object_object_get(root, "version");
    const char* version_str = json_object_get_string(version);
    if (version_str == NULL)
        goto fail;

    if      (strcmp(version_str, "1.7") == 0) game_id = import_reframed_metadata_1_7(dbi, db, root);
    else if (strcmp(version_str, "1.6") == 0) game_id = import_reframed_metadata_1_6(dbi, db, root);
    else if (strcmp(version_str, "1.5") == 0) game_id = import_reframed_metadata_1_5(dbi, db, root);
    else
    {
        log_err("Failed to import RFR: Unsupported metadata version %s\n", version_str);
        goto fail;
    }
    if (game_id < 0)
        goto fail;

    json_object_put(root);
    return game_id;

    fail         : json_object_put(root);
    parse_failed : return -1;
}
