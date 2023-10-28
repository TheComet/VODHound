#include "search/dfa.h"
#include "search/nfa.h"

#include "vh/btree.h"
#include "vh/str.h"
#include "vh/hm.h"
#include "vh/mem.h"

#include <stdio.h>
#include <inttypes.h>

static hash32
match_hm_hash(const void* data, int len)
{
    const struct matcher* m = data;
    hash32 a = (hash32)((m->symbol.u64 & m->mask.u64) >> 32UL);
    hash32 b = (hash32)((m->symbol.u64 & m->mask.u64) & 0xFFFFFFFF);
    return hash32_combine(a, b);
}

static int
match_hm_compare(const void* adata, const void* bdata, int size)
{
    (void)size;
    /* Inversion, because hashmap expects this to behavle like memcmp() */
    const struct matcher* a = adata;
    const struct matcher* b = bdata;
    return !((a->symbol.u64 & a->mask.u64) == (b->symbol.u64 & b->mask.u64));
}

static hash32
states_hm_hash(const void* data, int len)
{
    const struct vec* states = data;
    hash32 h = (hash32)*(int*)vec_back(states);
    int i = vec_count(states) - 1;
    while (i--)
        h = hash32_combine(h, (hash32)*(int*)vec_get(states, i));
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
        if (*(int*)vec_get(states_a, i) != *(int*)vec_get(states_b, i))
            return 1;
    return 0;
}

static void
print_nfa(const struct table* tt, const struct vec* tf)
{
    char buf[12];  /* -2147483648 */
    struct table tt_str;
    struct vec col_titles;
    struct vec col_widths;
    struct vec row_indices;
    int c, r;

    if (table_init(&tt_str, tt->rows, tt->cols, sizeof(struct str)) < 0)
        goto table_init_failed;
    for (c = 0; c != tt_str.cols; ++c)
        for (r = 0; r != tt_str.rows; ++r)
            str_init(table_get(&tt_str, r, c));

    vec_init(&col_titles, sizeof(struct str));
    vec_init(&col_widths, sizeof(int));
    vec_init(&row_indices, sizeof(int));

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
            if (str_fmt(title, "0x%" PRIx64, ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl)) < 0)
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

            VEC_FOR_EACH(cell, int, next)
                sprintf(buf, "%d", *next < 0 ? -*next : *next);
                if (cstr_join(s, ",", buf) < 0)
                    goto calc_text_failed;
            VEC_END_EACH

            if (*col_width < s->len)
                *col_width = s->len;
        }
    }

    for (r = 0; r != tt->rows; ++r)
        if (vec_push(&row_indices, &r) < 0)
            goto calc_text_failed;
    for (r = 0; r != tt->rows; ++r)
        for (c = 0; c != tt->cols; ++c)
        {
            struct vec* cell = table_get(tt, r, c);
            VEC_FOR_EACH(cell, int, next)
                if (*next < 0)
                {
                    int* row_idx = vec_get(&row_indices, -*next);
                    if (*row_idx > 0)
                        *row_idx = -*row_idx;
                }
            VEC_END_EACH
        }

    fprintf(stderr, " State ");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        struct str* s = vec_get(&col_titles, c);
        fprintf(stderr, "| %*s%.*s ", w - s->len, "", s->len, s->data);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "-------");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        fprintf(stderr, "+-");
        for (r = 0; r != w; ++r)
            fprintf(stderr, "-");
        fprintf(stderr, "-");
    }
    fprintf(stderr, "\n");
    for (r = 0; r != tt_str.rows; ++r)
    {
        int* row_idx = vec_get(&row_indices, r);
        if (*row_idx < 0)
        {
            sprintf(buf, "%d", -*row_idx);
            fprintf(stderr, " %*s*%s ", (int)(4 - strlen(buf)), "", buf);
        }
        else
            fprintf(stderr, " %*d ", 5, *row_idx);

        for (c = 0; c != tt_str.cols; ++c)
        {
            int w = *(int*)vec_get(&col_widths, c);
            struct str* s = table_get(&tt_str, r, c);
            fprintf(stderr, "| %*s%.*s ", w - s->len, "", s->len, s->data);
        }
        fprintf(stderr, "\n");
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
    return;
}

