#include "rf/log.h"
#include "rf/cli_colors.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static RF_THREADLOCAL const char* g_prefix = "";
static RF_THREADLOCAL const char* g_col_set = "";
static RF_THREADLOCAL const char* g_col_clr = "";

static FILE* g_log = NULL;
static FILE* g_net = NULL;

/* ------------------------------------------------------------------------- */
void
rf_log_set_prefix(const char* prefix)
{
    g_prefix = prefix;
}
void
rf_log_set_colors(const char* set, const char* clear)
{
    g_col_set = set;
    g_col_clr = clear;
}

/* ------------------------------------------------------------------------- */
void
rf_log_file_open(const char* log_file)
{
    if (g_log)
    {
        rf_log_warn("log_file_open() called, but a log file is already open. Closing previous file...\n");
        rf_log_file_close();
    }

    rf_log_info("Opening log file \"%s\"\n", log_file);
    g_log = fopen(log_file, "w");
    if (g_log == NULL)
        rf_log_err("Failed to open log file \"%s\": %s\n", log_file, strerror(errno));
}
void
rf_log_file_close()
{
    if (g_log)
    {
        rf_log_info("Closing log file\n");
        fclose(g_log);
        g_log = NULL;
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_raw(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    if (g_log)
    {
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_dbg(const char* fmt, ...)
{
    va_list va;
    fprintf(stderr, "[" COL_N_YELLOW "Debug" COL_RESET "] %s%s%s", g_col_set, g_prefix, g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    if (g_log)
    {
        fprintf(g_log, "[Debug] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_info(const char* fmt, ...)
{
    va_list va;
    fprintf(stderr, "[" COL_B_WHITE "Info " COL_RESET "] %s%s%s", g_col_set, g_prefix, g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    if (g_log)
    {
        fprintf(g_log, "[Info ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_warn(const char* fmt, ...)
{
    va_list va;
    fprintf(stderr, "[" COL_B_YELLOW "Warn " COL_RESET "] %s%s%s", g_col_set, g_prefix, g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    if (g_log)
    {
        fprintf(g_log, "[Warn ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_err(const char* fmt, ...)
{
    va_list va;
    fprintf(stderr, "[" COL_B_RED "Error" COL_RESET "] %s%s%s", g_col_set, g_prefix, g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);

    if (g_log)
    {
        fprintf(g_log, "[Error] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_note(const char* fmt, ...)
{
    va_list va;
    fprintf(stderr, "[" COL_B_MAGENTA "NOTE " COL_RESET "] %s%s%s" COL_B_YELLOW, g_col_set, g_prefix, g_col_clr);
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, COL_RESET);

    if (g_log)
    {
        fprintf(g_log, "[NOTE ] %s", g_prefix);
        va_start(va, fmt);
        vfprintf(g_log, fmt, va);
        va_end(va);
        fflush(g_log);
    }
}

/* ------------------------------------------------------------------------- */
void
rf_log_sqlite_err(int error_code, const char* error_msg)
{
    fprintf(stderr, "[" COL_B_RED "Error" COL_RESET "] %s%s%s", g_col_set, g_prefix, g_col_clr);
    fprintf(stderr, "SQLite3 error (%d): %s\n", error_code, error_msg);

    if (g_log)
    {
        fprintf(g_log, "[Error] %sSQLite3 error (%d): %s\n",
                g_prefix, error_code, error_msg);
        fflush(g_log);
    }
}
