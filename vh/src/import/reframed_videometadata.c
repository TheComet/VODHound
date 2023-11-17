#include "vh/db.h"
#include "vh/mstream.h"

#include "json-c/json.h"

int
import_reframed_videometadata_1_0(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root,
        int game_id);

int
import_reframed_videometadata(
        struct db_interface* dbi,
        struct db* db,
        struct mstream* ms,
        int game_id)
{
    struct json_tokener* tok = json_tokener_new();
    struct json_object* root = json_tokener_parse_ex(tok, ms->address, ms->size);
    json_tokener_free(tok);

    if (root == NULL)
        goto parse_failed;

    struct json_object* version = json_object_object_get(root, "version");
    const char* version_str = json_object_get_string(version);
    if (version_str == NULL)
        goto fail;

    if (strcmp(version_str, "1.0") == 0)
    {
        if (import_reframed_videometadata_1_0(dbi, db, root, game_id) < 0)
            goto fail;
    }
    else
        goto fail;

    json_object_put(root);
    return 0;

    fail         : json_object_put(root);
    parse_failed : return -1;
}
