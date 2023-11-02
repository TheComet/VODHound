#include "vh/btree.h"
#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mfile.h"
#include "vh/mstream.h"

int
import_reframed_motion_labels(
        struct db_interface* dbi,
        struct db* db,
        const char* file_name)
{
    struct mfile mf;
    struct mstream ms;
    struct btree layer_ids;
    int i;
    int hash40_count;
    int layer_count;
    int fighter_count;
    int fighter_id;
    uint8_t major, minor;

    btree_init(&layer_ids, sizeof(int));

    if (mfile_map(&mf, file_name) != 0)
        goto map_file_failed;

    log_info("Importing motion labels from '%s'\n", file_name);

    ms = mstream_from_memory(mf.address, mf.size);
    major = mstream_read_u8(&ms);
    minor = mstream_read_u8(&ms);
    if (major != 1 || minor != 1)
    {
        log_err("Failed to import file '%s': Unsupported version %d.%d\n",
            file_name, (int)major, (int)minor);
        goto fail;
    }

    /* TODO: preferred layers are currently ignored */
    mstream_read_lu16(&ms);  /* Readable */
    mstream_read_lu16(&ms);  /* Notation */
    mstream_read_lu16(&ms);  /* Categorization */

    /* Hash40 table */
    hash40_count = mstream_read_lu32(&ms);
    for (i = 0; i != hash40_count; ++i)
    {
        uint32_t lower = mstream_read_lu32(&ms);
        uint8_t upper = mstream_read_u8(&ms);
        uint64_t motion = ((uint64_t)upper << 32) | lower;
        uint8_t label_len = mstream_read_u8(&ms);
        struct str_view label = { mstream_read(&ms, label_len), label_len };
        if (dbi->motion.add(db, motion, label) < 0)
            goto fail;
        (void)label;
    }

    /* 
     * Load layer names, group names, and layer usages. These map onto the
     * motion_layer, motion_groups and motion_usages tables in the db
     */
    layer_count = mstream_read_lu16(&ms);
    for (i = 0; i != layer_count; ++i)
    {
        int layer_id;
        int name_len = mstream_read_u8(&ms);
        int group_len = mstream_read_u8(&ms);
        int usage = mstream_read_u8(&ms);
        struct str_view layer_name = { mstream_read(&ms, name_len), name_len };
        struct str_view group_name = { mstream_read(&ms, group_len), group_len };
        int group_id = dbi->motion_label.add_or_get_group(db, group_name);
        if (group_id < 0)
            goto fail;
        layer_id = dbi->motion_label.add_or_get_layer(db, group_id, layer_name);
        if (layer_id < 0)
            goto fail;
        if (btree_insert_new(&layer_ids, i, &layer_id) != 1)
            goto fail;
    }

    fighter_count = mstream_read_u8(&ms);
    for (fighter_id = 0; fighter_id != fighter_count; ++fighter_id)
    {
        int row;
        int row_count = mstream_read_lu16(&ms);
        for (row = 0; row != row_count; ++row)
        {
            int category_id = -1;
            uint32_t lower = mstream_read_lu32(&ms);
            uint8_t upper = mstream_read_lu32(&ms);
            enum category {
                MOVEMENT,
                GROUND_ATTACKS,
                AERIAL_ATTACKS,
                SPECIAL_ATTACKS,
                GRABS,
                LEDGE,
                DEFENSIVE,
                DISADVANTAGE,
                ITEMS,
                MISC,
                UNLABELED
            } category = mstream_read_u8(&ms);
            uint64_t motion = ((uint64_t)upper << 32) | lower;

            switch (category)
            {
            case MOVEMENT        : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case GROUND_ATTACKS  : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case AERIAL_ATTACKS  : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case SPECIAL_ATTACKS : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case GRABS           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case LEDGE           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case DEFENSIVE       : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case DISADVANTAGE    : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case ITEMS           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case MISC            : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            case UNLABELED       : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
            }

            int layer_idx;
            for (layer_idx = 0; layer_idx != layer_count; ++layer_idx)
            {
                int8_t len = mstream_read_u8(&ms);
                struct str_view label = { mstream_read(&ms, len), len };
                int layer_id = *(int*)btree_find(&layer_ids, layer_idx);

            }
        }
    }

    mfile_unmap(&mf);
    btree_deinit(&layer_ids);
    return 0;

fail:
    mfile_unmap(&mf);
map_file_failed:
    btree_deinit(&layer_ids);
    return -1;
}
