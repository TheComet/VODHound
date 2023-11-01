#pragma once

#include "search/range.h"
#include "vh/table.h"
#include "vh/vec.h"

#if defined(__cplusplus)
extern "C" {
#endif

union symbol;
struct nfa_graph;
struct vec;

struct dfa_table
{
    struct table tt;
    struct vec tf;
};

int
dfa_compile(struct dfa_table* dfa, struct nfa_graph* nfa);

void
dfa_deinit(struct dfa_table* dfa);

int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name);

struct range
dfa_find_first(const struct dfa_table* dfa, const union symbol* symbols, struct range window);

int
dfa_find_all(struct vec* ranges, const struct dfa_table* dfa, const union symbol* symbols, struct range window);

#if defined(__cplusplus)
}
#endif
