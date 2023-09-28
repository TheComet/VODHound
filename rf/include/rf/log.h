#pragma once

#include "rf/config.h"

C_BEGIN

void
rf_log_set_prefix(const char* prefix);
void
rf_log_set_colors(const char* set, const char* clear);

void
rf_log_file_open(const char* log_file);
void
rf_log_file_close(void);

/* General logging functions ----------------------------------------------- */
RF_PRINTF_FORMAT(1, 2) void
rf_log_raw(const char* fmt, ...);

RF_PRINTF_FORMAT(1, 2) void
rf_log_dbg(const char* fmt, ...);

RF_PRINTF_FORMAT(1, 2) void
rf_log_info(const char* fmt, ...);

RF_PRINTF_FORMAT(1, 2) void
rf_log_warn(const char* fmt, ...);

RF_PRINTF_FORMAT(1, 2) void
rf_log_err(const char* fmt, ...);

RF_PRINTF_FORMAT(1, 2) void
rf_log_note(const char* fmt, ...);

/* Specialized logging functions ------------------------------------------- */
void
rf_log_sqlite_err(int error_code, const char* error_code_str, const char* error_msg);

C_END
