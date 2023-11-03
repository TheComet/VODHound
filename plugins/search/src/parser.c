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

int
parser_parse(struct parser* parser, const char* text, struct ast* ast)
{
    int pushed_char;
    int parse_result;
    YY_BUFFER_STATE buffer;
    YYSTYPE pushed_value;
    YYLTYPE location = {1, 1};

    if (ast_init(ast) < 0)
        goto init_ast_failed;

    buffer = yy_scan_string(text, parser->scanner);
    if (buffer == NULL)
        goto init_buffer_failed;

    do
    {
        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
    } while (parse_result == YYPUSH_MORE);

    if (parse_result == 0)
    {
        yy_delete_buffer(buffer, parser->scanner);
        return 0;
    }

parse_failed       : yy_delete_buffer(buffer, parser->scanner);
init_buffer_failed : ast_deinit(ast);
init_ast_failed    : return -1;
}
