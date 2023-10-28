#include "search/asm.h"
#include "search/ast.h"
#include "search/dfa.h"
#include "search/frame_data.h"
#include "search/nfa.h"
#include "search/parser.h"
#include "search/range.h"

#include "vh/init.h"

#include <stdio.h>
#include <inttypes.h>

static void
run_on_test_data(const struct dfa_table* dfa)
{
    union symbol symbols[11] = {
        { 0xa },
        { 0xb },
        { 0xb },
        { 0xa },
        { 0xc },
        { 0xc },
        { 0xc },
        { 0xc },
        { 0xd },
        { 0xe },
        { 0xf },
    };
    struct frame_data fdata = { symbols };
    struct range range = { 0, 11 };

    range = dfa_run(dfa, &fdata, range);
    fprintf(stderr, "Interpreted match :");
    for (; range.start != range.end; ++range.start)
        fprintf(stderr, " 0x%" PRIx64, ((uint64_t)symbols[range.start].motionh << 32) | ((uint64_t)symbols[range.start].motionl));
    fprintf(stderr, "\n");
}

static void
run_asm_on_test_data(const struct asm_dfa* assembly)
{
    union symbol symbols[11] = {
        { 0xa },
        { 0xb },
        { 0xb },
        { 0xa },
        { 0xc },
        { 0xc },
        { 0xc },
        { 0xc },
        { 0xd },
        { 0xe },
        { 0xf },
    };
    struct frame_data fdata = { symbols };
    struct range range = { 0, 11 };

    range = asm_run(assembly, &fdata, range);
    fprintf(stderr, "ASM match         :");
    for (; range.start != range.end; ++range.start)
        fprintf(stderr, " 0x%" PRIx64, ((uint64_t)symbols[range.start].motionh << 32) | ((uint64_t)symbols[range.start].motionl));
    fprintf(stderr, "\n");
}

int main(int argc, char** argv)
{
    struct parser parser;
    union ast_node* ast = NULL;
    struct nfa_graph nfa;
    struct dfa_table dfa;
    struct asm_dfa asm_dfa;
    int nfa_result = -1;
    int dfa_result = -1;
    int asm_result = -1;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [options] <query text>\n", argv[0]);
        return 1;
    }

    vh_threadlocal_init();
    vh_init();

    parser_init(&parser);
    ast = parser_parse(&parser, argv[1]);
    //ast = parser_parse(&parser, argv[1]);

    if (ast)
    {
        ast_export_dot(ast, "ast.dot");
        nfa_result = nfa_compile(&nfa, ast);
        ast_destroy_recurse(ast);
    }

    if (nfa_result == 0)
    {
        nfa_export_dot(&nfa, "nfa.dot");
        dfa_result = dfa_compile(&dfa, &nfa);
        nfa_export_dot(&nfa, "dfa.dot");
        nfa_deinit(&nfa);
    }

    if (dfa_result == 0)
    {
        dfa_export_dot(&dfa, "dfa.dot");
        run_on_test_data(&dfa);
        asm_result = asm_compile(&asm_dfa, &dfa);
        dfa_deinit(&dfa);
    }

    if (asm_result == 0)
    {
        run_asm_on_test_data(&asm_dfa);
        asm_deinit(&asm_dfa);
    }

    vh_deinit();
    vh_threadlocal_deinit();

    return 0;
}
