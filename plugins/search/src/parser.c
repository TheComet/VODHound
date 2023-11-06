#include "search/parser.h"
#include "search/parser.y.h"
#include "search/scanner.lex.h"
#include "search/ast.h"
#include "search/ast_post.h"

int
parser_init(struct parser* parser)
{
    if (yylex_init(&parser->scanner) != 0)
        goto init_scanner_failed;

    parser->parser = yypstate_new();
    if (parser->parser == NULL)
        goto init_parser_failed;

    return 0;

    init_parser_failed  : yylex_destroy(parser->scanner);
    init_scanner_failed : return -1;
}

void
parser_deinit(struct parser* parser)
{
    yypstate_delete(parser->parser);
    yylex_destroy(parser->scanner);
}

static int
ast_post(struct ast* ast)
{
    if (ast_post_timing(ast) < 0) return -1;
    if (ast_post_jump_qualifiers(ast) < 0) return -1;
    if (ast_post_validate_params(ast) < 0) return -1;
    ast_export_dot(ast, "ast.dot");
    return 0;
}

int
parser_parse(struct parser* parser, const char* text, struct ast* ast)
{
    int pushed_char;
    int parse_result;
    YY_BUFFER_STATE buffer;
    YYSTYPE pushed_value;
    YYLTYPE location = {1, 1};

    yyset_extra(&ast->labels, parser->scanner);

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
        yyset_extra(NULL, parser->scanner);
        ast_export_dot(ast, "ast.dot");
        return ast_post(ast);
    }

    yy_delete_buffer(buffer, parser->scanner);
init_buffer_failed:
    yyset_extra(NULL, parser->scanner);
    return -1;
}
