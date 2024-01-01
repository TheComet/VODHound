#include "search/dfa.h"
#include "search/nfa.h"
#include "search/state.h"

#include "vh/str.h"
#include "vh/hm.h"

#include <stdio.h>
#include <inttypes.h>

static hash32
match_hm_hash(const void* data, int len)
{
    const struct matcher* m = data;
    hash32 a = (hash32)((m->symbol.u64 & m->mask.u64) >> 32UL);
    hash32 b = (hash32)((m->symbol.u64 & m->mask.u64) & 0xFFFFFFFF);
    hash32 c = m->is_inverted;
    return hash32_combine(a, b);
}

static int
match_hm_compare(const void* adata, const void* bdata, int size)
{
    (void)size;
    const struct matcher* a = adata;
    const struct matcher* b = bdata;
    /* hashmap expects this to behave like memcmp() */
    return !(
        (a->symbol.u64 & a->mask.u64) == (b->symbol.u64 & b->mask.u64) &&
        a->is_inverted == b->is_inverted);
}

static hash32
states_hm_hash(const void* data, int len)
{
    const struct vec* states = data;
    hash32 h = (hash32)((union state*)vec_back(states))->data;
    int i = vec_count(states) - 1;
    while (i--)
        h = hash32_combine(h, (hash32)((union state*)vec_get(states, i))->data);
    return h;
}

static int
states_hm_compare(const void* a, const void* b, int size)
{
    const struct vec* states_a = a;
    const struct vec* states_b = b;
    int i = vec_count(states_a);
    if (vec_count(states_a) != vec_count(states_b))
        return 1;
    while (i--)
        if (((union state*)vec_get(states_a, i))->data != ((union state*)vec_get(states_b, i))->data)
            return 1;
    return 0;
}

#if defined(EXPORT_DOT)
void
nfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name)
{
    FILE* fp;
    char buf[12];  /* -2147483648 */
    struct table tt_str;
    struct vec col_titles;
    struct vec col_widths;
    struct vec row_indices;
    int c, r;

    fp = fopen(file_name, "w");
    if (fp == NULL)
        goto fopen_failed;

    if (table_init_with_size(&tt_str, tt->rows, tt->cols, sizeof(struct str)) < 0)
        goto table_init_failed;
    for (c = 0; c != tt_str.cols; ++c)
        for (r = 0; r != tt_str.rows; ++r)
            str_init(table_get(&tt_str, r, c));

    vec_init(&col_titles, sizeof(struct str));
    vec_init(&col_widths, sizeof(int));
    vec_init(&row_indices, sizeof(union state));

    for (c = 0; c != tt->cols; ++c)
    {
        struct matcher* m = vec_get(tf, c);
        struct str* title = vec_emplace(&col_titles);
        int* col_width = vec_emplace(&col_widths);
        if (title == NULL || col_width == NULL)
            goto calc_text_failed;

        str_init(title);
        if (matches_wildcard(m))
        {
            if (cstr_set(title, ".") < 0)
                goto calc_text_failed;
        }
        else
        {
            if (str_fmt(title, "%s0x%" PRIx64,
                    m->is_inverted ? "!" : "",
                    ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl)) < 0)
                goto calc_text_failed;
        }

        *col_width = 0;
        if (*col_width < title->len)
            *col_width = title->len;

        for (r = 0; r != tt->rows; ++r)
        {
            struct vec* cell = table_get(tt, r, c);
            struct str* s = table_get(&tt_str, r, c);
            str_init(s);

            VEC_FOR_EACH(cell, union state, next)
                sprintf(buf, "%d", next->idx);
                if (cstr_join(s, ",", buf) < 0)
                    goto calc_text_failed;
            VEC_END_EACH

            if (*col_width < s->len)
                *col_width = s->len;
        }
    }

    for (r = 0; r != tt->rows; ++r)
    {
        union state row_idx = make_state(r, 0, 0);
        if (vec_push(&row_indices, &row_idx) < 0)
            goto calc_text_failed;
    }
    for (r = 0; r != tt->rows; ++r)
        for (c = 0; c != tt->cols; ++c)
        {
            struct vec* cell = table_get(tt, r, c);
            VEC_FOR_EACH(cell, union state, next)
                if (next->is_accept)
                    ((union state*)vec_get(&row_indices, next->idx))->is_accept = 1;
            VEC_END_EACH
        }

    fprintf(fp, " State ");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        struct str* s = vec_get(&col_titles, c);
        fprintf(fp, "| %*s%.*s ", w - s->len, "", s->len, s->data);
    }
    fprintf(fp, "\n");
    fprintf(fp, "-------");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        fprintf(fp, "+-");
        for (r = 0; r != w; ++r)
            fprintf(fp, "-");
        fprintf(fp, "-");
    }
    fprintf(fp, "\n");
    for (r = 0; r != tt_str.rows; ++r)
    {
        union state* row_idx = vec_get(&row_indices, r);
        if (row_idx->is_accept)
        {
            sprintf(buf, "%d", row_idx->idx);
            fprintf(fp, " %*s*%s ", (int)(4 - strlen(buf)), "", buf);
        }
        else
            fprintf(fp, " %*d ", 5, row_idx->idx);

        for (c = 0; c != tt_str.cols; ++c)
        {
            int w = *(int*)vec_get(&col_widths, c);
            struct str* s = table_get(&tt_str, r, c);
            fprintf(fp, "| %*s%.*s ", w - s->len, "", s->len, s->data);
        }
        fprintf(fp, "\n");
     }

