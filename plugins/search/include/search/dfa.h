#pragma once

#include "search/match.h"

struct nfa_graph;

struct dfa_node
{
    struct match match;
    int next;
};

struct dfa_graph
{
    struct dfa_node* nodes;
    int* transitions;
    int node_count;
};

int
dfa_compile(struct dfa_graph* dfa, const struct nfa_graph* nfa);

void
dfa_deinit(struct dfa_graph* dfa);

int
dfa_export_dot(const struct dfa_graph* dfa, const char* file_name);
