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
    struct btree usage_ids;
    int layer_idx;
    int hash40_count;
    int layer_count;
    int fighter_count;
    int fighter_id;
    uint8_t major, minor;

    btree_init(&layer_ids, sizeof(int));
    btree_init(&usage_ids, sizeof(int));

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s'\n", file_name);
        goto map_file_failed;
    }

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
    hash40_count = (int)mstream_read_lu32(&ms);
    for (layer_idx = 0; layer_idx != hash40_count; ++layer_idx)
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
    for (layer_idx = 0; layer_idx != layer_count; ++layer_idx)
    {
        enum usage
        {
            READABLE,
            NOTATION,
            CATEGORIZATION
        };

        int group_id, layer_id, usage_id;
        int name_len = mstream_read_u8(&ms);
        int group_len = mstream_read_u8(&ms);
        enum usage usage = mstream_read_u8(&ms);
        struct str_view layer_name = { mstream_read(&ms, name_len), name_len };
        struct str_view group_name = { mstream_read(&ms, group_len), group_len };

        group_id = dbi->motion_label.add_or_get_group(db, group_name);
        if (group_id < 0)
            goto fail;
        layer_id = dbi->motion_label.add_or_get_layer(db, group_id, layer_name);
        if (layer_id < 0)
            goto fail;
        switch (usage)
        {
            default             : usage_id = dbi->motion_label.add_or_get_usage(db, cstr_view("Readable")); break;
            case NOTATION       : usage_id = dbi->motion_label.add_or_get_usage(db, cstr_view("Notation")); break;
            case CATEGORIZATION : usage_id = dbi->motion_label.add_or_get_usage(db, cstr_view("Categorization")); break;
        }
        if (usage_id < 0)
            goto fail;

        if (btree_insert_new(&layer_ids, (btree_key)layer_idx, &layer_id) != 1)
            goto fail;
        if (btree_insert_new(&usage_ids, (btree_key)layer_idx, &usage_id) != 1)
            goto fail;
    }

    fighter_count = mstream_read_u8(&ms);
    for (fighter_id = 0; fighter_id != fighter_count; ++fighter_id)
    {
        int row;
        int row_count = mstream_read_lu16(&ms);
        for (row = 0; row != row_count; ++row)
        {
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
            };

            int category_id = -1;
            uint32_t lower = mstream_read_lu32(&ms);
            uint8_t upper = mstream_read_u8(&ms);
            enum category category = mstream_read_u8(&ms);
            uint64_t motion = ((uint64_t)upper << 32) | lower;

            switch (category)
            {
                case MOVEMENT        : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Movement")); break;
                case GROUND_ATTACKS  : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Ground Attacks")); break;
                case AERIAL_ATTACKS  : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Aerial Attacks")); break;
                case SPECIAL_ATTACKS : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Special Attacks")); break;
                case GRABS           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Grabs")); break;
                case LEDGE           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Ledge")); break;
                case DEFENSIVE       : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Defensive")); break;
                case DISADVANTAGE    : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Disadvantage")); break;
                case ITEMS           : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Items")); break;
                case MISC            : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Misc")); break;
                default              : category_id = dbi->motion_label.add_or_get_category(db, cstr_view("Unlabeled")); break;
            }
            if (category_id < 0)
                goto fail;

            for (layer_idx = 0; layer_idx != layer_count; ++layer_idx)
            {
                int8_t len = mstream_read_i8(&ms);
                struct str_view label = { mstream_read(&ms, len), len };
                int layer_id = *(int*)btree_find(&layer_ids, (btree_key)layer_idx);
                int usage_id = *(int*)btree_find(&usage_ids, (btree_key)layer_idx);
                if (dbi->motion_label.add_or_get_label(db, motion, fighter_id, layer_id, category_id, usage_id, label) < 0)
                    goto fail;
            }
        }
    }

    mfile_unmap(&mf);
    btree_deinit(&usage_ids);
    btree_deinit(&layer_ids);
    return 0;

fail:
    log_err("Invalid data found in file '%s'\n", file_name);
    mfile_unmap(&mf);
map_file_failed:
    btree_deinit(&usage_ids);
    btree_deinit(&layer_ids);
    return -1;
}