calc_text_failed:
    vec_deinit(&row_indices);
    VEC_FOR_EACH(&col_titles, struct str, title)
        str_deinit(title);
    VEC_END_EACH
    vec_deinit(&col_titles);
    vec_deinit(&col_widths);

    for (c = 0; c != tt_str.cols; ++c)
        for (r = 0; r != tt_str.rows; ++r)
            str_deinit(table_get(&tt_str, r, c));
    table_deinit(&tt_str);
table_init_failed:
    fclose(fp);
fopen_failed:
    return;
}

void
dfa_export_table(const struct table* tt, const struct vec* tf, const char* file_name)
{
    FILE* fp;
    char int_str[12];  /* -2147483648 */
    struct table tt_str;
    struct vec col_titles;
    struct vec col_widths;
    struct vec row_indices;
    int c, r;

    fp = fopen(file_name, "w");
    if (fp == NULL)
        goto fopen_failed;

    if (table_init_with_size(&tt_str, tt->rows, tt->cols, sizeof(struct str)) < 0)
        goto table_init_failed;
    for (r = 0; r != tt_str.rows; ++r)
        for (c = 0; c != tt_str.cols; ++c)
            str_init(table_get(&tt_str, r, c));

    vec_init(&col_titles, sizeof(struct str));
    vec_init(&col_widths, sizeof(int));
    vec_init(&row_indices, sizeof(union state));

    for (c = 0; c != tt->cols; ++c)
    {
        struct matcher* m = vec_get(tf, c);
        struct str* title = vec_emplace(&col_titles);
        int* col_width = vec_emplace(&col_widths);
        if (title == NULL || col_width == NULL)
            goto calc_text_failed;

        str_init(title);
        if (matches_wildcard(m))
        {
            if (cstr_set(title, ".") < 0)
                goto calc_text_failed;
        }
        else
        {
            if (str_fmt(title, "%s0x%" PRIx64,
                    m->is_inverted ? "!" : "",
                    ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl)) < 0)
                goto calc_text_failed;
        }

        *col_width = 0;
        if (*col_width < title->len)
            *col_width = title->len;

        for (r = 0; r != tt->rows; ++r)
        {
            union state* cell = table_get(tt, r, c);
            struct str* s = table_get(&tt_str, r, c);
            str_init(s);

            if (!state_is_trap(*cell))
                if (str_fmt(s, "%d", cell->idx) < 0)
                    goto calc_text_failed;

            if (*col_width < s->len)
                *col_width = s->len;
        }
    }

    for (r = 0; r != tt->rows; ++r)
    {
        union state row_idx = make_state(r, 0, 0);
        if (vec_push(&row_indices, &row_idx) < 0)
            goto calc_text_failed;
    }
    for (r = 0; r != tt->rows; ++r)
        for (c = 0; c != tt->cols; ++c)
        {
            union state* next = table_get(tt, r, c);
            if (next->is_accept)
                ((union state*)vec_get(&row_indices, next->idx))->is_accept = 1;
        }

    fprintf(fp, " State ");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        struct str* s = vec_get(&col_titles, c);
        fprintf(fp, "| %*s%.*s ", w - s->len, "", s->len, s->data);
    }
    fprintf(fp, "\n");
    fprintf(fp, "-------");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        fprintf(fp, "+-");
        for (r = 0; r != w; ++r)
            fprintf(fp, "-");
        fprintf(fp, "-");
    }
    fprintf(fp, "\n");
    for (r = 0; r != tt_str.rows; ++r)
    {
        union state* row_idx = vec_get(&row_indices, r);
        if (row_idx->is_accept)
        {
            sprintf(int_str, "%d", row_idx->idx);
            fprintf(fp, " %*s*%s ", (int)(4 - strlen(int_str)), "", int_str);
        }
        else
            fprintf(fp, " %*d ", 5, row_idx->idx);

        for (c = 0; c != tt_str.cols; ++c)
        {
            int w = *(int*)vec_get(&col_widths, c);
            struct str* s = table_get(&tt_str, r, c);
            fprintf(fp, "| %*s%.*s ", w - s->len, "", s->len, s->data);
        }
        fprintf(fp, "\n");
    }

