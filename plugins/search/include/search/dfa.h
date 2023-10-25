#pragma once

#include "search/match.h"
#include "vh/table.h"
#include "vh/vec.h"

struct nfa_graph;

struct dfa_node
{
    struct match match;
    int next;
};

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
