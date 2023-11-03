#include "gmock/gmock.h"

#include "search/ast.h"
#include "search/parser.h"

#define NAME ast

using namespace testing;

TEST(NAME, parse)
{
    struct parser parser;
    struct ast ast;

    parser_init(&parser);
    ASSERT_THAT(parser_parse(&parser, "0xa->0xb*|0xc+", &ast), Eq(0));
    ast_deinit(&ast);
    parser_deinit(&parser);
}
