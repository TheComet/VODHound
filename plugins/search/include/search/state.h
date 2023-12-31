#pragma once

#include <stdint.h>

union state
{
    /* Data is organized such that unnecessary upper bytes can be cut
     * away. This is used in asm_x86_64.c to generate more efficient
     * jump tables that use up less space in memory */
    struct {
        unsigned is_accept   : 1;
        unsigned is_inverted : 1;
        unsigned idx         : 30;
    };
    uint32_t data;
};

static inline union state
make_state(int idx, char is_accept, char is_inverted)
{
    union state s;
    s.idx = idx;
    s.is_inverted = is_inverted;
    s.is_accept = is_accept;
    return s;
}

static inline union state
make_trap_state(void)
{
    union state s = { 0 };
    return s;
}

static inline int
state_is_trap(union state state) { return state.data == 0; }
