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

#if defined(EXPORT_DOT)
int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name);
void
nfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name);
void
dfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name);
#else
#define dfa_export_dot(dfa, file_name)
#define nfa_export_table(tt, tf, file_name)
#define dfa_export_table(tt, tf, file_name)
#endif

struct range
dfa_find_first(const struct dfa_table* dfa, const union symbol* symbols, struct range window);

int
dfa_find_all(struct vec* ranges, const struct dfa_table* dfa, const union symbol* symbols, struct range window);

#if defined(__cplusplus)
}
#endif
