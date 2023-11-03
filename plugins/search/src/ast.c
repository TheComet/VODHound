#include "search/ast.h"
#include "search/parser.y.h"

#include "vh/hm.h"
#include "vh/mem.h"

#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>

#define NEW_NODE(ast, node_type, loc)                               \
    ast->node_count++;                                              \
    if (ast->node_count >= ast->node_capacity) {                    \
        union ast_node* new_nodes = mem_realloc(ast->nodes, ast->node_capacity * 2); \
        if (new_nodes == NULL)                                      \
            return -1;                                              \
        ast->nodes = new_nodes;                                     \
    }                                                               \
    ast->nodes[ast->node_count-1].base.info.type = node_type;       \
    ast->nodes[ast->node_count-1].base.info.loc.begin = loc->begin; \
    ast->nodes[ast->node_count-1].base.info.loc.end = loc->end;     \
    ast->nodes[ast->node_count-1].base.left = -1;                   \
    ast->nodes[ast->node_count-1].base.right = -1;

int ast_statement(struct ast* ast, int child, int next, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_STATEMENT, loc);
    ast->nodes[n].statement.child = child;
    ast->nodes[n].statement.next = next;
    return n;
}

int ast_repetition(struct ast* ast, int child, int min_reps, int max_reps, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_REPETITION, loc);
    ast->nodes[n].repetition.child = child;
    ast->nodes[n].repetition.min_reps = min_reps;
    ast->nodes[n].repetition.max_reps = max_reps;
    return n;
}

int ast_union(struct ast* ast, int child, int next, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_UNION, loc);
    ast->nodes[n].union_.child = child;
    ast->nodes[n].union_.next = next;
    return n;
}

int ast_inversion(struct ast* ast, int child, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_INVERSION, loc);
    ast->nodes[n].inversion.child = child;
    return n;
}

int ast_wildcard(struct ast* ast, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_WILDCARD, loc);
    return n;
}

int ast_context_qualifier(struct ast* ast, int child, uint8_t flags, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_CONTEXT_QUALIFIER, loc);
    ast->nodes[n].context_qualifier.child = child;
    ast->nodes[n].context_qualifier.flags = flags;
    return n;
}

int ast_label_steal(struct ast* ast, char* label, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_LABEL, loc);
    ast->nodes[n].labels.label = label;
    ast->nodes[n].labels.opponent_label = NULL;
    return n;
}

int ast_labels_steal(struct ast* ast, char* label, char* opponent_label, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_LABEL, loc);
    ast->nodes[n].labels.label = label;
    ast->nodes[n].labels.opponent_label = opponent_label;
    return n;
}

int ast_motion(struct ast* ast, uint64_t motion, struct YYLTYPE* loc)
{
    int n = NEW_NODE(ast, AST_MOTION, loc);
    ast->nodes[n].motion.motion = motion;
    return n;
}

void ast_swap_nodes(struct ast* ast, int n1, int n2)
{
    int n;
    union ast_node tmp;

    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left == n1) ast->nodes[n].base.left = -2;
        if (ast->nodes[n].base.right == n1) ast->nodes[n].base.right = -2;
    }
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left == n2) ast->nodes[n].base.left = n1;
        if (ast->nodes[n].base.right == n2) ast->nodes[n].base.right = n1;
    }
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left == -2) ast->nodes[n].base.left = n2;
        if (ast->nodes[n].base.right == -2) ast->nodes[n].base.right = n2;
    }

    tmp = ast->nodes[n1];
    ast->nodes[n1] = ast->nodes[n2];
    ast->nodes[n2] = tmp;
}

void ast_collapse_into(struct ast* ast, int node, int target)
{
    ast->node_count--;
    ast_swap_nodes(ast, node, ast->node_count);
    ast_deinit_node(ast, target);
    ast->nodes[target] = ast->nodes[ast->node_count];
}

void ast_set_root(struct ast* ast, int node)
{
    ast_swap_nodes(ast, 0, node);
}

int ast_init(struct ast* ast)
{
    ast->node_count = 0;
    ast->node_capacity = 32;
    ast->nodes = mem_alloc(sizeof(union ast_node) * ast->node_capacity);
    if (ast->nodes == NULL)
        return -1;
    return 0;
}

void ast_deinit_node(struct ast* ast, int n)
{
    if (ast->nodes[n].info.type == AST_LABEL)
    {
        mem_free(ast->nodes[n].labels.label);
        if (ast->nodes[n].labels.opponent_label)
            mem_free(ast->nodes[n].labels.opponent_label);
    }
}

void ast_deinit(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        ast_deinit_node(ast, n);

    mem_free(ast->nodes);
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
        case AST_LABEL:
            if (ast->nodes[n].labels.opponent_label)
                fprintf(fp, "  n%d [shape=\"rectangle\",label=\"%s [%s]\"];\n",
                    n, ast->nodes[n].labels.label, ast->nodes[n].labels.opponent_label);
            else
                fprintf(fp, "  n%d [shape=\"rectangle\",label=\"%s\"];\n",
                    n, ast->nodes[n].labels.label);
            break;
        case AST_MOTION:
            fprintf(fp, "  n%d [shape=\"rectangle\",label=\"0x%" PRIx64 "\"];\n",
                n, ast->nodes[n].motion.motion);
            break;
        case AST_CONTEXT_QUALIFIER: {
            #define APPEND_WITH_PIPE(str) {  \
                if (need_pipe)               \
                    fprintf(fp, " | " str);  \
                else                         \
                    fprintf(fp, str);        \
                need_pipe = 1;               \
            }

            int need_pipe = 0;
            fprintf(fp, "  n%d [shape=\"record\",label=\"", n);
            if (ast->nodes[n].context_qualifier.flags & AST_CTX_OS) APPEND_WITH_PIPE("OS")
            if (ast->nodes[n].context_qualifier.flags & AST_CTX_HIT) APPEND_WITH_PIPE("HIT")
            if (ast->nodes[n].context_qualifier.flags & AST_CTX_WHIFF) APPEND_WITH_PIPE("WHIFF")
            fprintf(fp, "\"];\n");

            #undef APPEND_WITH_PIPE
        } break;
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
    }
}

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
