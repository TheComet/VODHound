#include "search/ast.h"
#include "search/ast_ops.h"

void ast_set_root(struct ast* ast, int node)
{
    ast_swap_nodes(ast, 0, node);
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
    //ast_deinit_node(ast, target);
    ast->nodes[target] = ast->nodes[ast->node_count];
}

int ast_find_parent(struct ast* ast, int node)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].base.left == node || ast->nodes[n].base.right == node)
            return n;
    return -1;
}

void ast_replace_into(struct ast* ast, int node, int target)
{
    int parent = ast_find_parent(ast, target);
    if (parent < 0)
    {
        ast_set_root(ast, node);
        return;
    }

    if (ast->nodes[parent].base.left == target)
        ast->nodes[parent].base.left = node;
    if (ast->nodes[parent].base.right == target)
        ast->nodes[parent].base.right = node;
}

int ast_trees_equal(struct ast* a1, int n1, struct ast* a2, int n2)
{
    if (a1->nodes[n1].info.type != a2->nodes[n2].info.type)
        return 0;

    switch (a1->nodes[n1].info.type)
    {
        case AST_STATEMENT: break;
        case AST_REPETITION:
            if (a1->nodes[n1].repetition.min_reps != a2->nodes[n2].repetition.min_reps)
                return 0;
            if (a1->nodes[n1].repetition.max_reps != a2->nodes[n2].repetition.max_reps)
                return 0;
            break;
        case AST_UNION: break;
        case AST_INVERSION: break;
        case AST_CONTEXT:
            if (a1->nodes[n1].context.flags != a2->nodes[n2].context.flags)
                return 0;
            break;
        case AST_LABEL:
            if (!str_equal(
                    strlist_to_view(&a1->labels, a1->nodes[n1].label.label),
                    strlist_to_view(&a2->labels, a2->nodes[n2].label.label)))
                return 0;
            break;
        case AST_MOTION:
            if (a1->nodes[n1].motion.motion != a2->nodes[n2].motion.motion)
                return 0;
            break;
        case AST_TIMING:
            if (a1->nodes[n1].timing.start != a2->nodes[n2].timing.start)
                return 0;
            if (a1->nodes[n1].timing.end != a2->nodes[n2].timing.end)
                return 0;
            break;
    }

    if (a1->nodes[n1].base.left >= 0 && a2->nodes[n2].base.left < 0)
        return 0;
    if (a1->nodes[n1].base.left < 0 && a2->nodes[n2].base.left >= 0)
        return 0;
    if (a1->nodes[n1].base.right >= 0 && a2->nodes[n2].base.right < 0)
        return 0;
    if (a1->nodes[n1].base.right < 0 && a2->nodes[n2].base.right >= 0)
        return 0;

    if (a1->nodes[n1].base.left >= 0)
        if (ast_trees_equal(a1, a1->nodes[n1].base.left, a2, a2->nodes[n2].base.left) == 0)
            return 0;
    if (a1->nodes[n1].base.right >= 0)
        if (ast_trees_equal(a1, a1->nodes[n1].base.right, a2, a2->nodes[n2].base.right) == 0)
            return 0;

    return 1;
}
