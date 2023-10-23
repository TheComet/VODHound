#pragma once

#include "vh/config.h"
#include <stdint.h>

C_BEGIN

VH_PRIVATE_API void
crc32_init(void);

VH_PUBLIC_API uint32_t
crc32_buf(const void* buf, int len, uint32_t crc);

VH_PUBLIC_API uint32_t
crc32_str(const char* str, uint32_t crc);

C_END
