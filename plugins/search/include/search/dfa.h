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

union symbol
{
    struct {
        /* hash40 value, 5 bytes */
        uint64_t motion          : 40;
        /* Status enum - largest value seen is 651 (?) from kirby -> 10 bits = 1024 values */
        unsigned status          : 10;
        /* Various flags that cannot be detected from regex alone */
        unsigned hitlag          : 1;
        unsigned hitstun         : 1;
        unsigned shieldlag       : 1;
        unsigned rising          : 1;
        unsigned falling         : 1;
        unsigned buried          : 1;
        unsigned phantom         : 1;
        /* same but for opponent */
        unsigned opp_hitlag      : 1;
        unsigned opp_hitstun     : 1;
        unsigned opp_shieldlag   : 1;
        unsigned opp_rising      : 1;
        unsigned opp_falling     : 1;
        unsigned opp_buried      : 1;
        unsigned opp_phantom     : 1;
    };
    uint64_t u64;
};

struct frame_data
{
    union symbol* symbols;
};

struct range
{
    int start;
    int end;
};

int
dfa_compile(struct dfa_table* dfa, struct nfa_graph* nfa);

void
dfa_deinit(struct dfa_table* dfa);

int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name);

struct range
dfa_run(const struct dfa_table* dfa, const struct frame_data* fdata, struct range window);
