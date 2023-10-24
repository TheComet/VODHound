#include "search/dfa.h"
#include "search/nfa.h"

#include "vh/mem.h"

int
dfa_compile(struct dfa_graph* dfa, const struct nfa_graph* nfa)
{
    return 0;
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
