#include "vh/hash40.h"
#include "vh/crc32.h"
#include <string.h>

uint64_t
hash40_buf(const void* buf, int len)
{
    uint64_t h40 = (uint64_t)len << 32;
    return h40 | crc32_buf(buf, len, 0);
}

uint64_t
hash40_cstr(const char* str)
{
    return hash40_buf((const void*)str, (int)strlen(str));
}
