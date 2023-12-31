#pragma once

#include "search/symbol.h"

struct matcher
{
    union symbol symbol;
    union symbol mask;
    unsigned is_accept   : 1;
    unsigned is_inverted : 1;
};

static inline int
matches_wildcard(const struct matcher* m)
{
    return m->mask.u64 == 0;
}
static inline int
matches_motion(const struct matcher* m)
{
    return m->mask.motionl != 0;
}

static inline struct matcher
match_none(void)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.is_accept = 0;
    m.is_inverted = 0;
    return m;
}

static inline struct matcher
match_motion(uint64_t motion, char is_inverted)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.mask.motionl = 0xFFFFFFFF;
    m.mask.motionh = 0xFF;
    m.symbol.motionl = motion & 0xFFFFFFFF;
    m.symbol.motionh = motion >> 32;
    m.is_accept = 0;
    m.is_inverted = is_inverted;
    return m;
}

static inline struct matcher
match_wildcard(char is_inverted)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.is_accept = 0;
    m.is_inverted = is_inverted;
    return m;
}
