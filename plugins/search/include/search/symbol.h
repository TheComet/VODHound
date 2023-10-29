#pragma once

#include <stdint.h>

union symbol
{
    uint64_t u64;
    struct {
        /* hash40 value, 5 bytes */
        unsigned motionl          : 32;
        unsigned motionh          : 8;
        /* Status enum - largest value seen is 651 (?) from kirby -> 10 bits = 1024 values */
        unsigned status          : 10;
        /* Various flags that cannot be detected from regex alone */
        unsigned hitlag          : 1;
        unsigned hitstun         : 1;
        unsigned shieldlag       : 1;
        unsigned rising          : 1;
        unsigned falling         : 1;
        unsigned buried          : 1;
        unsigned phantom         : 1;
        /* same but for opponent */
        unsigned opp_hitlag      : 1;
        unsigned opp_hitstun     : 1;
        unsigned opp_shieldlag   : 1;
        unsigned opp_rising      : 1;
        unsigned opp_falling     : 1;
        unsigned opp_buried      : 1;
        unsigned opp_phantom     : 1;
    };
};
