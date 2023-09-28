#include "rf/str.h"

#include <string.h>

int
rf_str_hex_to_u64(struct rf_str_view str, uint64_t* out)
{
    int i = 0;
    uint64_t value = 0;
    if (str.data[i] == '0' && (str.data[i+1] == 'x' || str.data[i+1] == 'X'))
        if ((i+=2) >= str.len)
            return -1;

    for (; i != str.len; ++i)
    {
        char b = str.data[i];
        if (b >= '0' && b <= '9') value = (value << 4) | ((b - '0') & 0x0F);
        else if (b >= 'a' && b <= 'f') value = (value << 4) | ((b - 'a' + 10) & 0x0F);
        else if (b >= 'A' && b <= 'F') value = (value << 4) | ((b - 'A' + 10) & 0x0F);
        else return -1;
    }

    *out = value;
    return 0;
}
