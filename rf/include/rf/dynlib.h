#pragma once

#include "rf/config.h"

C_BEGIN

RF_PUBLIC_API void*
dynlib_open(const char* file_name);

RF_PUBLIC_API void
dynlib_close(void* handle);

RF_PUBLIC_API void*
dynlib_lookup_symbol(void* handle, const char* name);

C_END
