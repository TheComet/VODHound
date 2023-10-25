#include "search/dfa.h"
#include "search/nfa.h"

#include "vh/btree.h"
#include "vh/str.h"
#include "vh/hm.h"
#include "vh/mem.h"

#include <stdio.h>
#include <inttypes.h>

struct table
{
    void* data;
    int rows;
    int cols;
    int element_size;
    int capacity;
};

static int
table_init(struct table* table, int rows, int cols, unsigned int element_size)
{
    table->rows = rows;
    table->cols = cols;
    table->element_size = element_size;
    table->capacity = rows * cols * element_size;
    table->data = mem_alloc(table->capacity);
    if (table->data == NULL)
        return -1;
    return 0;
}

static void
table_deinit(struct table* table)
{
    mem_free(table->data);
}

static int
table_add_row(struct table* table)
{
    int row_size = table->cols * table->element_size;
    int table_size = table->rows * table->cols * table->element_size;
    while (table_size + row_size > table->capacity)
    {
        void* new_data = mem_realloc(table->data, table_size * 2);
        if (new_data == NULL)
            return -1;
        table->data = new_data;
        table->capacity = table_size * 2;
    }

    table->rows++;

    return 0;
}

static void*
table_get(const struct table* table, int row, int col)
{
    int offset = (row * table->cols + col) * table->element_size;
    return (void*)((char*)table->data + offset);
}

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

static int
dfa_calc_tfs_for_row(struct table* dfa_tt, const struct vec* dfa_state, const struct table* nfa_tt)
{
    int c;
    int r = dfa_tt->rows - 1;

    for (c = 0; c != dfa_tt->cols; ++c)
    {
        VEC_FOR_EACH(dfa_state, int, nfa_state)
            struct vec* nfa_cell = table_get(nfa_tt, *nfa_state, c);
            struct vec* dfa_cell = table_get(dfa_tt, r, c);
            vec_push_vec(dfa_cell, nfa_cell);
        VEC_END_EACH
    }

    return 0;
}

int
dfa_compile(struct dfa_graph* dfa, struct nfa_graph* nfa)
{
    struct vec dfa_nodes;
    struct vec tf;
    struct btree visited;
    struct hm nfa_unique_tf;
    struct table nfa_tt;
    struct table dfa_tt;
    struct table dfa_tt_final;
    struct hm dfa_unique_states;
    int n, r, c;
    const int term = -1;

    /*
     * Purpose of this hashmap is to create a set of unique transition functions,
     * which form the columns of the transition table being built. Loop through
     * all nodes and insert their transition functions (in this case they're
     * called "matchers").
     *
     * Since each column of the table is associated with a unique matcher, the
     * matchers are stored in a separate vector "tf", indexed by column.
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
    vec_init(&tf, sizeof(struct match));
    for (n = 1; n != nfa->node_count; ++n)
    {
        int col = hm_count(&nfa_unique_tf);
        switch (hm_insert_new(&nfa_unique_tf, &nfa->nodes[n].match, &col))
        {
            case 1:
                if (vec_push(&tf, &nfa->nodes[n].match) < 0)
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

    for (r = 0; r != nfa->node_count; ++r)
        VEC_FOR_EACH(&nfa->nodes[r].next, int, next)
            int* col = hm_find(&nfa_unique_tf, &nfa->nodes[*next].match);
            int next_state = (nfa->nodes[*next].match.flags & MATCH_ACCEPT) ? -*next : *next;
            struct vec* cell = table_get(&nfa_tt, r, *col);
            if (vec_push(cell, &next_state) < 0)
                goto build_nfa_table_failed;
        VEC_END_EACH

    fprintf(stderr, "NFA:\n");
    print_nfa(&nfa_tt, &tf);
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
    if (table_init(&dfa_tt, 1, nfa_tt.cols, sizeof(struct vec)) < 0)
        goto init_dfa_table_failed;
    for (c = 0; c != dfa_tt.cols; ++c)
        vec_init(table_get(&dfa_tt, 0, c), sizeof(int));

    for (c = 0; c != nfa_tt.cols; ++c)
    {
        struct vec* nfa_cell = table_get(&nfa_tt, 0, c);
        struct vec* dfa_cell = table_get(&dfa_tt, 0, c);
        if (vec_push_vec(dfa_cell, nfa_cell) < 0)
            goto build_dfa_table_failed;
    }

    for (r = 0; r != dfa_tt.rows; ++r)
    {
        for (c = 0; c != dfa_tt.cols; ++c)
        {
            /* Go through current row and see if any sets of NFA states form
             * a new DFA state. If yes, we append a new row to the table with
             * that new state and initialize all cells. */
            struct vec* dfa_state = table_get(&dfa_tt, r, c);
            if (vec_count(dfa_state) == 0)
                continue;
            switch (hm_insert_new(&dfa_unique_states, dfa_state, &dfa_tt.rows))
            {
                case 1: {
                    if (table_add_row(&dfa_tt) < 0)
                        goto build_dfa_table_failed;
                    for (n = 0; n != dfa_tt.cols; ++n)
                        vec_init(table_get(&dfa_tt, dfa_tt.rows - 1, n), sizeof(int));

                    /* For each cell in the new row, calculate transitions using data from NFA */
                    dfa_state = table_get(&dfa_tt, r, c);  /* Adding a row may invalidate the pointer, get it again */
                    for (n = 0; n != dfa_tt.cols; ++n)
                    {
                        VEC_FOR_EACH(dfa_state, int, nfa_state)
                            struct vec* nfa_cell = table_get(&nfa_tt, *nfa_state < 0 ? -*nfa_state : *nfa_state, n);
                            struct vec* dfa_cell = table_get(&dfa_tt, dfa_tt.rows - 1, n);
                            if (vec_push_vec(dfa_cell, nfa_cell) < 0)
                                goto build_dfa_table_failed;
                        VEC_END_EACH
                    }
                } break;

                case 0: break;
                case -1: goto build_dfa_table_failed;
            }
        }
    }
    //print_nfa(&dfa_tt, &tf, &dfa_unique_states);

    if (table_init(&dfa_tt_final, dfa_tt.rows, dfa_tt.cols, sizeof(int)) < 0)
        goto init_final_dfa_table_failed;
    for (r = 0; r != dfa_tt.rows; ++r)
        for (c = 0; c != dfa_tt.cols; ++c)
        {
            int* dfa_final_state = table_get(&dfa_tt_final, r, c);
            struct vec* dfa_state = table_get(&dfa_tt, r, c);
            if (vec_count(dfa_state) == 0)
            {
                /* 
                 * Normally, a DFA will have a "trap state" to transition to
                 * in case where is no matching input word. In our case, we
                 * want to stop execution when this happens. Since state 0
                 * cannot be re-visited under normal operation, transitioning
                 * back to state 0 can be interpreted as halting the machine.
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

    fprintf(stderr, "DFA:\n");
    print_dfa(&dfa_tt_final, &tf);

    table_deinit(&dfa_tt_final);
init_final_dfa_table_failed:
build_dfa_table_failed:
    for (r = 0; r != dfa_tt.rows; ++r)
        for (c = 0; c != dfa_tt.cols; ++c)
            vec_deinit(table_get(&dfa_tt, r, c));
    table_deinit(&dfa_tt);
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
    vec_deinit(&tf);
    hm_deinit(&nfa_unique_tf);
init_nfa_unique_tf_failed:
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
