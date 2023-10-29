#include "search/asm.h"
#include "search/ast.h"
#include "search/dfa.h"
#include "search/frame_data.h"
#include "search/nfa.h"
#include "search/parser.h"
#include "search/range.h"

#include "vh/hash40.h"
#include "vh/init.h"

#include <stdio.h>
#include <inttypes.h>

union symbol symbols[10000];
const struct frame_data fdata = { symbols };
const struct range range = { 0, 10000 };

static union symbol
h40_to_symbol(const char* str)
{
    union symbol s;
    uint64_t h40 = hash40_str(str);
    s.u64 = 0;
    s.motionl = h40 & 0xFFFFFFFF;
    s.motionh = h40 >> 32UL;
    return s;
}

static void
init_symbols(void)
{
    int i;
    for (i = 0; i != 7000; ++i)
        symbols[i] = h40_to_symbol("wait");
#if 0
    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("grab");
    symbols[i++] = h40_to_symbol("wait");
    symbols[i++] = h40_to_symbol("pummel");
    symbols[i++] = h40_to_symbol("pummel");
    symbols[i++] = h40_to_symbol("dthrow");
    symbols[i++] = h40_to_symbol("fh");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("fh");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("fh");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("fh");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("dj");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("fair");
    symbols[i++] = h40_to_symbol("qa1");
#endif

    for (; i < 8000; ++i)
        symbols[i] = h40_to_symbol("wait");

    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("grab");
    symbols[i++] = h40_to_symbol("wait");
    symbols[i++] = h40_to_symbol("pummel");
    symbols[i++] = h40_to_symbol("pummel");
    symbols[i++] = h40_to_symbol("dthrow");
    symbols[i++] = h40_to_symbol("sh");
    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("grab");
    symbols[i++] = h40_to_symbol("dthrow");
    symbols[i++] = h40_to_symbol("sh");
    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("turn");
    symbols[i++] = h40_to_symbol("utilt");
    symbols[i++] = h40_to_symbol("sh");
    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("utilt");
    symbols[i++] = h40_to_symbol("sh");
    symbols[i++] = h40_to_symbol("nair");
    symbols[i++] = h40_to_symbol("land");
    symbols[i++] = h40_to_symbol("utilt");
    symbols[i++] = h40_to_symbol("fh");
    symbols[i++] = h40_to_symbol("uair");
    symbols[i++] = h40_to_symbol("dj");
    symbols[i++] = h40_to_symbol("bair");

    for (; i < 10000; ++i)
        symbols[i] = h40_to_symbol("wait");
}

static void
run_on_test_data(const struct dfa_table* dfa)
{
    struct range found = dfa_run(dfa, &fdata, range);
    fprintf(stderr, "Interpreted match (%d-%d) :", found.start, found.end);
    for (; found.start != found.end; ++found.start)
        fprintf(stderr, " 0x%" PRIx64, ((uint64_t)symbols[found.start].motionh << 32) | ((uint64_t)symbols[found.start].motionl));
    fprintf(stderr, "\n");
}

static void
run_asm_on_test_data(const struct asm_dfa* assembly)
{
    struct range found = asm_run(assembly, &fdata, range);
    fprintf(stderr, "ASM match (%d-%d)         :", found.start, found.end);
    for (; found.start != found.end; ++found.start)
        fprintf(stderr, " 0x%" PRIx64, ((uint64_t)symbols[found.start].motionh << 32) | ((uint64_t)symbols[found.start].motionl));
    fprintf(stderr, "\n");
}

#include <time.h>

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
    init_symbols();

    parser_init(&parser);
    ast = parser_parse(&parser, argv[1]);
    parser_deinit(&parser);
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
