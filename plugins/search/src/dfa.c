#include "search/dfa.h"
#include "search/nfa.h"

#include "vh/hm.h"
#include "vh/mem.h"

static hash32
match_hash(const void* key, int len)
{
    struct nfa_node* node = key;
    hash32 a = node->match.fighter_motion & 0xFFFFFFFF;
    hash32 b = (node->match.fighter_status << 16) | node->match.flags;
    return hash32_combine(a, b);
}

int
dfa_compile(struct dfa_graph* dfa, const struct nfa_graph* nfa)
{
    struct vec dfa_nodes;
    struct hm unique_nodes;
    int n;

    if (hm_init_with_options(&unique_nodes, sizeof(struct nfa_node), sizeof(int), 64, match_hash) < 0)
        goto init_hm_failed;

    vec_init(&dfa_nodes, sizeof(struct dfa_node));

    for (n = 0; n != nfa->node_count; ++n)
    {
        VEC_FOR_EACH(&nfa->nodes[n].next, int, next)
            const struct nfa_node* nfa_node = &nfa->nodes[*next];
            switch (hm_insert(&unique_nodes, nfa_node, *next))
            {
                case 1: {
                    struct dfa_node* dfa_node = vec_emplace(&dfa_nodes);
                    dfa_node->match = nfa_node->match;
                } break;
            }
        VEC_END_EACH
    }

init_hm_failed:
    return -1;
}

void
dfa_deinit(struct dfa_graph* dfa)
{
    mem_free(dfa->nodes);
    mem_free(dfa->transitions);
}

int
dfa_export_dot(const struct dfa_graph* dfa, const char* file_name)
{
    return 0;
}