static void
print_dfa(const struct table* tt, const struct vec* tf)
{
    char buf[12];  /* -2147483648 */
    struct table tt_str;
    struct vec col_titles;
    struct vec col_widths;
    struct vec row_indices;
    int c, r;

    if (table_init(&tt_str, tt->rows, tt->cols, sizeof(struct str)) < 0)
        goto table_init_failed;
    for (r = 0; r != tt_str.rows; ++r)
        for (c = 0; c != tt_str.cols; ++c)
            str_init(table_get(&tt_str, r, c));

    vec_init(&col_titles, sizeof(struct str));
    vec_init(&col_widths, sizeof(int));
    vec_init(&row_indices, sizeof(int));

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
            if (str_fmt(title, "0x%" PRIx64, ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl)) < 0)
                goto calc_text_failed;
        }

        *col_width = 0;
        if (*col_width < title->len)
            *col_width = title->len;

        for (r = 0; r != tt->rows; ++r)
        {
            int* cell = table_get(tt, r, c);
            struct str* s = table_get(&tt_str, r, c);
            str_init(s);

            if (*cell != 0)
                if (str_fmt(s, "%d", *cell < 0 ? -*cell : *cell) < 0)
                    goto calc_text_failed;

            if (*col_width < s->len)
                *col_width = s->len;
        }
    }

    for (r = 0; r != tt->rows; ++r)
        if (vec_push(&row_indices, &r) < 0)
            goto calc_text_failed;
    for (r = 0; r != tt->rows; ++r)
        for (c = 0; c != tt->cols; ++c)
        {
            int* next= table_get(tt, r, c);
            if (*next < 0)
            {
                int* row_idx = vec_get(&row_indices, -*next);
                if (*row_idx > 0)
                    *row_idx = -*row_idx;
            }
        }

    fprintf(stderr, " State ");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        struct str* s = vec_get(&col_titles, c);
        fprintf(stderr, "| %*s%.*s ", w - s->len, "", s->len, s->data);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "-------");
    for (c = 0; c != vec_count(&col_titles); ++c)
    {
        int w = *(int*)vec_get(&col_widths, c);
        fprintf(stderr, "+-");
        for (r = 0; r != w; ++r)
            fprintf(stderr, "-");
        fprintf(stderr, "-");
    }
    fprintf(stderr, "\n");
    for (r = 0; r != tt_str.rows; ++r)
    {
        int* row_idx = vec_get(&row_indices, r);
        if (*row_idx < 0)
        {
            sprintf(buf, "%d", -*row_idx);
            fprintf(stderr, " %*s*%s ", (int)(4 - strlen(buf)), "", buf);
        }
        else
            fprintf(stderr, " %*d ", 5, *row_idx);

        for (c = 0; c != tt_str.cols; ++c)
        {
            int w = *(int*)vec_get(&col_widths, c);
            struct str* s = table_get(&tt_str, r, c);
            fprintf(stderr, "| %*s%.*s ", w - s->len, "", s->len, s->data);
        }
        fprintf(stderr, "\n");
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
    return;
}

static int
dfa_state_is_accept(const struct dfa_table* dfa, int state)
{
    int r, c;
    for (r = 0; r != dfa->tt.rows; ++r)
        for (c = 0; c != dfa->tt.cols; ++c)
        {
            const int* next = table_get(&dfa->tt, r, c);
            if (*next < 0 && -*next == state)
                return 1;
        }
    return 0;
}

