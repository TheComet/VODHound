#include "search/ast.h"
#include "search/parser.h"

#include "vh/init.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [options] <query text>\n", argv[0]);
        return 1;
    }

    vh_threadlocal_init();
    vh_init();

    struct parser parser;
    parser_init(&parser);
    union ast_node* ast = parser_parse(&parser, argv[1]);
    parser_deinit(&parser);

    if (ast)
    {
        ast_export_dot(ast, "ast.dot");
        ast_destroy_recurse(ast);
    }

    vh_deinit();
    vh_threadlocal_deinit();

    return 0;
}
