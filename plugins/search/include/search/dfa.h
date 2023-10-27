#pragma once

#include "search/match.h"
#include "vh/table.h"
#include "vh/vec.h"

struct nfa_graph;

struct dfa_node
{
    struct matcher matcher;
    int next;
};

struct dfa_table
{
    struct table tt;
    struct vec tf;
};

struct frame_data
{
    union symbol* symbols;
};

struct range
{
    int start;
    int end;
};

int
dfa_compile(struct dfa_table* dfa, struct nfa_graph* nfa);

void
dfa_deinit(struct dfa_table* dfa);

int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name);

struct range
dfa_run(const struct dfa_table* dfa, const struct frame_data* fdata, struct range window);

typedef int (*dfa_asm_func)(int state, uint64_t symbol);

struct dfa_asm
{
    dfa_asm_func next_state;
    int size;
};

int
dfa_assemble(struct dfa_asm* assembly, const struct dfa_table* dfa);

void
dfa_asm_deinit(struct dfa_asm* assembly);

struct range
dfa_asm_run(const struct dfa_asm* assembly, const struct frame_data* fdata, struct range window);