static void
dfa_remove_duplicates(struct dfa_table* dfa)
{
    int r1, r2, c, r;
    for (r1 = 0; r1 < dfa->tt.rows; ++r1)
        for (r2 = r1 + 1; r2 < dfa->tt.rows; ++r2)
        {
            int is_accept1, is_accept2;
            for (c = 0; c != dfa->tt.cols; ++c)
            {
                int* cell1 = table_get(&dfa->tt, r1, c);
                int* cell2 = table_get(&dfa->tt, r2, c);
                if (*cell1 != *cell2)
                    goto skip_row;
            }

            /*
             * Have to additionally make sure that r1 and r2 are either both
             * accept conditions, or neither. It is invalid to merge states
             * only one of them is an accept condition.
             */
            if (dfa_state_is_accept(dfa, r1) != dfa_state_is_accept(dfa, r2))
                goto skip_row;

            /* Replace all references to r2 with r1, and decrement all references
             * above r2, since r2 is removed. */
            for (r = 0; r != dfa->tt.rows; ++r)
                for (c = 0; c != dfa->tt.cols; ++c)
                {
                    int* cell = table_get(&dfa->tt, r, c);
                    if (*cell == r2)
                        *cell = r1;
                    if (*cell == -r2)
                        *cell = -r1;

                    if (*cell < 0)
                    {
                        if (*cell < -r2)
                            (*cell)++;
                    }
                    else
                    {
                        if (*cell > r2)
                            (*cell)--;
                    }
                }

            table_remove_row(&dfa->tt, r2);
            r2--;
        skip_row:;
        }
}

