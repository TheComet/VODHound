#pragma once

#include "rf/config.h"

#include <stdint.h>

C_BEGIN

struct rf_str
{
    char* data;
    int len;
};

int
rf_str_hex_to_u64(struct rf_str str, uint64_t* out);

C_END
