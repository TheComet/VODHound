#pragma once

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
    int next;
};

struct nfa_graph
{
    struct nfa_node* nodes;
    int* edges;
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

struct nfa_graph*
nfa_compile(const union ast_node* ast);

void
nfa_destroy(struct nfa_graph* nfa);

int
nfa_export_dot(const struct nfa_graph* nfa, const char* file_name);
