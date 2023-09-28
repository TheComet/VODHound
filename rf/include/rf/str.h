#pragma once

#include "rf/config.h"

#include <stdint.h>
#include <string.h>

C_BEGIN

struct rf_str
{
    char* data;
    int len;
};

struct rf_str_view
{
    const char* data;
    int len;
};

static inline struct rf_str_view
rf_str_view(struct rf_str str)
{
    struct rf_str_view view = {
        str.data,
        str.len
    };
    return view;
}

static inline struct rf_str_view
rf_cstr_view(const char* str)
{
    struct rf_str_view view = {
        str,
        (int)strlen(str)
    };
    return view;
}

RF_PUBLIC_API int
rf_str_hex_to_u64(struct rf_str_view str, uint64_t* out);

C_END
