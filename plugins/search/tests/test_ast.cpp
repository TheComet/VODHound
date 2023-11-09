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
    int jump = ast_union(&ast1,
        ast_union(&ast1,
            ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
            ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
            &loc),
        ast_union(&ast1,
            ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
            ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
            &loc),
        &loc);
    int dj = ast_union(&ast1,
        ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
        ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
        &loc);
    int timing = ast_timing(&ast1,
        -1,
        dj,
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, jump);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_motion(&ast1, hash40_cstr("jump_squat"), &loc),
                    jump,
                    &loc),
                timing,
                &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                AST_CTX_OS, &loc),
            &loc));
    ast_export_dot(&ast1, "ast.dot");
    ASSERT_THAT(parser_parse(&parser, "idj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, parse_idj_0xa_os_with_parent)
{
    int jump = ast_union(&ast1,
        ast_union(&ast1,
            ast_motion(&ast1, hash40_cstr("jump_f_mini"), &loc),
            ast_motion(&ast1, hash40_cstr("jump_b_mini"), &loc),
            &loc),
        ast_union(&ast1,
            ast_motion(&ast1, hash40_cstr("jump_f"), &loc),
            ast_motion(&ast1, hash40_cstr("jump_b"), &loc),
            &loc),
        &loc);
    int dj = ast_union(&ast1,
        ast_motion(&ast1, hash40_cstr("jump_aerial_f"), &loc),
        ast_motion(&ast1, hash40_cstr("jump_aerial_b"), &loc),
        &loc);
    int timing = ast_timing(&ast1,
        -1,
        dj,
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, jump);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_motion(&ast1, 0xb, &loc),
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_statement(&ast1,
                        ast_motion(&ast1, hash40_cstr("jump_squat"), &loc),
                        jump,
                        &loc),
                    timing,
                    &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xa, &loc),
                    AST_CTX_OS, &loc),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xb->idj 0xa os", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, post_ctx)
{
    ast_set_root(&ast1,
        ast_union(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            ast_context(&ast1,
                ast_motion(&ast1, 0xb, &loc),
                (enum ast_ctx_flags)(AST_CTX_OS | AST_CTX_HIT),
                &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa|0xb os|hit", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, pre_ctx)
{
    ast_set_root(&ast1,
        ast_union(&ast1,
            ast_context(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                (enum ast_ctx_flags)(AST_CTX_RISING | AST_CTX_FALLING),
                &loc),
            ast_motion(&ast1, 0xb, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "rising|falling 0xa|0xb", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, pre_and_post_ctx)
{
    ast_set_root(&ast1,
        ast_union(&ast1,
            ast_union(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                ast_context(&ast1,
                    ast_motion(&ast1, 0xb, &loc),
                    (enum ast_ctx_flags)(AST_CTX_RISING | AST_CTX_FALLING | AST_CTX_OS | AST_CTX_WHIFF),
                    &loc),
                &loc),
            ast_motion(&ast1, 0xc, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa | rising|falling 0xb os|whiff | 0xc", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, timing_with_ref1)
{
    int ref = ast_motion(&ast1, 0xa, &loc);
    int timing = ast_timing(&ast1,
        ast_motion(&ast1, 0xa, &loc),
        ast_motion(&ast1, 0xc, &loc),
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, ref);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_statement(&ast1,
                        ref,
                        ast_motion(&ast1, 0xb, &loc),
                        &loc),
                    timing,
                    &loc),
                ast_motion(&ast1, 0xd, &loc),
                &loc),
            ast_motion(&ast1, 0xe, &loc),
            &loc));
    
    ASSERT_THAT(parser_parse(&parser, "0xa->0xb->f1,0xa 0xc->0xd->0xe", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, timing_with_ref2)
{
    int ref = ast_motion(&ast1, 0xa, &loc);
    int timing = ast_timing(&ast1,
        ast_motion(&ast1, 0xa, &loc),
        ast_motion(&ast1, 0xc, &loc),
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, ref);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_statement(&ast1,
                        ref,
                        ast_motion(&ast1, 0xa, &loc),
                        &loc),
                    timing,
                    &loc),
                ast_motion(&ast1, 0xa, &loc),
                &loc),
            ast_motion(&ast1, 0xa, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa->0xa->f1,0xa 0xc->0xa->0xa", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, timing_with_ref3)
{
    int ref = ast_motion(&ast1, 0xa, &loc);
    int timing = ast_timing(&ast1,
        ast_motion(&ast1, 0xa, &loc),
        ast_motion(&ast1, 0xa, &loc),
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, ref);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ast_statement(&ast1,
                    ast_statement(&ast1,
                        ref,
                        ast_motion(&ast1, 0xa, &loc),
                        &loc),
                    timing,
                    &loc),
                ast_motion(&ast1, 0xa, &loc),
                &loc),
            ast_motion(&ast1, 0xa, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa->0xa->f1,0xa 0xa->0xa->0xa", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, timing_with_ref4)
{
    ASSERT_THAT(parser_parse(&parser, "0xa->0xb->f1,0xc 0xc->0xd->0xe", &ast2), Ne(0));
}

TEST_F(NAME, timing_with_ref5)
{
    ASSERT_THAT(parser_parse(&parser, "0xa->0xb->f1,0xc 0xc->0xc->0xc", &ast2), Ne(0));
}

TEST_F(NAME, timing_with_ref6)
{
    int ref = ast_statement(&ast1,
        ast_motion(&ast1, 0xa, &loc),
        ast_motion(&ast1, 0xa, &loc),
        &loc);
    int timing = ast_timing(&ast1,
        ast_duplicate(&ast1, ref),
        ast_motion(&ast1, 0xc, &loc),
        1, 1, &loc);
    ast_timing_set_ref(&ast1, timing, ref);
    ast_set_root(&ast1,
        ast_statement(&ast1,
            ast_statement(&ast1,
                ref,
                timing,
                &loc),
            ast_motion(&ast1, 0xd, &loc),
            &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa->0xa->f1,(0xa->0xa) 0xc->0xd", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage1)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.1f, 999.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa >20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage2)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.f, 999.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa >=20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage3)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            0.f, 19.9f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa <20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage4)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            0.f, 20.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa <=20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range1)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.f, 30.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa 20%-30%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range2)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.1f, 29.9f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa >20% <30%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range3)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.1f, 29.9f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa <30% >20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range4)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_motion(&ast1, 0xa, &loc),
            20.1f, 29.9f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa <30% >20% >18% <32% <35%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range5)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_damage(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                30.1f, 999.f, &loc),
            0.f, 19.9f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa >30% <20%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range6)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_damage(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                15.f, 30.f, &loc),
            50.f, 80.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa 10%-30% 50%-80% >=15%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}

TEST_F(NAME, damage_range7)
{
    ast_set_root(&ast1,
        ast_damage(&ast1,
            ast_damage(&ast1,
                ast_motion(&ast1, 0xa, &loc),
                15.f, 30.f, &loc),
            50.f, 80.f, &loc));
    ASSERT_THAT(parser_parse(&parser, "0xa >=15% 10%-30% 50%-80%", &ast2), Eq(0));
    EXPECT_THAT(ast_trees_equal(&ast1, 0, &ast2, 0), IsTrue());
}
