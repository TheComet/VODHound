#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/ast_post.h"

int
ast_post_dj(struct ast* ast)
{
    int n;
    for (n = 0; n != ast->node_count; ++n)
        if (ast->nodes[n].info.type == AST_CONTEXT_QUALIFIER &&
            (ast->nodes[n].context_qualifier.flags & AST_CTX_DJ))
        {
            int jump_aerial_f, jump_aerial_b, union_, dj, dj_n;
            const struct YYLTYPE* loc = (const struct YYLTYPE*)&ast->nodes[n].info.loc;

            /* hash40("jump_aerial_f") = 0xd0b71815b */
            jump_aerial_f = ast_motion(ast, 0xd0b71815bul, loc);
            if (jump_aerial_f < 0) return -1;

            /* hash40("jump_aerial_b") = 0xd0c1c4542 */
            jump_aerial_b = ast_motion(ast, 0xd0c1c4542ul, loc);
            if (jump_aerial_b < 0) return -1;

            /* jump_aerial_f | jump_aerial_b */
            union_ = ast_union(ast, jump_aerial_f, jump_aerial_b, loc);
            if (union_ < 0) return -1;

            /* (jump_aerial_f | jump_aerial_b)+ */
            dj = ast_repetition(ast, union_, 1, -1, loc);
            if (dj < 0) return -1;

            dj_n = ast_statement(ast, dj, ast->nodes[n].context_qualifier.child, loc);
            if (dj_n < 0) return -1;

            ast_collapse_into(ast, dj_n, n);
        }

    return 0;
}
