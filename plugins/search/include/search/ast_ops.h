#pragma once

struct ast;

void ast_set_root(struct ast* ast, int node);
void ast_swap_nodes(struct ast* ast, int n1, int n2);
void ast_collapse_into(struct ast* ast, int node, int target);
