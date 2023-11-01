#pragma once

#include "search/range.h"
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct dfa_table;
union symbol;
struct vec;

typedef int (*asm_func)(int state, uint64_t symbol);

struct asm_dfa
{
    asm_func next_state;
    int size;
};

int
asm_compile(struct asm_dfa* assembly, const struct dfa_table* dfa);

void
asm_deinit(struct asm_dfa* assembly);

struct range
asm_find_first(const struct asm_dfa* assembly, const union symbol* symbols, struct range window);

int
asm_find_all(struct vec* ranges, const struct asm_dfa* assembly, const union symbol* symbols, struct range window);

#if defined(__cplusplus)
}
#endif
