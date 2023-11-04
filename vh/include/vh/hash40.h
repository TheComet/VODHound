#pragma once

#include "vh/config.h"
#include "vh/str.h"
#include <stdint.h>

C_BEGIN

VH_PUBLIC_API uint64_t
hash40_buf(const void* buf, int len);

VH_PUBLIC_API uint64_t
hash40_cstr(const char* str);

static inline uint64_t
hash40_str(struct str_view str) { return hash40_buf(str.data, str.len); }

C_END
