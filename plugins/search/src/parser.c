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
    if (ast_post_jump_qualifiers(ast) < 0) return -1;
    if (ast_post_timing(ast) < 0) return -1;
    if (ast_post_damage(ast) < 0) return - 1;
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
    enum ast_ctx_flags flags;

    yyset_extra(&ast->labels, parser->scanner);

    buffer = yy_scan_string(text, parser->scanner);
    if (buffer == NULL)
        goto init_buffer_failed;

    do
    {
        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        if (pushed_char == TOK_PRE_CTX)
            goto pre_context_parser;
        else if (pushed_char == TOK_POST_CTX)
            goto post_context_parser;
        parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
main_parser:;
    } while (parse_result == YYPUSH_MORE);
    goto main_parser_done;

pre_context_parser:
    flags = pushed_value.ctx_flag_value;
    do
    {
        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        if (pushed_char != '|')
        {
            YYSTYPE flag_value;
            flag_value.ctx_flag_value = flags;
            if ((parse_result = yypush_parse(parser->parser, TOK_PRE_CTX, &flag_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
            goto main_parser;
        }

        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        if (pushed_char != TOK_PRE_CTX)
        {
            YYSTYPE flag_value;
            flag_value.ctx_flag_value = flags;
            if ((parse_result = yypush_parse(parser->parser, TOK_PRE_CTX, &flag_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            if ((parse_result = yypush_parse(parser->parser, '|', &pushed_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
            goto main_parser;
        }

        flags |= pushed_value.ctx_flag_value;
    } while (1);

post_context_parser:
    flags = pushed_value.ctx_flag_value;
    do
    {
        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        if (pushed_char != '|')
        {
            YYSTYPE flag_value;
            flag_value.ctx_flag_value = flags;
            if ((parse_result = yypush_parse(parser->parser, TOK_POST_CTX, &flag_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
            goto main_parser;
        }

        pushed_char = yylex(&pushed_value, &location, parser->scanner);
        if (pushed_char == TOK_PRE_CTX)
        {
            YYSTYPE flag_value;
            flag_value.ctx_flag_value = flags;
            if ((parse_result = yypush_parse(parser->parser, TOK_POST_CTX, &flag_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            if ((parse_result = yypush_parse(parser->parser, '|', &pushed_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            goto pre_context_parser;
        }
        else if (pushed_char != TOK_POST_CTX)
        {
            YYSTYPE flag_value;
            flag_value.ctx_flag_value = flags;
            if ((parse_result = yypush_parse(parser->parser, TOK_POST_CTX, &flag_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            if ((parse_result = yypush_parse(parser->parser, '|', &pushed_value, &location, ast)) != YYPUSH_MORE)
                goto main_parser_done;
            parse_result = yypush_parse(parser->parser, pushed_char, &pushed_value, &location, ast);
            goto main_parser;
        }

        flags |= pushed_value.ctx_flag_value;
    } while (1);

main_parser_done:
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
