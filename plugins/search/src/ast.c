#include "search/ast.h"
#include "search/parser.y.h"

#include "vh/mem.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static hash32
hash_hash40(const void* data, int len)
{
    const uint64_t* motion = data;
    return *motion & 0xFFFFFFFF;
}

int ast_init(struct ast* ast)
{
    ast->node_count = 0;
    ast->node_capacity = 32;

    ast->nodes = mem_alloc(sizeof(union ast_node) * ast->node_capacity);
    if (ast->nodes == NULL)
        return -1;

    if (hm_init_with_options(&ast->merged_labels,
        sizeof(uint64_t), sizeof(struct strlist_str), VH_HM_MIN_CAPACITY,
        hash_hash40, (hm_compare_func)memcmp) < 0)
    {
        mem_free(ast->nodes);
        return -1;
    }

    strlist_init(&ast->labels);

    return 0;
}

void ast_deinit(struct ast* ast)
{
    strlist_deinit(&ast->labels);

    /* NOTE: hashmap references strings stored in AST nodes! If it ever changes
     * to where the hashmap owns the strings, the following code needs to be
     * uncommented.
    HM_FOR_EACH(&ast->merged_labels, uint64_t, char*, motion, label)
        mem_free(*label);
    HM_END_EACH*/
    hm_deinit(&ast->merged_labels);

    mem_free(ast->nodes);
}

void ast_clear(struct ast* ast)
{
    strlist_clear(&ast->labels);
    hm_clear(&ast->merged_labels);

    ast->node_count = 0;
}

#define NEW_NODE_NO_INIT(ast)                                       \
    ast->node_count++;                                              \
    if (ast->node_count >= ast->node_capacity) {                    \
        union ast_node* new_nodes = mem_realloc(ast->nodes, ast->node_capacity * 2); \
        if (new_nodes == NULL)                                      \
            return -1;                                              \
        ast->nodes = new_nodes;                                     \
    }                                                               \

#define NEW_NODE(ast, node_type, loc)                               \
    NEW_NODE_NO_INIT(ast)                                           \
    ast->nodes[ast->node_count-1].base.info.type = node_type;       \
    ast->nodes[ast->node_count-1].base.info.loc.begin = loc->begin; \
    ast->nodes[ast->node_count-1].base.info.loc.end = loc->end;     \
    ast->nodes[ast->node_count-1].base.left = -1;                   \
    ast->nodes[ast->node_count-1].base.right = -1;

int ast_statement(struct ast* ast, int child, int next, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_STATEMENT, loc);
    ast->nodes[n].statement.child = child;
    ast->nodes[n].statement.next = next;
    return n;
}

int ast_repetition(struct ast* ast, int child, int min_reps, int max_reps, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_REPETITION, loc);
    ast->nodes[n].repetition.child = child;
    ast->nodes[n].repetition.min_reps = min_reps;
    ast->nodes[n].repetition.max_reps = max_reps;
    return n;
}

int ast_union(struct ast* ast, int child, int next, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_UNION, loc);
    ast->nodes[n].union_.child = child;
    ast->nodes[n].union_.next = next;
    return n;
}

int ast_inversion(struct ast* ast, int child, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_INVERSION, loc);
    ast->nodes[n].inversion.child = child;
    return n;
}

int ast_wildcard(struct ast* ast, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_WILDCARD, loc);
    return n;
}

int ast_label(struct ast* ast, struct strlist_str label, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_LABEL, loc);
    ast->nodes[n].label.label = label;
    return n;
}

int ast_motion(struct ast* ast, uint64_t motion, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_MOTION, loc);
    ast->nodes[n].motion.motion = motion;
    return n;
}

int ast_context(struct ast* ast, int child, enum ast_ctx_flags flags, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_CONTEXT, loc);
    ast->nodes[n].context.child = child;
    ast->nodes[n].context.flags = flags;
    return n;
}


int ast_timing(struct ast* ast, int rel_to, int child, int start, int end, const struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_TIMING, loc);
    ast->nodes[n].timing.rel_to = rel_to;
    ast->nodes[n].timing.child = child;
    ast->nodes[n].timing.start = start;
    ast->nodes[n].timing.end = end;
    ast->nodes[n].timing.rel_to_ref = -1;
    return n;
}

