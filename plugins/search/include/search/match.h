#pragma once

#include "search/symbol.h"

enum match_flags
{
    MATCH_ACCEPT       = 0x01,
    MATCH_MOTION       = 0x02,
    MATCH_STATUS       = 0x04,

    MATCH_CTX_HIT      = 0x08,
    MATCH_CTX_WHIFF    = 0x10,
    MATCH_CTX_SHIELD   = 0x20,
    MATCH_CTX_RISING   = 0x40,
    MATCH_CTX_FALLING  = 0x80
};

struct matcher
{
    union symbol symbol;
    union symbol mask;
    char is_accept;
};

static inline int
matches_wildcard(const struct matcher* m)
{
    return m->mask.u64 == 0;
}
static inline int
matches_motion(const struct matcher* m)
{
    return m->mask.motion != 0;
}
static inline int
matches_status(const struct matcher* m)
{
    return m->mask.status != 0;
}

static inline struct matcher
match_none(void)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.is_accept = 0;
    return m;
}

static inline struct matcher
match_motion(uint64_t motion)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.mask.motion = 0xFFFFFFFFFF;
    m.symbol.motion = motion;
    m.is_accept = 0;
    return m;
}

static inline struct matcher
match_wildcard(void)
{
    struct matcher m;
    m.mask.u64 = 0;
    m.symbol.u64 = 0;
    m.is_accept = 0;
    return m;
}