int
dfa_compile(struct dfa_table* dfa, struct nfa_graph* nfa)
{
    struct hm nfa_unique_tf;
    struct hm dfa_unique_states;
    struct table nfa_tt;
    struct table dfa_tt_intermediate;
    int n, r, c;
    int success = -1;

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
         sizeof(struct matcher),
         sizeof(int),
         VH_HM_MIN_CAPACITY,
         match_hm_hash,
         match_hm_compare) < 0)
    {
        goto init_nfa_unique_tf_failed;
    }

    /* Skip node 0, as it merely acts as a container for all start states */
    vec_init(&dfa->tf, sizeof(struct matcher));
    for (n = 1; n != nfa->node_count; ++n)
    {
        int col = hm_count(&nfa_unique_tf);
        switch (hm_insert_new(&nfa_unique_tf, &nfa->nodes[n].matcher, &col))
        {
            case 1:
                if (vec_push(&dfa->tf, &nfa->nodes[n].matcher) < 0)
                    goto build_tfs_failed;
                break;
            case 0 : break;
            default: goto build_tfs_failed;
        }
    }

    /*
     * The transition table stores a list of states per cell. In this case,
     * each state is identified by an integer, which is an index into nfa->nodes,
     * or equivalently, a row index of the table.
     */
    if (table_init(&nfa_tt, nfa->node_count, hm_count(&nfa_unique_tf), sizeof(struct vec)) < 0)
        goto init_nfa_table_failed;
    for (r = 0; r != nfa_tt.rows; ++r)
        for (c = 0; c != nfa_tt.cols; ++c)
            vec_init(table_get(&nfa_tt, r, c), sizeof(int));

    /*
     * Insert transitions. It is necessary to somehow keep track of which states
     * are accept conditions. This is achieved by making the target state a
     * negative integer.
     */
    for (r = 0; r != nfa->node_count; ++r)
        VEC_FOR_EACH(&nfa->nodes[r].next, int, next)
            int* col = hm_find(&nfa_unique_tf, &nfa->nodes[*next].matcher);
            int next_state = nfa->nodes[*next].matcher.is_accept ? -*next : *next;
            struct vec* cell = table_get(&nfa_tt, r, *col);
            if (vec_push(cell, &next_state) < 0)
                goto build_nfa_table_failed;
        VEC_END_EACH

    fprintf(stderr, "NFA:\n");
    print_nfa(&nfa_tt, &dfa->tf);
    fprintf(stderr, "\n");

    /*
     * Unlike the NFA transition table, the DFA table's states are sets of
     * NFA states. These are tracked in this hashmap.
     */
    if (hm_init_with_options(&dfa_unique_states,
        sizeof(struct vec),
        sizeof(int),
        VH_HM_MIN_CAPACITY,
        states_hm_hash,
        states_hm_compare) < 0)
    {
        goto init_dfa_unique_states_failed;
    }
    if (table_init(&dfa_tt_intermediate, 1, nfa_tt.cols, sizeof(struct vec)) < 0)
        goto init_dfa_table_failed;
    for (c = 0; c != dfa_tt_intermediate.cols; ++c)
        vec_init(table_get(&dfa_tt_intermediate, 0, c), sizeof(int));

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
            /* Go through current row and see if any sets of NFA states form
             * a new DFA state. If yes, we append a new row to the table with
             * that new state and initialize all cells. */
            struct vec* dfa_state = table_get(&dfa_tt_intermediate, r, c);
            if (vec_count(dfa_state) == 0)
                continue;
            switch (hm_insert_new(&dfa_unique_states, dfa_state, &dfa_tt_intermediate.rows))
            {
                case 1: {
                    if (table_add_row(&dfa_tt_intermediate) < 0)
                        goto build_dfa_table_failed;
                    for (n = 0; n != dfa_tt_intermediate.cols; ++n)
                        vec_init(table_get(&dfa_tt_intermediate, dfa_tt_intermediate.rows - 1, n), sizeof(int));

                    /* For each cell in the new row, calculate transitions using data from NFA */
                    dfa_state = table_get(&dfa_tt_intermediate, r, c);  /* Adding a row may invalidate the pointer, get it again */
                    for (n = 0; n != dfa_tt_intermediate.cols; ++n)
                    {
                        VEC_FOR_EACH(dfa_state, int, nfa_state)
                            struct vec* nfa_cell = table_get(&nfa_tt, *nfa_state < 0 ? -*nfa_state : *nfa_state, n);
                            struct vec* dfa_cell = table_get(&dfa_tt_intermediate, dfa_tt_intermediate.rows - 1, n);
                            if (vec_push_vec(dfa_cell, nfa_cell) < 0)
                                goto build_dfa_table_failed;
                        VEC_END_EACH
                    }
                } break;

                case 0: break;
                case -1: goto build_dfa_table_failed;
            }
        }

    if (table_init(&dfa->tt, dfa_tt_intermediate.rows, dfa_tt_intermediate.cols, sizeof(int)) < 0)
        goto init_final_dfa_table_failed;
    for (r = 0; r != dfa_tt_intermediate.rows; ++r)
        for (c = 0; c != dfa_tt_intermediate.cols; ++c)
        {
            int* dfa_final_state = table_get(&dfa->tt, r, c);
            struct vec* dfa_state = table_get(&dfa_tt_intermediate, r, c);
            if (vec_count(dfa_state) == 0)
            {
                /*
                 * Normally, a DFA will have a "trap state" in cases where
                 * there is no matching input word. In our case, we want to
                 * stop execution when this happens. Since state 0 cannot be
                 * re-visited under normal operation, transitioning back to
                 * state 0 can be interpreted as halting the machine.
                 *
                 * The reason we cannot use negative numbers is because
                 * those are already used to indicate an accept state.
                 */
                *dfa_final_state = 0;
                continue;
            }
            int* dfa_row = hm_find(&dfa_unique_states, dfa_state);
            assert(dfa_row != NULL);

            /*
             * If any of the NFA states in this DFA state are marked as an
             * accept condition, then mark the DFA state as an accept condition
             * as well.
             */
            *dfa_final_state = *dfa_row;
            VEC_FOR_EACH(dfa_state, int, nfa_state)
                if (*nfa_state < 0)
                {
                    *dfa_final_state = -*dfa_row;
                    break;
                }
            VEC_END_EACH
        }

    fprintf(stderr, "DFA (duplicates):\n");
    print_dfa(&dfa->tt, &dfa->tf);
    dfa_remove_duplicates(dfa);

    fprintf(stderr, "DFA:\n");
    print_dfa(&dfa->tt, &dfa->tf);

    /* Success - This causes dfa->tf to not be freed */
    success = 0;

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
    if (success < 0)  /* If function succeeds, ownership of tf is transferred out of the function*/
        vec_deinit(&dfa->tf);
    hm_deinit(&nfa_unique_tf);
