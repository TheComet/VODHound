#include "rf/backtrace.h"
#include <execinfo.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
int
rf_backtrace_init(void)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
void
rf_backtrace_deinit(void)
{
}

/* ------------------------------------------------------------------------- */
char**
rf_backtrace_get(int* size)
{
    void* array[RF_BACKTRACE_SIZE];
    char** strings;

    *size = backtrace(array, RF_BACKTRACE_SIZE);
    strings = backtrace_symbols(array, *size);

    return strings;
}

/* ------------------------------------------------------------------------- */
void
rf_backtrace_free(char** bt)
{
    free(bt);
}
