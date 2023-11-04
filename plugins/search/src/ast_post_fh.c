#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/ast_post.h"

int
ast_post_fh(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_FH))
        {
            int jump_f, jump_b, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_f") = 0x62dd02058 */
            jump_f = ast_motion(ast, 0x62dd02058ul, loc);
            if (jump_f < 0) return -1;

            /* hash40("jump_b") = 0x62abde441 */
            jump_b = ast_motion(ast, 0x62abde441ul, loc);
            if (jump_b < 0) return -1;

            /* jump_f | jump_b */
            union_ = ast_union(ast, jump_f, jump_b, loc);
            if (union_ < 0) return -1;

            /* (jump_f | jump_b)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}