init_nfa_unique_tf_failed:
    return success;
}

void
dfa_deinit(struct dfa_table* dfa)
{
    table_deinit(&dfa->tt);
    vec_deinit(&dfa->tf);
}

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
        if (dfa_state_is_accept(dfa, r))
            fprintf(fp, ", shape=\"doublecircle\"");
        fprintf(fp, "];\n");
    }

    for (r = 0; r != dfa->tt.rows; ++r)
        for (c = 0; c != dfa->tt.cols; ++c)
        {
            const int* next = table_get(&dfa->tt, r, c);
            const struct matcher* m = vec_get(&dfa->tf, c);

            if (*next == 0)
                continue;

            if (r == 0)
                fprintf(fp, "start -> n%d [", *next < 0 ? -*next : *next);
            else
                fprintf(fp, "n%d -> n%d [", r, *next < 0 ? -*next : *next);

            fprintf(fp, "label=\"");
            if (matches_motion(m))
                fprintf(fp, "0x%" PRIx64, ((uint64_t)m->symbol.motionh << 32) | ((uint64_t)m->symbol.motionl));
            if (matches_status(m))
            {
                if (matches_motion(m))
                    fprintf(fp, ", ");
                fprintf(fp, ", %d", m->symbol.status);
            }
            if (matches_wildcard(m))
                fprintf(fp, "(.)");
            fprintf(fp, "\"];\n");
        }

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;

open_file_failed:
    return -1;
}

static int
do_match(const struct matcher* m, union symbol s)
{
    return (m->symbol.u64 & m->mask.u64) == (s.u64 & m->mask.u64);
}

static int
lookup_next_state(const struct dfa_table* dfa, union symbol s, int state)
{
    int c;
    for (c = 0; c != dfa->tt.cols; ++c)
    {
        struct matcher* m = vec_get(&dfa->tf, c);
        if (do_match(m, s))
            return *(int*)table_get(&dfa->tt, state, c);
    }
    return 0;
}

static int
dfa_run_single(const struct dfa_table* dfa, const struct frame_data* fdata, struct range r)
{
    int state;
    int idx;

    state = 0;
    for (idx = r.start; idx != r.end; idx++)
    {
        state = lookup_next_state(dfa, fdata->symbols[idx], state < 0 ? -state : state);

        /*
         * Transitioning to state 0 indicates the state machine has entered the
         * "trap state", i.e. no match was found.
         */
        if (state == 0)
            return r.start;

        /*
         * Negative states indicate an accept condition.
         * We want to match as much as possible, so if the state machine is
         * able to continue, then continue.
         */
        if (state < 0)
        {
            int next_state;

            /* Can't look ahead, so we're done (success) */
            if (idx+1 >= r.end)
                return idx + 1;

            next_state = lookup_next_state(dfa, fdata->symbols[idx+1], state < 0 ? -state : state);
            if (next_state == 0)
                return idx + 1;
        }
    }

    /*
     * Negative states indicate the current state is an accept condition.
     * Return the end of the matched range = last matched index + 1
     */
    if (state < 0)
        return idx + 1;

    /*
     * State machine has not completed, which means we only have a
     * partial match -> failure
     */
    return r.start;
}

struct range
dfa_run(const struct dfa_table* dfa, const struct frame_data* fdata, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = dfa_run_single(dfa, fdata, window);
        if (end > window.start)
        {
            window.end = end;
            break;
        }
    }

    return window;
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#include <stdarg.h>
#include <sys/mman.h>
#endif

