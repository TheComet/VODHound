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
dynlib_symbol_table(void* handle, struct strlist* sl);

VH_PUBLIC_API int
dynlib_symbol_table_filtered(
        void* handle,
        struct strlist* sl,
        int (*match)(struct str_view str, const void* data),
        const void* data);

VH_PUBLIC_API int
dynlib_symbol_count(void* handle);

VH_PUBLIC_API const char*
dynlib_symbol_at(void* handle, int idx);

VH_PUBLIC_API const char*
dynlib_last_error(void);

#if defined(_WIN32)

VH_PUBLIC_API int
dynlib_string_count(void* handle);

VH_PUBLIC_API struct str_view
dynlib_string_at(void* handle, int idx);

#endif

C_END
