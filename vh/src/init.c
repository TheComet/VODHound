#include "vh/backtrace.h"
#include "vh/mem.h"
#include "vh/init.h"

#if !defined(VH_MEM_BACKTRACE)
int backtrace_init(void) { return 0; }
void backtrace_deinit(void) {}
#endif

/* ------------------------------------------------------------------------- */
int
vh_init(void)
{
    if (backtrace_init() < 0)
        goto backtrace_init_failed;
    if (vh_threadlocal_init() < 0)
        goto threadlocal_init_failed;

    return 0;

    threadlocal_init_failed : backtrace_deinit();
    backtrace_init_failed   : return -1;
}

/* ------------------------------------------------------------------------- */
void
vh_deinit(void)
{
    vh_threadlocal_deinit();
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
