#include "vh/backtrace.h"
#include "vh/mem.h"
#include "vh/init.h"

/* ------------------------------------------------------------------------- */
VH_PUBLIC_API int
init(void)
{
    if (backtrace_init() < 0)
        goto backtrace_init_failed;
    if (threadlocal_init() < 0)
        goto threadlocal_init_failed;

    return 0;

    threadlocal_init_failed : backtrace_deinit();
    backtrace_init_failed   : return -1;
}

/* ------------------------------------------------------------------------- */
VH_PUBLIC_API void
deinit(void)
{
    threadlocal_deinit();
    backtrace_deinit();
}

/* ------------------------------------------------------------------------- */
VH_PUBLIC_API int
threadlocal_init(void)
{
    return mem_threadlocal_init();
}

/* ------------------------------------------------------------------------- */
VH_PUBLIC_API void
threadlocal_deinit(void)
{
    mem_threadlocal_deinit();
}
