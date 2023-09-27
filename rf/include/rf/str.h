#pragma once

#include "rf/config.h"

#include <stdint.h>

C_BEGIN

struct rf_arena
{
    char* data;
    int len;
    int idx;
};

struct rf_str
{
    int16_t off;
    int16_t len;
};

struct rf_str
rf_str_dup(struct rf_arena* a1, const struct rf_arena* a2, );

struct rf_str
rf_str_dup(struct rf_str other);

int
string_hex_to_u64(struct rf_arena* a, struct rf_str str, uint64_t* out);

C_END
