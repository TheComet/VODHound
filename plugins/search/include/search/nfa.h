#pragma once

#include "search/match.h"
#include "vh/vec.h"

#if defined(__cplusplus)
extern "C" {
#endif

union ast_node;

struct nfa_node
{
    struct matcher matcher;
    struct vec next;
};

struct nfa_graph
{
    struct nfa_node* nodes;
    int node_count;
};

int
nfa_compile(struct nfa_graph* nfa, const union ast_node* ast);

void
nfa_deinit(struct nfa_graph* nfa);

int
nfa_export_dot(const struct nfa_graph* nfa, const char* file_name);

#if defined(__cplusplus)
}
#endif
