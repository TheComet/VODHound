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
        if (ast->nodes[n].info.type == AST_TIMING)
            if (ast->nodes[n].timing.rel_to_ref == n1)
                ast->nodes[n].timing.rel_to_ref = -2;
    }
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left == n2) ast->nodes[n].base.left = n1;
        if (ast->nodes[n].base.right == n2) ast->nodes[n].base.right = n1;
        if (ast->nodes[n].info.type == AST_TIMING)
            if (ast->nodes[n].timing.rel_to_ref == n2)
                ast->nodes[n].timing.rel_to_ref = n1;
    }
    for (n = 0; n != ast->node_count; ++n)
    {
        if (ast->nodes[n].base.left == -2) ast->nodes[n].base.left = n2;
        if (ast->nodes[n].base.right == -2) ast->nodes[n].base.right = n2;
        if (ast->nodes[n].info.type == AST_TIMING)
            if (ast->nodes[n].timing.rel_to_ref == -2)
                ast->nodes[n].timing.rel_to_ref = n2;
    }

    tmp = ast->nodes[n1];
    ast->nodes[n1] = ast->nodes[n2];
    ast->nodes[n2] = tmp;
}

void ast_collapse_into(struct ast* ast, int node, int target)
{
    ast->node_count--;
    ast_swap_nodes(ast, node, ast->node_count);
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

int ast_is_in_subtree_of(struct ast* ast, int node, int root)
{
    if (node == root)
        return 1;

    if (ast->nodes[root].base.left >= 0)
        if (ast_is_in_subtree_of(ast, node, ast->nodes[root].base.left))
            return 1;
    if (ast->nodes[root].base.right >= 0)
        if (ast_is_in_subtree_of(ast, node, ast->nodes[root].base.right))
            return 1;

    return 0;
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
            if (!ast_trees_equal(
                    a1, a1->nodes[n1].timing.rel_to_ref,
                    a2, a2->nodes[n2].timing.rel_to_ref))
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

int ast_node_preceeds(struct ast* ast, int n1, int n2)
{
    while (1)
    {
        do {
            n2 = ast_find_parent(ast, n2);
            if (n2 < 0)
                return 0;
        } while (ast->nodes[n2].info.type != AST_STATEMENT);

        if (ast->nodes[n2].base.left >= 0)
            if (ast_is_in_subtree_of(ast, n1, ast->nodes[n2].base.left))
                return 1;
        if (ast->nodes[n2].base.right >= 0)
            if (ast_is_in_subtree_of(ast, n1, ast->nodes[n2].base.right))
                return 0;
    }
}
