#pragma once

#include <stdint.h>

union symbol
{
    uint64_t u64;
    struct {
        /* hash40 value, 5 bytes */
        unsigned motionl        : 32;
        unsigned motionh        : 8;
        /* Various flags that cannot be detected from regex alone */
        unsigned me_dead        : 1;
        unsigned me_hitlag      : 1;
        unsigned me_hitstun     : 1;
        unsigned me_shieldlag   : 1;
        unsigned me_rising      : 1;
        unsigned me_falling     : 1;
        unsigned me_buried      : 1;
        unsigned me_phantom     : 1;
        /* same but for opponent */
        unsigned op_dead        : 1;
        unsigned op_hitlag      : 1;
        unsigned op_hitstun     : 1;
        unsigned op_shieldlag   : 1;
        unsigned op_rising      : 1;
        unsigned op_falling     : 1;
        unsigned op_buried      : 1;
        unsigned op_phantom     : 1;
    };
};

static inline union symbol
symbol_make(
    uint64_t motion,
    char me_dead, char me_hitlag, char me_hitstun, char me_shieldlag, char me_rising, char me_falling, char me_buried, char me_phantom,
    char op_dead, char op_hitlag, char op_hitstun, char op_shieldlag, char op_rising, char op_falling, char op_buried, char op_phantom)
{
    union symbol s;
    s.motionl = (motion & 0xFFFFFFFF);
    s.motionh = (motion >> 32);

    s.me_dead      = (me_dead != 0);
    s.me_hitlag    = (me_hitlag != 0);
    s.me_hitstun   = (me_hitstun != 0);
    s.me_shieldlag = (me_shieldlag != 0);
    s.me_rising    = (me_rising != 0);
    s.me_falling   = (me_falling != 0);
    s.me_buried    = (me_buried != 0);
    s.me_phantom   = (me_phantom != 0);

    s.op_dead      = (op_dead != 0);
    s.op_hitlag    = (op_hitlag != 0);
    s.op_hitstun   = (op_hitstun != 0);
    s.op_shieldlag = (op_shieldlag != 0);
    s.op_rising    = (op_rising != 0);
    s.op_falling   = (op_falling != 0);
    s.op_buried    = (op_buried != 0);
    s.op_phantom   = (op_phantom != 0);

    return s;
}