int ast_duplicate(struct ast* ast, int node)
{
    int dup = NEW_NODE_NO_INIT(ast);
    memcpy(&ast->nodes[dup], &ast->nodes[node], sizeof(union ast_node));

    if (ast->nodes[node].base.left >= 0)
        if ((ast->nodes[dup].base.left = ast_duplicate(ast, ast->nodes[node].base.left)) < 0)
            return -1;

    if (ast->nodes[node].base.right >= 0)
        if ((ast->nodes[dup].base.right = ast_duplicate(ast, ast->nodes[node].base.right)) < 0)
            return -1;

    return dup;
}

static void write_nodes(const struct ast* ast, int n, FILE* fp)
{
    switch (ast->nodes[n].info.type)
    {
        case AST_STATEMENT:
            fprintf(fp, "  n%d [label=\"->\"];\n", n);
            break;
        case AST_REPETITION:
            fprintf(fp, "  n%d [label=\"rep %d,%d\"];\n",
                    n, ast->nodes[n].repetition.min_reps, ast->nodes[n].repetition.max_reps);
            break;
        case AST_UNION:
            fprintf(fp, "  n%d [label=\"|\"];\n", n);
            break;
        case AST_INVERSION:
            fprintf(fp, "  n%d [label=\"!\"];\n", n);
            break;
        case AST_WILDCARD:
            fprintf(fp, "  n%d [shape=\"rectangle\",label=\".\"];\n", n);
            break;
        case AST_LABEL: {
            struct str_view label = strlist_to_view(&ast->labels, ast->nodes[n].label.label);
            fprintf(fp, "  n%d [shape=\"rectangle\",label=\"%.*s\"];\n",
                n, label.len, label.data);
        } break;
        case AST_MOTION:
            fprintf(fp, "  n%d [shape=\"rectangle\",label=\"0x%" PRIx64 "\"];\n",
                n, ast->nodes[n].motion.motion);
            break;
        case AST_CONTEXT: {
            #define APPEND_WITH_PIPE(str) {  \
                if (need_pipe)               \
                    fprintf(fp, " | " str);  \
                else                         \
                    fprintf(fp, str);        \
                need_pipe = 1;               \
            }
            #define APPEND(name)             \
                if (ast->nodes[n].context.flags & AST_CTX_##name) \
                    APPEND_WITH_PIPE(#name)

            int need_pipe = 0;
            fprintf(fp, "  n%d [shape=\"record\",label=\"", n);
            APPEND(OS)
            APPEND(OOS)
            APPEND(HIT)
            APPEND(WHIFF)
            APPEND(CLANK)
            APPEND(TRADE)
            APPEND(KILL)
            APPEND(DIE)
            APPEND(BURY)
            APPEND(BURIED)
            APPEND(RISING)
            APPEND(FALLING)
            APPEND(SH)
            APPEND(FH)
            APPEND(DJ)
            APPEND(FS)
            APPEND(IDJ)
            fprintf(fp, "\"];\n");

            #undef APPEND
            #undef APPEND_WITH_PIPE
        } break;
        case AST_TIMING:
            fprintf(fp, "  n%d [shape=\"record\",label=\"f%d", n, ast->nodes[n].timing.start);
            if (ast->nodes[n].timing.end >= 0)
                fprintf(fp, "-%d", ast->nodes[n].timing.end);
            fprintf(fp, "\"];\n");
            break;
    }

    if (ast->nodes[n].base.left >= 0)
        write_nodes(ast, ast->nodes[n].base.left, fp);
    if (ast->nodes[n].base.right >= 0)
        write_nodes(ast, ast->nodes[n].base.right, fp);
}

static void write_edges(const struct ast* ast, FILE* fp)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left >= 0)
            fprintf(fp, "  n%d -> n%d;\n", n, ast->nodes[n].base.left);

        if (ast->nodes[n].base.right >= 0)
            fprintf(fp, "  n%d -> n%d;\n", n, ast->nodes[n].base.right);

        if (ast->nodes[n].info.type == AST_TIMING)
            if (ast->nodes[n].timing.rel_to_ref >= 0)
                fprintf(fp, "  n%d -> n%d [color=\"blue\"];\n", n, ast->nodes[n].timing.rel_to_ref);
    }
}

#if defined(EXPORT_DOT)
int ast_export_dot(const struct ast* ast, const char* file_name)
{
    FILE* fp = fopen(file_name, "w");
    if (fp == NULL)
        return -1;

    fprintf(fp, "digraph ast {\n");
        write_nodes(ast, 0, fp);
        write_edges(ast, fp);
    fprintf(fp, "}\n");
    fclose(fp);

    return 0;
}
#endif
