#pragma once

#include "vh/vec.h"
#include <stdint.h>

union ast_node;

enum nfa_flags
{
    NFA_MATCH_ACCEPT = 0x01,
    NFA_MATCH_MOTION = 0x02,
    NFA_MATCH_STATUS = 0x04,

    NFA_CTX_HIT      = 0x08,
    NFA_CTX_WHIFF    = 0x10,
    NFA_CTX_SHIELD   = 0x20,
    NFA_CTX_RISING   = 0x40,
    NFA_CTX_FALLING  = 0x80
};

struct nfa_state
{
    uint64_t fighter_motion;
    uint16_t fighter_status;
    uint8_t flags;
};

struct nfa_node
{
    struct nfa_state state;
    struct vec out;
};

struct nfa_graph
{
    struct nfa_node* nodes;
    int node_count;
};

static inline int
nfa_node_is_wildcard(const struct nfa_node* node)
{
    return !(
        (node->state.flags & NFA_MATCH_MOTION) ||
        (node->state.flags & NFA_MATCH_STATUS)
    );
}

int
nfa_compile(struct nfa_graph* nfa, const union ast_node* ast);

void
nfa_deinit(struct nfa_graph* nfa);

int
nfa_export_dot(const struct nfa_graph* nfa, const char* file_name);
