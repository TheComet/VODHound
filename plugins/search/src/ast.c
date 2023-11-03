#include "search/ast.h"
#include "search/parser.y.h"

#include "vh/hm.h"
#include "vh/mem.h"

#include <stddef.h>
#include <stdio.h>

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

union ast_node* ast_repetition(union ast_node* child, int min_reps, int max_reps, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_REPETITION, loc);
    node->repetition.child = child;
    node->repetition.min_reps = min_reps;
    node->repetition.max_reps = max_reps;
    return node;
}

union ast_node* ast_union(union ast_node* child, union ast_node* next, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_UNION, loc);
    node->union_.child = child;
    node->union_.next = next;
    return node;
}

union ast_node* ast_inversion(union ast_node* child, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_INVERSION, loc);
    node->inversion.child = child;
    return node;
}

union ast_node* ast_context_qualifier(union ast_node* child, uint8_t flags, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_CONTEXT_QUALIFIER, loc);
    node->context_qualifier.child = child;
    node->context_qualifier.flags = flags;
    return node;
}

union ast_node* ast_label_steal(char* label, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_LABEL, loc);
    node->labels.label = label;
    node->labels.opponent_label = NULL;
    return node;
}

union ast_node* ast_labels_steal(char* label, char* opponent_label, struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_LABEL, loc);
    node->labels.label = label;
    node->labels.opponent_label = opponent_label;
    return node;
}

union ast_node* ast_wildcard(struct YYLTYPE* loc)
{
    union ast_node* node = MALLOC_AND_INIT(AST_WILDCARD, loc);
    return node;
}

void ast_destroy_single(union ast_node* node)
{
    if (node->info.type == AST_LABEL)
    {
        mem_free(node->labels.label);
        if (node->labels.opponent_label)
            mem_free(node->labels.opponent_label);
    }

    mem_free(node);
}

void ast_destroy_recurse(union ast_node* node)
{
    if (node->base.left)
        ast_destroy_recurse(node->base.left);
    if (node->base.right)
        ast_destroy_recurse(node->base.right);

    ast_destroy_single(node);
}

// ----------------------------------------------------------------------------
static int calculate_node_ids(const union ast_node* node, struct hm* node_ids, int* counter)
{
    *counter += 1;
    if (hm_insert_new(node_ids, &node, counter) <= 0)
        return -1;

    if (node->base.left)
        if (calculate_node_ids(node->base.left, node_ids, counter) < 0)
            return -1;
    if (node->base.right)
        if (calculate_node_ids(node->base.right, node_ids, counter) < 0)
            return -1;

    return 0;
}

static void write_nodes(const union ast_node* node, FILE* fp, const struct hm* node_ids)
{
    const int node_id = *(int*)hm_find(node_ids, &node);
    switch (node->info.type)
    {
        case AST_STATEMENT:
            fprintf(fp, "  n%d [label=\"->\"];\n", node_id);
            break;
        case AST_REPETITION:
            fprintf(fp, "  n%d [label=\"rep %d,%d\"];\n",
                    node_id, node->repetition.min_reps, node->repetition.max_reps);
            break;
        case AST_UNION:
            fprintf(fp, "  n%d [label=\"|\"];\n", node_id);
            break;
        case AST_INVERSION:
            fprintf(fp, "  n%d [label=\"!\"];\n", node_id);
            break;
        case AST_WILDCARD:
            fprintf(fp, "  n%d [shape=\"rectangle\",label=\".\"];\n", node_id);
            break;
        case AST_LABEL:
            if (node->labels.opponent_label)
                fprintf(fp, "  n%d [shape=\"rectangle\",label=\"%s [%s]\"];\n",
                        node_id, node->labels.label, node->labels.opponent_label);
            else
                fprintf(fp, "  n%d [shape=\"rectangle\",label=\"%s\"];\n",
                        node_id, node->labels.label);
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
            fprintf(fp, "  n%d [shape=\"record\",label=\"", node_id);
            if (node->context_qualifier.flags & AST_CTX_OS) APPEND_WITH_PIPE("OS")
            if (node->context_qualifier.flags & AST_CTX_HIT) APPEND_WITH_PIPE("HIT")
            if (node->context_qualifier.flags & AST_CTX_WHIFF) APPEND_WITH_PIPE("WHIFF")
            fprintf(fp, "\"];\n");

            #undef APPEND_WITH_PIPE
        } break;
    }

    if (node->base.left)
        write_nodes(node->base.left, fp, node_ids);
    if (node->base.right)
        write_nodes(node->base.right, fp, node_ids);
}

static void write_edges(const union ast_node* node, FILE* fp, const struct hm* node_ids)
{
    if (node->base.left)
    {
        fprintf(fp, "  n%d -> n%d;\n",
            *(int*)hm_find(node_ids, &node), *(int*)hm_find(node_ids, &node->base.left));
        write_edges(node->base.left, fp, node_ids);
    }

    if (node->base.right)
    {
        fprintf(fp, "  n%d -> n%d;\n",
            *(int*)hm_find(node_ids, &node), *(int*)hm_find(node_ids, &node->base.right));
        write_edges(node->base.right, fp, node_ids);
    }
}

int ast_export_dot(union ast_node* root, const char* file_name)
{
    FILE* fp;
    struct hm node_ids;
    int counter;

    fp = fopen(file_name, "w");
    if (fp == NULL)
        goto fopen_failed;

    if (hm_init(&node_ids, sizeof(union ast_node*), sizeof(int)) != 0)
        goto hm_init_failed;

    counter = 0;
    if (calculate_node_ids(root, &node_ids, &counter) < 0)
        goto calc_node_ids_failed;
    fprintf(fp, "digraph ast {\n");
        write_nodes(root, fp, &node_ids);
        write_edges(root, fp, &node_ids);
    fprintf(fp, "}\n");
    fclose(fp);

    hm_deinit(&node_ids);
    return 0;

    calc_node_ids_failed : hm_deinit(&node_ids);
    hm_init_failed       : fclose(fp);
    fopen_failed         : return -1;
}
