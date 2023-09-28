#include "rf/mstream.h"
#include "rf/str.h"

int
rf_mstream_read_string_until_delim(struct rf_mstream* ms, char delim, struct rf_str_view* str)
{
    const char* data = ms->address;
    str->data = &data[ms->idx];
    str->len = 0;
    for (; ms->idx + str->len != ms->size; ++str->len)
        if (str->data[str->len] == delim)
        {
            ms->idx += str->len + 1;
            return 0;
        }

    return -1;
}

int
rf_mstream_read_string_until_condition(struct rf_mstream* ms, int (*cond)(char), struct rf_str_view* str)
{
    const char* data = ms->address;
    str->data = &data[ms->idx];
    str->len = 0;
    for (; ms->idx + str->len != ms->size; ++str->len)
        if (cond(str->data[str->len]))
        {
            ms->idx += str->len + 1;
            while (!rf_mstream_at_end(ms) && cond(data[ms->idx]))
                ++ms->idx;
            return 0;
        }

    return -1;
}
