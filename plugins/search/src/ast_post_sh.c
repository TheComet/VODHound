#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/ast_post.h"

int
ast_post_sh(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_SH))
        {
            int jump_f_mini, jump_b_mini, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_f_mini") = 0xb38c9ab48 */
            jump_f_mini = ast_motion(ast, 0xb38c9ab48ul, loc);
            if (jump_f_mini < 0) return -1;

            /* hash40("jump_b_mini") = 0xba358e95e */
            jump_b_mini = ast_motion(ast, 0xba358e95eul, loc);
            if (jump_b_mini < 0) return -1;

            /* jump_f_mini | jump_b_mini */
            union_ = ast_union(ast, jump_f_mini, jump_b_mini, loc);
            if (union_ < 0) return -1;

            /* (jump_f_mini | jump_b_mini)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}
