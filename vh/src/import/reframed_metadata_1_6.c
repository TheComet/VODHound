#include "vh/db.h"
#include "vh/log.h"

#include "json-c/json.h"

int
import_reframed_metadata_1_6(
        struct db_interface* dbi,
        struct db* db,
        struct json_object* root)
{
    log_err("Metadata v1.6 importer not implemented\n");
    return -1;
}