static int
get_page_size(void)
{
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return sysconf(_SC_PAGESIZE);
#endif
}
static void*
alloc_page_rw(int size)
{
#if defined(_WIN32)
    return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
#else
    return mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}
static void
protect_rx(void* addr, int size)
{
#if defined(_WIN32)
    DWORD old_protect;
    BOOL result = VirtualProtect(addr, size, PAGE_EXECUTE_READ, &old_protect);
#else
    mprotect(addr, size, PROT_READ | PROT_EXEC);
#endif
}
static void
free_page(void* addr, int size)
{
#if defined(_WIN32)
    (void)size;
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, size);
#endif
}

static int
write_asm(struct vec* bytes, int n, ...)
{
    va_list va;
    va_start(va, n);
    while (n--)
    {
        uint8_t* byte = vec_emplace(bytes);
        if (byte == NULL)
            return -1;
        *byte = (uint8_t)va_arg(va, int);
    }
    return 0;
}
static int
write_asm_u64(struct vec* bytes, uint64_t value)
{
    int i = 8;
    while (i--)
    {
        uint8_t* byte = vec_emplace(bytes);
        if (byte == NULL)
            return -1;
        *byte = value & 0xFF;
        value >>= 8;
    }
    return 0;
}
static void
patch_asm_u32(struct vec* bytes, vec_size offset, uint32_t value)
{
    vec_size i;
    for (i = 0; i != 4; ++i)
    {
        *(uint8_t*)vec_get(bytes, offset + i) = value & 0xFF;
        value >>= 8;
    }
}
static int
write_asm_u32(struct vec* bytes, uint32_t value)
{
    int i = 4;
    while (i--)
    {
        uint8_t* byte = vec_emplace(bytes);
        if (byte == NULL)
            return -1;
        *byte = value & 0xFF;
        value >>= 8;
    }
    return 0;
}

static uint8_t RAX(void) { return 0; }   static uint8_t EAX(void) { return 0; }
static uint8_t RCX(void) { return 1; }   static uint8_t ECX(void) { return 1; }
static uint8_t RDX(void) { return 2; }   static uint8_t EDX(void) { return 2; }
static uint8_t RBX(void) { return 3; }   static uint8_t EBX(void) { return 3; }
static uint8_t RSP(void) { return 4; }   static uint8_t ESP(void) { return 4; }
static uint8_t RBP(void) { return 5; }   static uint8_t EBP(void) { return 5; }
static uint8_t RSI(void) { return 6; }   static uint8_t ESI(void) { return 6; }
static uint8_t RDI(void) { return 7; }   static uint8_t EDI(void) { return 7; }

static uint8_t SCALE4(void) { return 2; }

/* movabs */
static int MOV_r64_i64(struct vec* bytes, uint8_t reg, uint64_t value)
    { return write_asm(bytes, 2, 0x48, 0xb8 | reg) || write_asm_u64(bytes, value); }
/* mov: 0100 1000 1000 1001 11xx xyyy, xxx=from, yyy=to*/
static int MOV_r64_r64(struct vec* bytes, uint8_t dst, uint8_t src)
    { return write_asm(bytes, 3, 0x48, 0x89, 0xC0 | (src << 3) | dst); }
static int MOV_r32_r32(struct vec* bytes, uint8_t dst, uint8_t src)
    { return write_asm(bytes, 2, 0x89, 0xC0 | (src << 3) | dst); }
/* 1000 1011 0xxx 0100 ssyy yzzz, xxx=dst, yyy=offset, zzz=base, ss=scale */
static int MOV_r32_ptr_r64_base_offset_scale(struct vec* bytes, uint8_t dst, uint8_t base, uint8_t offset, uint8_t scale)
    { return write_asm(bytes, 3, 0x8B, 0x04 | (dst << 4), (scale << 6) | (offset << 3) | base); }
