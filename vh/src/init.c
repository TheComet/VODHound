#include "vh/backtrace.h"
#include "vh/crc32.h"
#include "vh/db.h"
#include "vh/fs.h"
#include "vh/mem.h"
#include "vh/init.h"

/* ------------------------------------------------------------------------- */
int
vh_init(void)
{
    if (backtrace_init() < 0)
        goto backtrace_init_failed;
    if (fs_init() < 0)
        goto fs_init_failed;
    if (db_init() < 0)
        goto db_init_failed;

    crc32_init();

    return 0;

    db_init_failed          : fs_deinit();
    fs_init_failed          : backtrace_deinit();
    backtrace_init_failed   : return -1;
}

/* ------------------------------------------------------------------------- */
void
vh_deinit(void)
{
    db_deinit();
    fs_deinit();
    backtrace_deinit();
}

/* ------------------------------------------------------------------------- */
int
vh_threadlocal_init(void)
{
    return mem_threadlocal_init();
}

/* ------------------------------------------------------------------------- */
void
vh_threadlocal_deinit(void)
{
    mem_threadlocal_deinit();
}