calc_text_failed:
    vec_deinit(&row_indices);
    VEC_FOR_EACH(&col_titles, struct str, title)
        str_deinit(title);
    VEC_END_EACH
    vec_deinit(&col_titles);
    vec_deinit(&col_widths);

    for (c = 0; c != tt_str.cols; ++c)
        for (r = 0; r != tt_str.rows; ++r)
            str_deinit(table_get(&tt_str, r, c));
    table_deinit(&tt_str);
table_init_failed:
    fclose(fp);
fopen_failed:
    return;
}
#endif

static int
dfa_state_idx_is_accept(const struct table* dfa_tt, int idx)
{
    int r, c;
    for (r = 0; r != dfa_tt->rows; ++r)
        for (c = 0; c != dfa_tt->cols; ++c)
        {
            const union state* next = table_get(dfa_tt, r, c);
            if (next->idx == idx && next->is_accept)
                return 1;
        }
    return 0;
}

static int
dfa_state_idx_is_inverted(const struct table* dfa_tt, int idx)
{
    int r, c;
    for (r = 0; r != dfa_tt->rows; ++r)
        for (c = 0; c != dfa_tt->cols; ++c)
        {
            const union state* next = table_get(dfa_tt, r, c);
            if (next->idx == idx && next->is_inverted)
                return 1;
        }
    return 0;
}

static void
dfa_remove_duplicates(struct table* dfa_tt, struct vec* tf)
{
    int r1, r2, c, r;
    for (r1 = 0; r1 < dfa_tt->rows; ++r1)
        for (r2 = r1 + 1; r2 < dfa_tt->rows; ++r2)
        {
            for (c = 0; c != dfa_tt->cols; ++c)
            {
                const union state* cell1 = table_get(dfa_tt, r1, c);
                const union state* cell2 = table_get(dfa_tt, r2, c);
                if (cell1->idx != cell2->idx)
                    goto skip_row;
            }

            /*
             * Have to additionally make sure that r1 and r2 are either both
             * accept conditions, or neither. It is invalid to merge states
             * where only one of them is an accept condition.
             */
            if (dfa_state_idx_is_accept(dfa_tt, r1) != dfa_state_idx_is_accept(dfa_tt, r2))
                goto skip_row;

            /* Replace all references to r2 with r1, and decrement all references
             * above r2, since r2 is removed. */
            for (r = 0; r != dfa_tt->rows; ++r)
                for (c = 0; c != dfa_tt->cols; ++c)
                {
                    union state* cell = table_get(dfa_tt, r, c);
                    if (cell->idx == r2)
                        cell->idx = r1;

                    if ((int)cell->idx > r2)
                        cell->idx--;
                }

            table_remove_row(dfa_tt, r2);
            r2--;
        skip_row:;
        }
}

