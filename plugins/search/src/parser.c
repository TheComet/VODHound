#include "search/parser.h"
#include "search/parser.y.h"
#include "search/scanner.lex.h"
#include "search/ast.h"

int
parser_init(struct parser* parser)
{
    if (yylex_init(&parser->scanner) != 0)
        goto init_scanner_failed;

    parser->parser = yypstate_new();
    if (parser->parser == NULL)
        goto init_parser_failed;

    yy_scan_bytes("test", 5, parser->scanner);

    return 0;

    init_parser_failed        : yylex_destroy(parser->scanner);
    init_scanner_failed       : return -1;
}

void
parser_deinit(struct parser* parser)
{
    yypstate_delete(parser->parser);
    yylex_destroy(parser->scanner);
}

union ast_node*
parser_parse(struct parser* parser, const char* text)
{
    int pushed_char;
    int parse_result;
    YY_BUFFER_STATE buffer;
    YYSTYPE pushed_value;
    YYLTYPE location = {1, 1};
    union ast_node* ast = NULL;

    buffer = yy_scan_string(text, parser->scanner);
    if (buffer == NULL)
        return NULL;

    do
    {
        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, &ast);
    } while (parse_result == YYPUSH_MORE);

    yy_delete_buffer(buffer, parser->scanner);

    if (parse_result == 0)
        return ast;
    if (ast)
        ast_destroy_recurse(ast);
    return NULL;
}