static int AND_r64_r64(struct vec* bytes, uint8_t dst, uint8_t src)
    { return write_asm(bytes, 3, 0x48, 0x21, 0xC0 | (src << 3) | dst); }
static int AND_r64_i32_sign_extend(struct vec* bytes, uint8_t reg, uint32_t value)
    { return write_asm(bytes, 3, 0x48, 0x81, 0xE0 | reg) || write_asm_u32(bytes, value); }
static int AND_r32_i32(struct vec* bytes, uint8_t reg, uint32_t value)
    { return write_asm(bytes, 2, 0x81, 0xE0 | reg) || write_asm_u32(bytes, value); }
static int CMP_r64_r64(struct vec* bytes, uint8_t dst, uint8_t src)
    { return write_asm(bytes, 3, 0x48, 0x39, 0xC0 | (src << 3) | dst); }
static int CMP_r32_i32(struct vec* bytes, uint8_t reg, uint32_t value)
    { return write_asm(bytes, 2, 0x81, 0xF8 | reg) || write_asm_u32(bytes, value); }
static int XOR_r32_r32(struct vec* bytes, uint8_t dst, uint8_t src)
    { return write_asm(bytes, 2, 0x31, 0xC0 | (src << 3) | dst); }
static int JE_rel8(struct vec* bytes, int8_t offset)
    { return write_asm(bytes, 2, 0x74, (uint8_t)(offset - 2)); }
static int JE_rel32(struct vec* bytes, int32_t dst)
    { return write_asm(bytes, 2, 0x0F, 0x84) || write_asm_u32(bytes, (uint32_t)(dst - 6)); }
static void JE_rel32_patch(struct vec* bytes, int32_t offset, int32_t dst)
    { patch_asm_u32(bytes, offset + 2, (uint32_t)(dst - 6)); }
/* 0100 1000 1000 1101 00xx x101, xxx=to */
static int LEA_r64_RSP_plus_i32(struct vec* bytes, uint8_t reg, uint32_t offset)
    { return write_asm(bytes, 3, 0x48, 0x8D, 0x05 | (reg << 3)) || write_asm_u32(bytes, offset); }
static int RET(struct vec* bytes)
    { return write_asm(bytes, 1, 0xC3); }
static int PUSH_r64(struct vec* bytes, uint8_t reg)
    { return write_asm(bytes, 1, 0x50 | reg); }
static int POP_r64(struct vec* bytes, uint8_t reg)
    { return write_asm(bytes, 1, 0x58 | reg); }