int
dfa_from_nfa(struct dfa_table* dfa, struct nfa_graph* nfa)
{
    struct hm nfa_unique_tf;
    struct hm dfa_unique_states;
    struct table nfa_tt;
    struct table dfa_tt_intermediate;
    struct table dfa_tt;
    struct vec dfa_tf;
    int n, r, c;
    int has_wildcards = 0;
    int return_code = -1;

    /*
     * In an error case, the table and transition functions should be empty to
     * prevent dfa_find_* to execute anything. The data will then be freed.
     */
    dfa_tt = dfa->tt;
    dfa_tf = dfa->tf;
    vec_init(&dfa->tf, dfa->tf.element_size);
    table_init(&dfa->tt, dfa->tt.element_size);

    /*
     * Purpose of this hashmap is to create a set of unique transition functions,
     * which form the columns of the transition table being built. Loop through
     * all nodes and insert their transition functions (in this case they're
     * called "matchers").
     *
     * Since each column of the table is associated with a unique matcher, the
     * matchers are stored in a separate vector "tf", indexed by column.
     *
     * The hash and compare functions will ignore the MATCH_ACCEPT bit. The
     * information for whether a state is an accept condition is encoded into the
     * transitions as negative indices.
     */
    if (hm_init_with_options(&nfa_unique_tf,
         sizeof(struct matcher),  /* transition function */
         sizeof(int),             /* index into the "nfa->nodes[]" array as well as index into the "nfa_tt", "dfa_tt" columns and "dfa_tf" vector */
         VH_HM_MIN_CAPACITY,
         match_hm_hash,
         match_hm_compare) < 0)
    {
        goto init_nfa_unique_tf_failed;
    }

    vec_clear(&dfa_tf);
    for (n = 1; n != nfa->node_count; ++n)  /* Skip node 0, as it merely acts */
    {                                       /* a container for all start states */
        int* nfa_tf_idx;
        switch (hm_insert(&nfa_unique_tf, &nfa->nodes[n].matcher, (void**)&nfa_tf_idx))
        {
            case 1:
                *nfa_tf_idx = hm_count(&nfa_unique_tf) - 1;
                if (vec_push(&dfa_tf, &nfa->nodes[n].matcher) < 0)
                    goto build_tfs_failed;
                break;
            case 0 : break;
            default: goto build_tfs_failed;
        }
    }

    /*
     * Put the wildcard matcher (if it exists) as the last column. This
     * will cause the wildcard to be evaluated/executed last.
     */
    HM_FOR_EACH(&nfa_unique_tf, struct matcher, int, matcher1, col1)
        if (matches_wildcard(matcher1))
            HM_FOR_EACH(&nfa_unique_tf, struct matcher, int, matcher2, col2)
                if (*col2 == hm_count(&nfa_unique_tf) - 1)
                {
                    struct matcher tmp_matcher;

                    /* swap values in hashmap */
                    int tmp = *col1;
                    *col1 = *col2;
                    *col2 = tmp;

                    /* swap entries in transition function vector */
                    matcher1 = vec_get(&dfa_tf, *col1);
                    matcher2 = vec_get(&dfa_tf, *col2);
                    tmp_matcher = *matcher1;
                    *matcher1 = *matcher2;
                    *matcher2 = tmp_matcher;

                    has_wildcards = 1;
                    goto wildcard_swapped_to_end;
                }
            HM_END_EACH
    HM_END_EACH
wildcard_swapped_to_end:;

    /*
     * The transition table stores a list of states per cell. A "state" encodes
     * whether it is an accept/deny condition and it encodes an index into the
     * nfa->nodes array, or equivalently, a row index of the table. See "union states"
     * for details.
     */
    if (table_init_with_size(&nfa_tt, nfa->node_count, hm_count(&nfa_unique_tf), sizeof(struct vec)) < 0)
        goto init_nfa_table_failed;
    for (r = 0; r != nfa_tt.rows; ++r)
        for (c = 0; c != nfa_tt.cols; ++c)
            vec_init(table_get(&nfa_tt, r, c), sizeof(union state));

    for (r = 0; r != nfa->node_count; ++r)
        VEC_FOR_EACH(&nfa->nodes[r].next, int, next)
            int* col = hm_find(&nfa_unique_tf, &nfa->nodes[*next].matcher);
            union state next_state = make_state(
                *next,
                nfa->nodes[*next].matcher.is_accept,
                nfa->nodes[*next].matcher.is_inverted);
            struct vec* cell = table_get(&nfa_tt, r, *col);
            if (vec_push(cell, &next_state) < 0)
                goto build_nfa_table_failed;
        VEC_END_EACH

    nfa_export_table(&nfa_tt, &dfa_tf, "nfa.txt");

    /*
     * DFA cannot handle wildcards as-is, because it would require generating
     * states for all possible negated matches. Consider: "a->.?->c", rewritten
     * "(a->c) | (a->.->c)". If the input sequence is "acc", then depending on
     * the order, this could either match "ac" or "acc".
     *
     * To deal with this, if a state has an outgoing wildcard transition, then
     * we insert explicit transitions for all other possible state transitions
     * in parallel with the wildcard. In other words: "(a->c) | (a->c->c) | (a->.->c)".
     *
     * This way, when evaluating the DFA, as long as the wildcard is processed
     * last, it will prefer transitioning through known symbols before it falls
     * back to matching the wildcard on an unknown symbol.
     */
    if (has_wildcards)
        for (r = 0; r != nfa_tt.rows; ++r)
        {
            const struct vec* wildcard_next_states = table_get(&nfa_tt, r, nfa_tt.cols - 1);
            for (c = 0; c < nfa_tt.cols - 1; ++c)
            {
                int c2;
                struct vec* next_states = table_get(&nfa_tt, r, c);
                const struct matcher* m = vec_get(&dfa_tf, c);

                if (vec_count(next_states) == 0)
                    continue;

                if (m->is_inverted)
                    continue;


                /*for (c2 = 0; c2 < nfa_tt.cols - 1; ++c2)
                {
                    const struct matcher* m1 = vec_get(&dfa_tf, c);
                    const struct matcher* m2 = vec_get(&dfa_tf, c2);
                    if (c != c2 &&
                        m1->is_inverted != m2->is_inverted &&
                        m1->mask.u64 == m2->mask.u64 &&
                        m1->symbol.u64 == m2->symbol.u64)
                    {
                        goto matcher_is_negated;
                    }
                }*/

                VEC_FOR_EACH(wildcard_next_states, union state, wildcard_next_state)
                    if (vec_find(next_states, wildcard_next_state) == vec_count(next_states))
                        if (vec_push(next_states, wildcard_next_state) < 0)
                            goto build_nfa_table_failed;
                VEC_END_EACH

                matcher_is_negated: continue;
            }
        }
    nfa_export_table(&nfa_tt, &dfa_tf, "nfa_wc.txt");

    /*
     * Unlike the NFA transition table, the DFA table's states are sets of
     * NFA states. These are tracked in this hashmap.
     */
    if (hm_init_with_options(&dfa_unique_states,
        sizeof(struct vec),  /* DFA state == set of NFA states */
        sizeof(int),         /* index into the "dfa_tf" vector as well as index into the "dfa_tt" column */
        VH_HM_MIN_CAPACITY,
        states_hm_hash,
        states_hm_compare) < 0)
    {
        goto init_dfa_unique_states_failed;
    }
    if (table_init_with_size(&dfa_tt_intermediate, 1, nfa_tt.cols, sizeof(struct vec)) < 0)
        goto init_dfa_table_failed;
    for (c = 0; c != dfa_tt_intermediate.cols; ++c)
        vec_init(table_get(&dfa_tt_intermediate, 0, c), sizeof(union state));

    /* Initial row in DFA is simply the first row from the NFA table */
    for (c = 0; c != nfa_tt.cols; ++c)
    {
        struct vec* nfa_cell = table_get(&nfa_tt, 0, c);
        struct vec* dfa_cell = table_get(&dfa_tt_intermediate, 0, c);
        if (vec_push_vec(dfa_cell, nfa_cell) < 0)
            goto build_dfa_table_failed;
    }

    for (r = 0; r != dfa_tt_intermediate.rows; ++r)
        for (c = 0; c != dfa_tt_intermediate.cols; ++c)
        {
            int* dfa_tf_idx;
            /* Go through current row and see if any sets of NFA states form
             * a new DFA state. If yes, we append a new row to the table with
             * that new state and initialize all cells. */
            struct vec* dfa_state = table_get(&dfa_tt_intermediate, r, c);
            if (vec_count(dfa_state) == 0)
                continue;
            switch (hm_insert(&dfa_unique_states, dfa_state, (void**)&dfa_tf_idx))
            {
                case 1: {
                    *dfa_tf_idx = dfa_tt_intermediate.rows;
                    if (table_add_row(&dfa_tt_intermediate) < 0)
                        goto build_dfa_table_failed;
                    for (n = 0; n != dfa_tt_intermediate.cols; ++n)
                        vec_init(table_get(&dfa_tt_intermediate, dfa_tt_intermediate.rows - 1, n), sizeof(union state));

                    /* For each cell in the new row, calculate transitions using data from NFA */
                    dfa_state = table_get(&dfa_tt_intermediate, r, c);  /* Adding a row may invalidate the pointer, get it again */
                    for (n = 0; n != dfa_tt_intermediate.cols; ++n)
                    {
                        VEC_FOR_EACH(dfa_state, union state, nfa_state)
                            struct vec* nfa_cell = table_get(&nfa_tt, nfa_state->idx, n);
                            struct vec* dfa_cell = table_get(&dfa_tt_intermediate, dfa_tt_intermediate.rows - 1, n);
                            VEC_FOR_EACH(nfa_cell, union state, target_nfa_state)
                                if (vec_find(dfa_cell, target_nfa_state) == vec_count(dfa_cell))
                                    if (vec_push(dfa_cell, target_nfa_state) < 0)
                                        goto build_dfa_table_failed;
                            VEC_END_EACH
                        VEC_END_EACH
                    }
                } break;

                case 0: break;
                default: goto build_dfa_table_failed;
            }
        }

    if (table_resize(&dfa_tt, dfa_tt_intermediate.rows, dfa_tt_intermediate.cols) < 0)
        goto init_final_dfa_table_failed;
    for (r = 0; r != dfa_tt_intermediate.rows; ++r)
        for (c = 0; c != dfa_tt_intermediate.cols; ++c)
        {
            union state* dfa_final_state = table_get(&dfa_tt, r, c);
            const struct vec* dfa_state = table_get(&dfa_tt_intermediate, r, c);
            if (vec_count(dfa_state) == 0)
            {
                /*
                 * Normally, a DFA will have a "trap state" in cases where
                 * there is no matching input word. In our case, we want to
                 * stop execution when this happens. Since state 0 cannot be
                 * re-visited under normal operation, transitioning back to
                 * state 0 can be interpreted as halting the machine.
                 */
                *dfa_final_state = make_trap_state();
                continue;
            }
            const int* dfa_row = hm_find(&dfa_unique_states, dfa_state);
            assert(dfa_row != NULL);

            *dfa_final_state = make_state(*dfa_row, 0, 0);

            /*
             * If any of the NFA states in this DFA state are marked as an
             * accept condition, then mark the DFA state as an accept condition
             * as well.
             */
            VEC_FOR_EACH(dfa_state, union state, nfa_state)
                if (nfa_state->is_accept && !nfa_state->is_inverted)
                {
                    dfa_final_state->is_accept = 1;
                    break;
                }
            VEC_END_EACH
        }

    dfa_export_table(&dfa_tt, &dfa_tf, "dfa_dups.txt");
    dfa_remove_duplicates(&dfa_tt, &dfa_tf);
    dfa_export_table(&dfa_tt, &dfa_tf, "dfa.txt");

    /* Success! */
    return_code = 0;
    vec_steal_vector(&dfa->tf, &dfa_tf);
    table_steal_table(&dfa->tt, &dfa_tt);

    table_deinit(&dfa_tt);
init_final_dfa_table_failed:
build_dfa_table_failed:
    for (r = 0; r != dfa_tt_intermediate.rows; ++r)
        for (c = 0; c != dfa_tt_intermediate.cols; ++c)
            vec_deinit(table_get(&dfa_tt_intermediate, r, c));
    table_deinit(&dfa_tt_intermediate);
init_dfa_table_failed:
    hm_deinit(&dfa_unique_states);
init_dfa_unique_states_failed:
build_nfa_table_failed:
    for (r = 0; r != nfa_tt.rows; ++r)
        for (c = 0; c != nfa_tt.cols; ++c)
            vec_deinit(table_get(&nfa_tt, r, c));
    table_deinit(&nfa_tt);
init_nfa_table_failed:
build_tfs_failed:
    vec_deinit(&dfa_tf);
    hm_deinit(&nfa_unique_tf);
init_nfa_unique_tf_failed:
    return return_code;
}

