#pragma once

#include "vh/config.h"

C_BEGIN

VH_PUBLIC_API void*
dynlib_open(const char* file_name);

VH_PUBLIC_API void
dynlib_close(void* handle);

VH_PUBLIC_API void*
dynlib_lookup_symbol(void* handle, const char* name);

C_END