int
dfa_assemble(struct dfa_asm* assembly, const struct dfa_table* dfa)
{
    int r, c;
    int page_size = get_page_size();
    struct vec b;
    struct vec jump_offsets;

    vec_init(&b, sizeof(uint8_t));
    vec_init(&jump_offsets, sizeof(vec_size));

    if (vec_reserve(&b, page_size) < 0)
        goto push_failed;

    /*
     * Linux (System V AMD64 ABI):
     *   Integer args : RDI, RSI, RDX, RCX, R8, R9
     *   Volatile     :
     *
     * Windows:
     *   Integer args : RCX, RDX, R8, R9
     *   Volatile     : RAX, RCX, RDX, R8-R11, XMM0-XMM5
     */
#if defined(_MSC_VER)
    PUSH_r64(&b, RBX());
#endif

    for (c = 0; c != dfa->tt.cols; ++c)
    {
        const struct matcher* m = vec_get(&dfa->tf, c);

        /* if (matcher->symbol.u64 & matcher->mask.u64) == (input_symbol & matcher->mask.u64) */
#if defined(_MSC_VER)
        MOV_r64_i64(&b, RAX(), m->mask.u64);
        MOV_r64_i64(&b, RBX(), m->symbol.u64);
        AND_r64_r64(&b, RAX(), RDX());
        CMP_r64_r64(&b, RAX(), RBX());
#else
        MOV_r64_i64(&b, RAX(), m->mask.u64);
        MOV_r64_i64(&b, RDX(), m->symbol.u64);
        AND_r64_r64(&b, RAX(), RSI());
        CMP_r64_r64(&b, RAX(), RDX());
#endif
        vec_push(&jump_offsets, &vec_count(&b));  /* Don't know destination address of jump yet */
        JE_rel32(&b, 0);
    }

    /* Return 0 if nothing matched */
    XOR_r32_r32(&b, EAX(), EAX());
#if defined(_MSC_VER)
    POP_r64(&b, RBX());
#endif
    RET(&b);

    for (c = 0; c != dfa->tt.cols; ++c)
    {
        /* Patch in jump addresses from earlier */
        vec_size target = vec_count(&b);
        vec_size offset = *(vec_size*)vec_get(&jump_offsets, c);
        JE_rel32_patch(&b, offset, target - offset);

        /* next_state = lookup_table[current_state] */
#if defined(_MSC_VER)
        POP_r64(&b, RBX());
#endif
        XOR_r32_r32(&b, EAX(), EAX());
        MOV_r32_r32(&b, ECX(), ECX());  /* Clear upper 32 bits or RDI */
        LEA_r64_RSP_plus_i32(&b, RAX(), 4);
        MOV_r32_ptr_r64_base_offset_scale(&b, EAX(), RAX(), RCX(), SCALE4());
        RET(&b);

        /* Lookup table data */
        for (r = 0; r != dfa->tt.rows; ++r)
            write_asm_u32(&b, (uint32_t)*(int*)table_get(&dfa->tt, r, c));
    }

    FILE* fp = fopen("dump.bin", "w");
    fwrite(vec_data(&b), vec_count(&b), 1, fp);
    fclose(fp);

    while (page_size < vec_count(&b))
        page_size *= 2;

    void* mem = alloc_page_rw(page_size);
    memcpy(mem, vec_data(&b), vec_count(&b));
    protect_rx(mem, page_size);

    vec_deinit(&jump_offsets);
    vec_deinit(&b);
    assembly->next_state = (dfa_asm_func)mem;
    assembly->size = page_size;
    return 0;

push_failed:
    vec_deinit(&jump_offsets);
    vec_deinit(&b);
    return -1;
}

void
dfa_asm_deinit(struct dfa_asm* assembly)
{
    free_page((void*)assembly->next_state, assembly->size);
}

static int
dfa_asm_run_single(const struct dfa_asm* assembly, const struct frame_data* fdata, struct range r)
{
    int state;
    int idx;

    state = 0;
    for (idx = r.start; idx != r.end; idx++)
    {
        state = assembly->next_state(state < 0 ? -state : state, fdata->symbols[idx].u64);

        /*
         * Transitioning to state 0 indicates the state machine has entered the
         * "trap state", i.e. no match was found.
         */
        if (state == 0)
            return r.start;

        /*
         * Negative states indicate an accept condition.
         * We want to match as much as possible, so if the state machine is
         * able to continue, then continue.
         */
        if (state < 0)
        {
            int next_state;

            /* Can't look ahead, so we're done (success) */
            if (idx+1 >= r.end)
                return idx + 1;

            next_state = assembly->next_state(state < 0 ? -state : state, fdata->symbols[idx+1].u64);
            if (next_state == 0)
                return idx + 1;
        }
    }

    /*
     * Negative states indicate the current state is an accept condition.
     * Return the end of the matched range = last matched index + 1
     */
    if (state < 0)
        return idx + 1;

    /*
     * State machine has not completed, which means we only have a
     * partial match -> failure
     */
    return r.start;
}

struct range
dfa_asm_run(const struct dfa_asm* assembly, const struct frame_data* fdata, struct range window)
{
    for (; window.start != window.end; ++window.start)
    {
        int end = dfa_asm_run_single(assembly, fdata, window);
        if (end > window.start)
        {
            window.end = end;
            break;
        }
    }

    return window;
}
