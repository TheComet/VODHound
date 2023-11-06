#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

struct ast;

void ast_set_root(struct ast* ast, int node);
void ast_swap_nodes(struct ast* ast, int n1, int n2);
void ast_collapse_into(struct ast* ast, int node, int target);
int ast_find_parent(struct ast* ast, int node);
void ast_replace_into(struct ast* ast, int node, int target);

int ast_trees_equal(struct ast* a1, int n1, struct ast* a2, int n2);

#if defined(__cplusplus)
}
#endif
