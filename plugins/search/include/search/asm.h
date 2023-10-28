#pragma once

#include "search/range.h"
#include <stdint.h>

struct dfa_table;
struct frame_data;

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
asm_run(const struct asm_dfa* assembly, const struct frame_data* fdata, struct range window);
