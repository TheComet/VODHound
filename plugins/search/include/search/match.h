#pragma once

#include <stdint.h>

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

struct match
{
    uint64_t fighter_motion;
    uint16_t fighter_status;
    uint8_t flags;
};

static inline int
match_is_wildcard(const struct match* match)
{
    return !(
        (match->flags & MATCH_MOTION) ||
        (match->flags & MATCH_STATUS)
    );
}

static inline struct match
match_none(void)
{
    struct match m;
    m.fighter_motion = 0;
    m.fighter_status = 0;
    m.flags = 0;
    return m;
}

static inline struct match
match_motion(uint64_t motion)
{
    struct match m;
    m.fighter_motion = motion;
    m.fighter_status = 0;
    m.flags = MATCH_MOTION;
    return m;
}

static inline struct match
match_wildcard(void)
{
    struct match m;
    m.fighter_motion = 0;
    m.fighter_status = 0;
    m.flags = 0;
    return m;
}
