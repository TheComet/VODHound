#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

struct ast;

void ast_set_root(struct ast* ast, int node);
void ast_swap_node_idxs(struct ast* ast, int n1, int n2);
void ast_swap_node_values(struct ast* ast, int n1, int n2);
void ast_collapse_into(struct ast* ast, int node, int target);
void ast_replace_into(struct ast* ast, int node, int target);

int ast_find_parent(struct ast* ast, int node);
/* Returns true if "node" is found in the subtree starting (and including) node "root" */
int ast_is_in_subtree_of(struct ast* ast, int node, int root);
int ast_trees_equal(struct ast* a1, int n1, struct ast* a2, int n2);
int ast_node_preceeds(struct ast* ast, int n1, int n2);

#if defined(__cplusplus)
}
#endif