void
dfa_init(struct dfa_table* dfa)
{
    vec_init(&dfa->tf, sizeof(struct matcher));
    table_init(&dfa->tt, sizeof(union state));
}

void
dfa_deinit(struct dfa_table* dfa)
{
    table_deinit(&dfa->tt);
    vec_deinit(&dfa->tf);
}

#if defined(EXPORT_DOT)
int
dfa_export_dot(const struct dfa_table* dfa, const char* file_name)
{
    int r, c;
    FILE* fp = fopen(file_name, "w");
    if (fp == NULL)
        goto open_file_failed;

    fprintf(fp, "digraph {\n");
    fprintf(fp, "start [shape=\"point\", label=\"\", width=\"0.25\"];\n");

    for (r = 1; r < dfa->tt.rows; ++r)
    {
        fprintf(fp, "n%d [label=\"%d\"", r, r);
        if (dfa_state_idx_is_accept(&dfa->tt, r))
            fprintf(fp, ", shape=\"doublecircle\"");
        fprintf(fp, "];\n");
    }

    for (r = 0; r != dfa->tt.rows; ++r)
        for (c = 0; c != dfa->tt.cols; ++c)
        {
            const union state* next = table_get(&dfa->tt, r, c);
            const struct matcher* m = vec_get(&dfa->tf, c);

            if (state_is_trap(*next))
                continue;

            if (r == 0)
                fprintf(fp, "start -> n%d [", next->idx);
            else
                fprintf(fp, "n%d -> n%d [", r, next->idx);

            fprintf(fp, "label=\"");
            if (m->is_inverted)
                fprintf(fp, "!");
            if (matches_motion(m))
                fprintf(fp, "0x%" PRIx64, ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl));
            else if (matches_wildcard(m))
                fprintf(fp, "(.)");
            fprintf(fp, "\"];\n");
        }

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;

