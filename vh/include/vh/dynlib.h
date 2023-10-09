#pragma once

#include "vh/config.h"
#include "vh/str.h"

C_BEGIN

VH_PUBLIC_API int
dynlib_add_path(const char* path);

VH_PUBLIC_API void*
dynlib_open(const char* file_name);

VH_PUBLIC_API void
dynlib_close(void* handle);

VH_PUBLIC_API void*
dynlib_symbol_addr(void* handle, const char* name);

VH_PUBLIC_API int
dynlib_symbol_count(void* handle);

VH_PUBLIC_API const char*
dynlib_symbol_at(void* handle, int idx);

#if defined(_WIN32)

VH_PUBLIC_API int
dynlib_string_count(void* handle);

VH_PUBLIC_API struct str_view
dynlib_string_at(void* handle, int idx);

#endif

C_END
