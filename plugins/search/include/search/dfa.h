#pragma once

#include "search/range.h"
#include "vh/table.h"
#include "vh/vec.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct frame_data;
struct nfa_graph;

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
dfa_run(const struct dfa_table* dfa, const struct frame_data* fdata, struct range window);

#if defined(__cplusplus)
}
#endif
