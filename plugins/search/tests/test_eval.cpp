#include "gmock/gmock.h"

#include "search/asm.h"
#include "search/ast.h"
#include "search/dfa.h"
#include "search/nfa.h"
#include "search/parser.h"
#include "search/range.h"

#include "vh/hash40.h"

#define NAME eval

using namespace testing;

class NAME : public Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

    void run(const char* text, const std::vector<union symbol>& symbols)
    {
        struct parser parser;
        struct ast ast;
        struct nfa_graph nfa;
        struct dfa_table dfa;
        struct asm_dfa asm_dfa;

        ASSERT_THAT(parser_init(&parser), Eq(0));
        ASSERT_THAT(parser_parse(&parser, text, &ast), Eq(0));
        parser_deinit(&parser);

        ASSERT_THAT(nfa_compile(&nfa, &ast), Eq(0));
        ast_deinit(&ast);

        ASSERT_THAT(dfa_compile(&dfa, &nfa), Eq(0));
        nfa_deinit(&nfa);

        ASSERT_THAT(asm_compile(&asm_dfa, &dfa), Eq(0));

        struct range window = { 0, (int)symbols.size() };
        struct range dfa_res = dfa_find_first(&dfa, symbols.data(), window);
        struct range asm_res = asm_find_first(&asm_dfa, symbols.data(), window);
        dfa_deinit(&dfa);
        asm_deinit(&asm_dfa);

        ASSERT_THAT(dfa_res.start, Eq(asm_res.start));
        ASSERT_THAT(dfa_res.end, Eq(asm_res.end));
        result = dfa_res;
    }

    struct range result;
};

static union symbol
h40_to_symbol(const char* str)
{
    union symbol s;
    uint64_t h40 = hash40_cstr(str);
    s.u64 = 0;
    s.motionl = h40 & 0xFFFFFFFF;
    s.motionh = h40 >> 32UL;
    return s;
}

static union symbol
h40_to_symbol(uint64_t h40)
{
    union symbol s;
    s.u64 = 0;
    s.motionl = h40 & 0xFFFFFFFF;
    s.motionh = h40 >> 32UL;
    return s;
};

TEST_F(NAME, one_symbol)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol("nair"));
    run("nair", symbols);
    EXPECT_THAT(result.start, Eq(0));
    EXPECT_THAT(result.end, Eq(1));
}

TEST_F(NAME, conditional_wildcard_1)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));

    run("0xa->.?->0xb", symbols);
    EXPECT_THAT(result.start, Eq(0));
    EXPECT_THAT(result.end, Eq(3));
}

TEST_F(NAME, conditional_wildcard_2)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0x9));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));

    run("0xa->.?->0xb", symbols);
    EXPECT_THAT(result.start, Eq(1));
    EXPECT_THAT(result.end, Eq(4));
}

TEST_F(NAME, conditional_wildcard_3)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0x9));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xc));

    run("0xa->.?->0xb", symbols);
    EXPECT_THAT(result.start, Eq(1));
    EXPECT_THAT(result.end, Eq(3));
}

TEST_F(NAME, conditional_wildcard_4)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));

    run("0xa->.1,2->0xb", symbols);
    EXPECT_THAT(result.start, Eq(0));
    EXPECT_THAT(result.end, Eq(4));
}

TEST_F(NAME, conditional_wildcard_5)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));

    run("0xa->.0,2->0xb", symbols);
    EXPECT_THAT(result.start, Eq(0));
    EXPECT_THAT(result.end, Eq(4));
}

TEST_F(NAME, repeating_wildcard_1)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xa));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));
    symbols.push_back(h40_to_symbol(0xb));

    run("0xa->.*->0xb", symbols);
    EXPECT_THAT(result.start, Eq(0));
    EXPECT_THAT(result.end, Eq(6));
}

TEST_F(NAME, continue_execution_beyond_initial_accept_condition)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol("fall"));
    symbols.push_back(h40_to_symbol("land"));
    symbols.push_back(h40_to_symbol("grab"));
    symbols.push_back(h40_to_symbol("dthrow"));
    symbols.push_back(h40_to_symbol("sh"));
    symbols.push_back(h40_to_symbol("nair"));
    symbols.push_back(h40_to_symbol("land"));
    symbols.push_back(h40_to_symbol("grab"));
    symbols.push_back(h40_to_symbol("dthrow"));
    symbols.push_back(h40_to_symbol("sh"));
    symbols.push_back(h40_to_symbol("nair"));
    symbols.push_back(h40_to_symbol("land"));
    symbols.push_back(h40_to_symbol("utilt"));
    symbols.push_back(h40_to_symbol("dthrow"));
    symbols.push_back(h40_to_symbol("sh"));
    symbols.push_back(h40_to_symbol("nair"));
    symbols.push_back(h40_to_symbol("land"));
    symbols.push_back(h40_to_symbol("shield"));
    symbols.push_back(h40_to_symbol("jump"));
    symbols.push_back(h40_to_symbol("land"));

    run("(grab|utilt->.1,4)+", symbols);
    EXPECT_THAT(result.start, Eq(2));
    EXPECT_THAT(result.end, Eq(17));
}

TEST_F(NAME, return_to_last_accept_condition_if_continue_fails_to_match)
{
    std::vector<union symbol> symbols;
    symbols.push_back(h40_to_symbol("fall"));
    symbols.push_back(h40_to_symbol("land"));
    symbols.push_back(h40_to_symbol("grab"));
    symbols.push_back(h40_to_symbol("dthrow"));
    symbols.push_back(h40_to_symbol("sh"));
    symbols.push_back(h40_to_symbol("nair"));
    symbols.push_back(h40_to_symbol("grab"));
    symbols.push_back(h40_to_symbol("dthrow"));
    symbols.push_back(h40_to_symbol("sh"));

    run("(grab->.0,2->nair)+", symbols);
    EXPECT_THAT(result.start, Eq(2));
    EXPECT_THAT(result.end, Eq(6));
}
