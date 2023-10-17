#include "application/import.h"

#include "vh/db_ops.h"
#include "vh/log.h"
#include "vh/mfile.h"
#include "vh/mstream.h"

static int newline_or_end(char b) { return b == '\r' || b == '\n' || b == '\0'; }

int
import_param_labels_csv(struct db_interface* dbi, struct db* db, const char* file_name)
{
    struct mfile mf;
    struct mstream ms;

    if (mfile_map(&mf, file_name) != 0)
    {
        log_err("Failed to open file '%s'\n", file_name);
        goto open_file_failed;
    }

    log_info("Importing hash40 strings from '%s'\n", file_name);

    if (dbi->transaction.begin(db) != 0)
        goto transaction_begin_failed;

    ms = mstream_from_mfile(&mf);

    while (!mstream_at_end(&ms))
    {
        /* Extract hash40 and label, splitting on comma */
        struct str_view h40_str, label;
        if (mstream_read_string_until_delim(&ms, ',', &h40_str) != 0)
            break;
        if (mstream_read_string_until_condition(&ms, newline_or_end, &label) != 0)
            break;

        /* Convert hash40 into value */
        uint64_t h40;
        if (str_hex_to_u64(h40_str, &h40) != 0)
            continue;

        if (h40 == 0 || label.len == 0)
            continue;

        if (dbi->motion.add(db, h40, label) != 0)
            goto add_failed;
    }

    dbi->transaction.commit(db);
    mfile_unmap(&mf);

    return 0;

    add_failed               : dbi->transaction.rollback(db);
    transaction_begin_failed : mfile_unmap(&mf);
    open_file_failed         : return -1;
}
