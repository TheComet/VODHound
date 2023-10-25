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
    const uint8_t ignore_flags = MATCH_ACCEPT;
    const struct match* match = data;
    hash32 crc32 = match->fighter_motion & 0xFFFFFFFF;
    return hash32_combine(crc32, (match->fighter_status << 16) | (match->flags & ~ignore_flags));
}

static int
match_hm_compare(const void* adata, const void* bdata, int size)
{
    (void)size;
    /* Inversion, because hashmap expects this to behavle like memcmp() */
    uint8_t ignore_flags = MATCH_ACCEPT;
    const struct match* a = adata;
    const struct match* b = bdata;
    return !(
        a->fighter_motion == b->fighter_motion &&
        a->fighter_status == b->fighter_status &&
        (a->flags & ~ignore_flags) == (b->flags & ~ignore_flags));
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
        struct match* match = vec_get(tf, c);
        struct str* title = vec_emplace(&col_titles);
        int* col_width = vec_emplace(&col_widths);
        if (title == NULL || col_width == NULL)
            goto calc_text_failed;

        str_init(title);
        if (str_fmt(title, "0x%" PRIx64, match->fighter_motion) < 0)
            goto calc_text_failed;

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
        struct match* match = vec_get(tf, c);
        struct str* title = vec_emplace(&col_titles);
        int* col_width = vec_emplace(&col_widths);
        if (title == NULL || col_width == NULL)
            goto calc_text_failed;

        str_init(title);
        if (str_fmt(title, "0x%" PRIx64, match->fighter_motion) < 0)
            goto calc_text_failed;

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

static void
dfa_remove_duplicates(struct dfa_table* dfa)
{
    int r1, r2, c, r;
    for (r1 = 0; r1 < dfa->tt.rows; ++r1)
        for (r2 = r1 + 1; r2 < dfa->tt.rows; ++r2)
        {
            for (c = 0; c != dfa->tt.cols; ++c)
            {
                int* cell1 = table_get(&dfa->tt, r1, c);
                int* cell2 = table_get(&dfa->tt, r2, c);
                if (*cell1 != *cell2)
                    goto skip_row;
            }

            /* Replace all references to r2 with r1 */
            for (r = 0; r != dfa->tt.rows; ++r)
                for (c = 0; c != dfa->tt.cols; ++c)
                {
                    int* cell = table_get(&dfa->tt, r, c);
                    if (*cell == r2)
                        *cell = r1;
                    if (*cell == -r2)
                        *cell = -r1;
                }

            table_remove_row(&dfa->tt, r2);
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
         sizeof(struct match),
         sizeof(int),
         VH_HM_MIN_CAPACITY,
         match_hm_hash,
         match_hm_compare) < 0)
    {
        goto init_nfa_unique_tf_failed;
    }

    /* Skip node 0, as it merely acts as a container for all start states */
    vec_init(&dfa->tf, sizeof(struct match));
    for (n = 1; n != nfa->node_count; ++n)
    {
        int col = hm_count(&nfa_unique_tf);
        switch (hm_insert_new(&nfa_unique_tf, &nfa->nodes[n].match, &col))
        {
            case 1:
                if (vec_push(&dfa->tf, &nfa->nodes[n].match) < 0)
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
            int* col = hm_find(&nfa_unique_tf, &nfa->nodes[*next].match);
            int next_state = (nfa->nodes[*next].match.flags & MATCH_ACCEPT) ? -*next : *next;
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
            const struct match* match = vec_get(&dfa->tf, c);

            if (*next == 0)
                continue;

            if (r == 0)
                fprintf(fp, "start -> n%d [", *next < 0 ? -*next : *next);
            else
                fprintf(fp, "n%d -> n%d [", r, *next < 0 ? -*next : *next);

            fprintf(fp, "label=\"");
            if (match->flags & MATCH_MOTION)
                fprintf(fp, "0x%" PRIx64, match->fighter_motion);
            if (match->flags & MATCH_STATUS)
            {
                if (match->flags & MATCH_MOTION)
                    fprintf(fp, ", ");
                fprintf(fp, ", %d", match->fighter_status);
            }
            if (match_is_wildcard(match))
                fprintf(fp, ".");
            fprintf(fp, "\"];\n");
        }

    fprintf(fp, "}\n");
    fclose(fp);
    return 0;

open_file_failed:
    return -1;
}
