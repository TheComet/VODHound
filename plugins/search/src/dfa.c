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
match_hm_compare(const void* a, const void* b, int size)
{
    /* Inversion, because hashmap expects this to behavle like memcmp() */
    return !match_equal(a, b);
}

static void
print_transition_table(const struct hm* tf, const struct nfa_graph* nfa)
{
    char buf[19];  /* 64-bit hex = 16 chars + "0x" + null */
    struct vec columns;
    struct vec col_titles;
    struct vec col_widths;
    int row_count;
    int c, r;
    vec_init(&columns, sizeof(struct vec));
    vec_init(&col_titles, sizeof(struct str));
    vec_init(&col_widths, sizeof(int));

    row_count = 0;
    HM_FOR_EACH(tf, struct match, struct vec, match, entry)
        struct vec* rows = vec_emplace(&columns);
        struct str* title = vec_emplace(&col_titles);
        int* col_width = vec_emplace(&col_widths);

        vec_init(rows, sizeof(struct str));
        str_init(title);
        str_fmt(title, "0x%" PRIx64, match->fighter_motion);

        *col_width = 0;
        if (*col_width < title->len)
            *col_width = title->len;

        VEC_FOR_EACH(entry, struct vec, states)
            struct str* s = vec_emplace(rows);
            str_init(s);
            VEC_FOR_EACH(states, int, idx)
                sprintf(buf, "%d", *idx);
                cstr_join(s, ",", buf);
            VEC_END_EACH
            if (*col_width < s->len)
                *col_width = s->len;
        VEC_END_EACH
    HM_END_EACH

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
    for (r = 0; r != vec_count((struct vec*)vec_get(&columns, 0)); ++r)
    {
        fprintf(stderr, "%*s%d ", 5, "", r);
        for (c = 0; c != vec_count(&columns); ++c)
        {
            int w = *(int*)vec_get(&col_widths, c);
            struct str* s = vec_get(vec_get(&columns, c), r);
            fprintf(stderr, "| %*s%.*s ", w - s->len, "", s->len, s->data);
        }
        fprintf(stderr, "\n");
     }

    VEC_FOR_EACH(&columns, struct vec, rows)
        VEC_FOR_EACH(rows, struct str, s)
            str_deinit(s);
        VEC_END_EACH
        vec_deinit(rows);
    VEC_END_EACH
    vec_deinit(&columns);

    VEC_FOR_EACH(&col_titles, struct str, title)
        str_deinit(title);
    VEC_END_EACH
    vec_deinit(&col_titles);

    vec_deinit(&col_widths);
}

int
dfa_compile(struct dfa_graph* dfa, struct nfa_graph* nfa)
{
    struct vec dfa_nodes;
    struct vec transitions;
    struct vec nfa_prev;
    struct btree visited;
    struct hm tf;
    struct vec template_tf_entry;
    int n;
    const int term = -1;

    if (hm_init_with_options(&tf, sizeof(struct match), sizeof(struct vec), VH_HM_MIN_CAPACITY, match_hm_hash, match_hm_compare) < 0)
        goto hm_init_failed;

    vec_init(&template_tf_entry, sizeof(struct vec));
    for (n = 0; n != nfa->node_count; ++n)
    {
        VEC_FOR_EACH(&nfa->nodes[n].next, int, next)
            struct vec* tf_entry = hm_insert_or_get(&tf, &nfa->nodes[*next].match, &template_tf_entry);
            while (vec_count(tf_entry) < n + 1)
            {
                struct vec* states = vec_emplace(tf_entry);
                vec_init(states, sizeof(int));
            }
            struct vec* states = vec_get(tf_entry, n);
            vec_push(states, next);
        VEC_END_EACH

        HM_FOR_EACH(&tf, struct match, struct vec, match, entry)
            while (vec_count(entry) < n + 1)
            {
                struct vec* states = vec_emplace(entry);
                vec_init(states, sizeof(int));
            }
        HM_END_EACH
    }
    print_transition_table(&tf, nfa);

compile_failed:
    HM_FOR_EACH(&tf, struct match, struct vec, match, entry)
        VEC_FOR_EACH(entry, struct vec, states)
            vec_deinit(states);
        VEC_END_EACH
        vec_deinit(entry);
    HM_END_EACH
hm_init_failed:
    hm_deinit(&tf);

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
