#include "search/ast.h"
#include "search/nfa.h"
#include "search/parser.h"

#include "vh/init.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    struct parser parser;
    union ast_node* ast = NULL;
    struct nfa_graph* nfa = NULL;
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [options] <query text>\n", argv[0]);
        return 1;
    }

    vh_threadlocal_init();
    vh_init();

    parser_init(&parser);
    ast = parser_parse(&parser, argv[1]);
    parser_deinit(&parser);

    if (ast)
    {
        ast_export_dot(ast, "ast.dot");
        nfa = nfa_compile(ast);
        ast_destroy_recurse(ast);
    }

    if (nfa)
    {
        nfa_export_dot(nfa, "nfa.dot");
        nfa_destroy(nfa);
    }

    vh_deinit();
    vh_threadlocal_deinit();

    return 0;
}
