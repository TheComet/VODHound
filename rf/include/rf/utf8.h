#pragma once

#include "rf/config.h"

#include <stdio.h>

C_BEGIN

RF_PRIVATE_API wchar_t*
rf_utf8_to_utf16(const char* utf8, int utf8_bytes);

RF_PRIVATE_API void
rf_utf16_free(wchar_t* utf16);

RF_PUBLIC_API FILE*
rf_fopen_utf8_wb(const char* utf8_filename, int len);

RF_PUBLIC_API int
rf_remove_utf8(const char* utf8_filename, int len);

C_END
