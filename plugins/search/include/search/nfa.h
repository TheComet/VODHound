#pragma once

#include "search/match.h"
#include "vh/vec.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct ast;

struct nfa_node
{
    struct matcher matcher;
    struct vec next;  /* int - indices into nfa_graph->nodes[] */
};

struct nfa_graph
{
    struct nfa_node* nodes;
    int node_count;
};

static inline void
nfa_init(struct nfa_graph* nfa)
    { nfa->nodes = (struct nfa_node*)0; nfa->node_count = 0; }

void
nfa_deinit(struct nfa_graph* nfa);

int
nfa_compile(struct nfa_graph* nfa, const struct ast* ast);

#if defined(EXPORT_DOT)
int
nfa_export_dot(const struct nfa_graph* nfa, const char* file_name);
#else
#define nfa_export_dot(nfa, file_name)
#endif

#if defined(__cplusplus)
}
#endif
