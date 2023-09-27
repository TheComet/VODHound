#include "rf/backtrace.h"
#include "rf/mem.h"
#include "rf/init.h"

/* ------------------------------------------------------------------------- */
RF_PUBLIC_API int
rf_init(void)
{
    if (rf_backtrace_init() < 0)
        goto backtrace_init_failed;
    if (rf_threadlocal_init() < 0)
        goto threadlocal_init_failed;

    return 0;

    threadlocal_init_failed : rf_backtrace_deinit();
    backtrace_init_failed   : return -1;
}

/* ------------------------------------------------------------------------- */
RF_PUBLIC_API void
rf_deinit(void)
{
    rf_threadlocal_deinit();
    rf_backtrace_deinit();
}

/* ------------------------------------------------------------------------- */
RF_PUBLIC_API int
rf_threadlocal_init(void)
{
    return rf_mem_threadlocal_init();
}

/* ------------------------------------------------------------------------- */
RF_PUBLIC_API void
rf_threadlocal_deinit(void)
{
    rf_mem_threadlocal_deinit();
}
