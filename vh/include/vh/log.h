#pragma once

#include "vh/config.h"

C_BEGIN

VH_PUBLIC_API void
log_set_prefix(const char* prefix);
VH_PUBLIC_API void
log_set_colors(const char* set, const char* clear);

VH_PUBLIC_API void
log_file_open(const char* log_file);
VH_PUBLIC_API void
log_file_close(void);

/* General logging functions ----------------------------------------------- */
VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_raw(const char* fmt, ...);

VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_dbg(const char* fmt, ...);

VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_info(const char* fmt, ...);

VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_warn(const char* fmt, ...);

VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_err(const char* fmt, ...);

VH_PUBLIC_API VH_PRINTF_FORMAT(1, 2) void
log_note(const char* fmt, ...);

/* Specific logging functions ---------------------------------------------- */
VH_PRIVATE_API VH_PRINTF_FORMAT(1, 2) void
log_mem_warn(const char* fmt, ...);
VH_PRIVATE_API VH_PRINTF_FORMAT(1, 2) void
log_mem_err(const char* fmt, ...);
VH_PRIVATE_API VH_PRINTF_FORMAT(1, 2) void
log_mem_note(const char* fmt, ...);

C_END
