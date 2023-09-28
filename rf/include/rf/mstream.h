#pragma once

#include "rf/config.h"

#include <stdint.h>
#include <string.h>

C_BEGIN

struct rf_str_view;

struct rf_mstream
{
    void* address;
    int size;
    int idx;
};

static inline struct rf_mstream
rf_mstream_from_memory(void* address, int size)
{
    struct rf_mstream ms;
    ms.address = address;
    ms.size = size;
    ms.idx = 0;
    return ms;
}

#define rf_mstream_from_mfile(mf) \
        (rf_mstream_from_memory((mf)->address, (mf)->size))

static inline struct rf_mstream
rf_mstream_from_mstream(struct rf_mstream* ms, int offset, int size)
{
    return rf_mstream_from_memory((char*)ms->address + offset, size);
}

static inline int
rf_mstream_at_end(struct rf_mstream* ms)
{
    return ms->idx >= ms->size;
}

static inline int
rf_mstream_past_end(struct rf_mstream* ms)
{
    return ms->idx > ms->size;
}

static inline int
rf_mstream_bytes_left(struct rf_mstream* ms)
{
    return ms->size - ms->idx;
}

static inline uint8_t
rf_mstream_read_u8(struct rf_mstream* ms)
{
    return ((const uint8_t*)ms->address)[ms->idx++];
}

static inline char
rf_mstream_read_char(struct rf_mstream* ms)
{
    return ((const char*)ms->address)[ms->idx++];
}

static inline uint16_t
rf_mstream_read_lu16(struct rf_mstream* ms)
{
    uint16_t value;
    memcpy(&value, (const char*)ms->address + ms->idx, 2);
    ms->idx += 2;
    return value;
}

static inline uint32_t
rf_mstream_read_lu32(struct rf_mstream* ms)
{
    uint32_t value;
    memcpy(&value, (const char*)ms->address + ms->idx, 4);
    ms->idx += 4;
    return value;
}

static inline uint64_t
rf_mstream_read_lu64(struct rf_mstream* ms)
{
    uint64_t value;
    memcpy(&value, (const char*)ms->address + ms->idx, 8);
    ms->idx += 8;
    return value;
}

static inline float
rf_mstream_read_lf32(struct rf_mstream* ms)
{
    float value;
    memcpy(&value, (const char*)ms->address + ms->idx, 4);
    ms->idx += 4;
    return value;
}

static inline const void*
rf_mstream_read(struct rf_mstream* ms, int len)
{
    const void* data = (const char*)ms->address + ms->idx;
    ms->idx += len;
    return data;
}

static inline const void*
rf_mstream_ptr(struct rf_mstream* ms)
{
    return (const char*)ms->address + ms->idx;
}

RF_PUBLIC_API int
rf_mstream_read_string_until_delim(
        struct rf_mstream* ms, char delim, struct rf_str_view* str);

RF_PUBLIC_API int
rf_mstream_read_string_until_condition(
        struct rf_mstream* ms, int (*cond)(char), struct rf_str_view* str);
