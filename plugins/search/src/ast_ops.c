#include "search/ast.h"
#include "search/ast_ops.h"

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

void ast_set_root(struct ast* ast, int node)
{
    ast_swap_nodes(ast, 0, node);
}
