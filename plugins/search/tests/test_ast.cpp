#include "gmock/gmock.h"

#include "search/ast.h"
#include "search/parser.h"

#define NAME ast

using namespace testing;

TEST(NAME, parse)
{
    struct parser parser;
    union ast_node* ast;

    parser_init(&parser);
    ast = parser_parse(&parser, "0xa->0xb*|0xc+");
    ASSERT_THAT(ast, NotNull());
    ast_destroy_recurse(ast);
    parser_deinit(&parser);
}
