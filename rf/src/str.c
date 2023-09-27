#include "rf/str.h"
#include "rf/mem.h"

#include <string.h>

struct arena* arena_ensure(struct arena* a, int size)
{
    while (a->idx + size > a->size)
        a->data = realloc(
    a.size = size;
    a.idx = 0;
    a.data = malloc(size);
    return a;
}
void arena_free(struct arena* a)
{
    free(a->data);
}

struct string string_dup(struct arena* a, struct string other)
{
    struct string s;
    while (a->idx + other.len > a->size)
        a->data = rf_realloc(a->data, a->size * 2);

    s.off = a->idx;
    s.len = other.len;
    memcpy(a->data + a->off, other.begin, other.len);
    a->idx += other.len;

    return s;
}

int string_hex_to_u64(struct string str, uint64_t* out)
{
    int i = 0;
    uint64_t value = 0;
    if (str.begin[i] == '0' && (str.begin[i+1] == 'x' || str.begin[i+1] == 'X'))
        if ((i+=2) >= str.len)
            return -1;

    for (; i != str.len; ++i)
    {
        char b = str.begin[i];
        if (b >= '0' && b <= '9') value = (value << 4) | ((b - '0') & 0x0F);
        else if (b >= 'a' && b <= 'f') value = (value << 4) | ((b - 'a' + 10) & 0x0F);
        else if (b >= 'A' && b <= 'F') value = (value << 4) | ((b - 'A' + 10) & 0x0F);
        else return -1;
    }

    *out = value;
    return 0;
}
