#include "rf/mstream.h"

int
mstream_read_string_until_delim(struct mstream* ms, char delim, struct string* str)
{
    const char* data = ms->address;
    str->begin = &data[ms->idx];
    str->len = 0;
    for (; ms->idx + str->len != ms->size; ++str->len)
        if (str->begin[str->len] == delim)
        {
            ms->idx += str->len + 1;
            return 0;
        }

    return -1;
}

int
mstream_read_string_until_condition(struct mstream* ms, int (*cond)(char), struct string* str)
{
    const char* data = ms->address;
    str->begin = &data[ms->idx];
    str->len = 0;
    for (; ms->idx + str->len != ms->size; ++str->len)
        if (cond(str->begin[str->len]))
        {
            ms->idx += str->len + 1;
            while (!mstream_at_end(ms) && cond(data[ms->idx]))
                ++ms->idx;
            return 0;
        }

    return -1;
}
