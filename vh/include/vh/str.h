#pragma once

#include "vh/config.h"

#include <stdint.h>
#include <string.h>

C_BEGIN

struct str
{
    char* data;
    int len;
};

struct str_view
{
    const char* data;
    int len;
};

static inline struct str_view
str_view(struct str str)
{
    struct str_view view = {
        str.data,
        str.len
    };
    return view;
}
static inline struct str_view
cstr_view(const char* str)
{
    struct str_view view = {
        str,
        (int)strlen(str)
    };
    return view;
}

static inline int
str_ends_with(struct str_view str, struct str_view cmp)
{
    if (str.len < cmp.len)
        return 0;
    const char* off = str.data + str.len - cmp.len;
    return memcmp(off, cmp.data, cmp.len) == 0;
}
static inline int
cstr_ends_with(struct str_view str, const char* cmp)
{
    return str_ends_with(str, cstr_view(cmp));
}

static inline int
cstr_cmp(struct str_view str, const char* cstr)
{
    return memcmp(str.data, cstr, (size_t)str.len);
}

VH_PUBLIC_API int
str_hex_to_u64(struct str_view str, uint64_t* out);

C_END
