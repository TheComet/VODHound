#include "vh/db.h"
#include "vh/log.h"
#include "vh/mfile.h"
#include "vh/mstream.h"

int
import_reframed_metadata(
        struct db_interface* dbi,
        struct db* db,
        struct mstream* ms);

int
import_reframed_videometadata(
        struct db_interface* dbi,
        struct db* db,
        struct mstream* ms,
        int game_id);

int
import_reframed_framedata(struct mstream* ms, int game_id);

int
import_reframed_replay(
        struct db_interface* dbi,
        struct db* db,
        const char* file_name)
{
    struct blob_entry
    {
        const void* type;
        int offset;
        int size;
    } entries[3];

    struct mfile mf;
    struct mstream ms;

    uint8_t num_entries;
    int i;
    int entry_idx;
    int game_id;

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s'\n", file_name);
        goto mmap_failed;
    }

    log_info("Importing replay '%s'\n", file_name);

    ms = mstream_from_mfile(&mf);
    if (memcmp(mstream_read(&ms, 4), "RFR1", 4) != 0)
    {
        log_err("File '%s' has invalid header\n", file_name);
        goto invalid_header;
    }

    /*
     * Blobs can be in any order within the RFR file. We have to import them
     * in a specific order for the db operations to work. This order is:
     *   1) META (metadata)
     *   2) VIDM (video metadata) depends on game_id from META
     *   3) FDAT (frame data) depends on game_id from META
     * The "MAPI" (mapping info) blob doesn't need to be loaded, because we
     * create the mapping info structures from a JSON file now.
     */
    num_entries = mstream_read_u8(&ms);
    for (i = 0, entry_idx = 0; i != num_entries; ++i)
    {
        const void* type = mstream_read(&ms, 4);
        int offset = (int)mstream_read_lu32(&ms);
        int size = (int)mstream_read_lu32(&ms);
        if (entry_idx < 3 &&
            (memcmp(type, "META", 4) == 0 || memcmp(type, "FDAT", 4) == 0 || memcmp(type, "VIDM", 4) == 0))
        {
            entries[entry_idx].type = type;
            entries[entry_idx].offset = offset;
            entries[entry_idx].size = size;
            entry_idx++;
        }
    }

    num_entries = (uint8_t)entry_idx;
    game_id = -1;
    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "META", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            game_id = import_reframed_metadata(dbi, db, &blob);
            if (game_id < 0)
                goto fail;
            break;
        }
    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "VIDM", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            if (import_reframed_videometadata(dbi, db, &blob, game_id) < 0)
                goto fail;
            break;
        }

    for (i = 0; i != num_entries; ++i)
        if (memcmp(entries[i].type, "FDAT", 4) == 0)
        {
            struct mstream blob = mstream_from_mstream(&ms, entries[i].offset, entries[i].size);
            if (import_reframed_framedata(&blob, game_id) < 0)
                goto fail;
            break;
        }

    mfile_unmap(&mf);
    return 0;

    fail                     :
    invalid_header           : mfile_unmap(&mf);
    mmap_failed              : return -1;
}
