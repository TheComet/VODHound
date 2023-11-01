#pragma once

#include "search/range.h"
#include "search/symbol.h"
#include "vh/vec.h"

union symbol;
struct frame_data;

struct search_index
{
    struct vec fighters;  /* struct fighter_index */
};

void
search_index_init(struct search_index* index);

void
search_index_deinit(struct search_index* index);

int
search_index_build(struct search_index* index, const struct frame_data* fdata);

void
search_index_clear(struct search_index* index);

static inline int
search_index_has_data(struct search_index* index)
    { return vec_count(&index->fighters) > 0; }

static inline int
search_index_fighter_count(struct search_index* index)
    { return vec_count(&index->fighters); }

int
search_index_symbol_count(struct search_index* index, int fighter_idx);

const union symbol*
search_index_symbols(const struct search_index* index, int fighter_idx);

static inline struct range
search_index_range(struct search_index* index, int fighter_idx)
{
    struct range r;
    r.start = 0;
    r.end = search_index_symbol_count(index, fighter_idx);
    return r;
}
