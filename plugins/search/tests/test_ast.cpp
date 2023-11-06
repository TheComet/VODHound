#include "gmock/gmock.h"

#include "search/ast.h"
#include "search/ast_ops.h"
#include "search/parser.h"
#include "search/parser.y.h"

#include "vh/hash40.h"

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
                ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
                ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
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
                    ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
                    ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->fh 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_sh_0xa_os)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_union(&ast1,
                ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
                ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
                &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                AST_CTX_OS, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "sh 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_sh_0xa_os_with_parent)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_motion(&ast1, 0xb, &loc),
            ast_statement(&ast1,
                ast_union(&ast1,
                    ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
                    ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->sh 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_dj_0xa_os)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_union(&ast1,
                ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
                ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
                &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                AST_CTX_OS, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "dj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_dj_0xa_os_with_parent)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_motion(&ast1, 0xb, &loc),
            ast_statement(&ast1,
                ast_union(&ast1,
                    ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
                    ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->dj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_fs_0xa_os)
{
    ASSERT_THAT(parser_parse(&parser, "fs 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_fs_0xa_os_with_parent)
{
    ASSERT_THAT(parser_parse(&parser, "0xb->fs 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_idj_0xa_os)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_motion(&ast1, hash40_cstr("jump_squat"), &loc),
                    ast_union(&ast1,
                        ast_union(&ast1,
                            ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
                            ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
                            &loc),
                        ast_union(&ast1,
                            ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
                            ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
                            &loc),
                        &loc),
                    &loc),
                ast_timing(&ast1,
                    ast_union(&ast1,
                        ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
                        ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
                        &loc),
                    -1, 1, 1, &loc),
                &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                AST_CTX_OS, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "idj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_idj_0xa_os_with_parent)
{
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_motion(&ast1, 0xb, &loc),
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_statement(&ast1,
                        ast_motion(&ast1, hash40_cstr("jump_squat"), &loc),
                        ast_union(&ast1,
                            ast_union(&ast1,
                                ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
                                ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
                                &loc),
                            ast_union(&ast1,
                                ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
                                ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
                                &loc),
                            &loc),
                        &loc),
                    ast_timing(&ast1,
                        ast_union(&ast1,
                            ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
                            ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
                            &loc),
                        -1, 1, 1, &loc),
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->idj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}
