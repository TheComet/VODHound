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

VH_PUBLIC_API int
str_hex_to_u64(struct str_view str, uint64_t* out);

C_END
