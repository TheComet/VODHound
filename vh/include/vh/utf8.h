#pragma once

#include "vh/config.h"

#include <stdio.h>

C_BEGIN

#if defined(_WIN32)

VH_PRIVATE_API wchar_t*
utf8_to_utf16(const char* utf8, int utf8_bytes);

VH_PRIVATE_API void
utf16_free(wchar_t* utf16);

#endif

VH_PUBLIC_API FILE*
fopen_utf8_wb(const char* utf8_filename, int len);

VH_PUBLIC_API int
remove_utf8(const char* utf8_filename, int len);

C_END