open_file_failed:
    return -1;
}
#endif

static int
do_match(const struct matcher* m, union symbol s)
{
    return (m->symbol.u64 & m->mask.u64) == (s.u64 & m->mask.u64);
}

static union state
lookup_next_state(const struct dfa_table* dfa, union symbol s, union state state)
{
    int c;
    for (c = 0; c != dfa->tt.cols; ++c)
    {
        struct matcher* m = vec_get(&dfa->tf, c);
        if (do_match(m, s))
            return *(union state*)table_get(&dfa->tt, state.idx, c);
    }

    return make_trap_state();
}

static int
dfa_run(const struct dfa_table* dfa, const union symbol* symbols, struct range r)
{
    int idx;
    int last_accept_idx = r.start;
    union state state = make_trap_state();

    for (idx = r.start; idx != r.end; idx++)
    {
        state = lookup_next_state(dfa, symbols[idx], state);

        /*
         * Transitioning to state 0 indicates the state machine has entered the
         * "trap state", i.e. no match was found for the current symbol. Stop
         * execution.
         */
        if (state_is_trap(state))
            break;

        /*
         * Accept condition.
         * We want to match as much as possible, so instead of returning
         * immediately here, save this index as the last known accept condition.
         */
        if (state.is_accept)
            last_accept_idx = idx + 1;
    }

    return last_accept_idx;
}

struct range
dfa_find_first(const struct dfa_table* dfa, const union symbol* symbols, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = dfa_run(dfa, symbols, window);
        if (end > window.start)
        {
            window.end = end;
            break;
        }
    }

    return window;
}

int
dfa_find_all(struct vec* ranges, const struct dfa_table* dfa, const union symbol* symbols, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = dfa_run(dfa, symbols, window);
        if (end > window.start)
        {
            struct range* r = vec_emplace(ranges);
            if (r == NULL)
                return -1;
            r->start = window.start;
            r->end = end;
            window.start = end - 1;
        }
    }

    return 0;
}
