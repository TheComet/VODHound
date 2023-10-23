#pragma once

#include "vh/config.h"
#include <stdint.h>

C_BEGIN

VH_PUBLIC_API uint64_t
hash40_buf(const void* buf, int len);

VH_PUBLIC_API uint64_t
hash40_str(const char* str);

C_END
