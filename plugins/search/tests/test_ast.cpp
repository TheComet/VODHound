#include "gmock/gmock.h"

#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/parser.h"
#include "search/parser.y.h"

#define NAME parse_ast

using namespace testing;

struct NAME : public Test
{
    void SetUp() override
    {
        parser_init(&parser);
        ast_init(&ast1);
        ast_init(&ast2);
    }
    void TearDown() override
    {
        ast_deinit(&ast2);
        ast_deinit(&ast1);
        parser_deinit(&parser);
    }

    struct parser parser;
    struct ast ast1, ast2;
    struct YYLTYPE loc = { 0, 0 };
};

TEST_F(NAME, parse)
{
    ASSERT_THAT(parser_parse(&parser, "0xa->0xb*|0xc+", &ast1), Eq(0));
}

TEST_F(NAME, parse_0xa)
{
    ast_set_root(&ast1, ast_motion(&ast1, 0xa, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_0xa_rep1)
{
    ast_set_root(&ast1,
        ast_repetition(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            1, -1, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa+", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_0xa_rep2)
{
    ast_set_root(&ast1,
        ast_repetition(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            0, -1, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa*", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_0xa_rep3)
{
    ast_set_root(&ast1,
        ast_repetition(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            5, -1, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa 5,", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_0xa_rep4)
{
    ast_set_root(&ast1,
        ast_repetition(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            5, 10, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa 5,10", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_fh_0xa_os)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_union(&ast1,
                ast_motion(&ast1, 0x62dd02058, &loc),
                ast_motion(&ast1, 0x62abde441, &loc),
                &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                AST_CTX_OS, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "fh 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_fh_0xa_os_with_parent)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_motion(&ast1, 0xb, &loc),
            ast_statement(&ast1,
                ast_union(&ast1,
                    ast_motion(&ast1, 0x62dd02058, &loc),
                    ast_motion(&ast1, 0x62abde441, &loc),
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->fh 0xa os", &ast2), Eq(0));
    ast_export_dot(&ast1, "ast1.dot");
    ast_export_dot(&ast2, "ast2.dot");
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}
