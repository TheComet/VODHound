#include "search/asm.h"
#include "search/ast.h"
#include "search/dfa.h"
#include "search/search_index.h"
#include "search/nfa.h"
#include "search/parser.h"

#include "vh/frame_data.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

#include "iup.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct plugin_ctx
{
    Ihandle* error_label;
    struct parser parser;
    struct asm_dfa asm_dfa;
    struct frame_data fdata;
    struct search_index index;
};

static struct plugin_ctx*
create(void)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);
    parser_init(&ctx->parser);
    search_index_init(&ctx->index);
    return ctx;
}

static void
destroy(struct plugin_ctx* ctx)
{
    if (ctx->asm_dfa.size)
        asm_deinit(&ctx->asm_dfa);
    if (ctx->fdata.frame_count)
        frame_data_free(&ctx->fdata);

    search_index_deinit(&ctx->index);
    parser_deinit(&ctx->parser);
    mem_free(ctx);
}

static void
run_search(struct plugin_ctx* ctx)
{
    const union symbol* symbols;
    struct range window;
    struct vec results;
    if (ctx->asm_dfa.size == 0)
        return;
    if (!search_index_has_data(&ctx->index))
        return;

    vec_init(&results, sizeof(struct range));
    symbols = search_index_symbols(&ctx->index, 0);
    window = search_index_range(&ctx->index, 0);
    asm_find_all(&results, &ctx->asm_dfa, symbols, window);

    fprintf(stderr, "Matches (window %d-%d):\n", window.start, window.end);
    VEC_FOR_EACH(&results, struct range, r)
        fprintf(stderr, "  %d-%d: ", r->start, r->end);
        for (int i = r->start; i != r->end; ++i)
        {
            if (i != r->start) fprintf(stderr, " -> ");
            fprintf(stderr, "0x%" PRIx64, ((uint64_t)symbols[i].motionh << 32) | symbols[i].motionl);
        }
        fprintf(stderr, "\n");
    VEC_END_EACH

    vec_deinit(&results);
}

static int
on_search_text_changed(Ihandle* search_box, int c, char* new_value)
{
    union ast_node* ast;
    struct nfa_graph nfa;
    struct dfa_table dfa;
    struct plugin_ctx* ctx = (struct plugin_ctx*)IupGetAttribute(search_box, "plugin_ctx");

    if (ctx->asm_dfa.size)
    {
        asm_deinit(&ctx->asm_dfa);
        ctx->asm_dfa.next_state = NULL;
        ctx->asm_dfa.size = 0;
    }

    ast = parser_parse(&ctx->parser, new_value);
    if (ast == NULL)
        goto parse_failed;
    if (nfa_compile(&nfa, ast))
        goto nfa_compile_failed;
    if (dfa_compile(&dfa, &nfa))
        goto dfa_compile_failed;
    if (asm_compile(&ctx->asm_dfa, &dfa))
        goto assemble_failed;

    ast_export_dot(ast, "ast.dot");
    nfa_export_dot(&nfa, "nfa.dot");
    dfa_export_dot(&dfa, "dfa.dot");

    run_search(ctx);

    assemble_failed    : dfa_deinit(&dfa);
    dfa_compile_failed : nfa_deinit(&nfa);
    nfa_compile_failed : ast_destroy_recurse(ast);
    parse_failed       : return IUP_DEFAULT;
}

static Ihandle* ui_create(struct plugin_ctx* ctx)
{
    Ihandle* search_box = IupSetAttributes(IupText(NULL), "EXPAND=HORIZONTAL");
    IupSetCallback(search_box, "ACTION", (Icallback)on_search_text_changed);
    IupSetAttribute(search_box, "plugin_ctx", (char*)ctx);

    ctx->error_label = IupSetAttributes(IupLabel(""), "PADDING=10x5");
    IupHide(ctx->error_label);

    return IupSetAttributes(IupVbox(search_box, ctx->error_label, NULL), "MINSIZE=300x0");
}
static void ui_destroy(struct plugin_ctx* ctx, Ihandle* ui)
{
    IupDestroy(ui);
}

static struct ui_pane_interface ui = {
    ui_create,
    ui_destroy
};

static void select_replays(struct plugin_ctx* ctx, const int* game_ids, int count)
{
    search_index_clear(&ctx->index);
    if (ctx->fdata.frame_count)
        frame_data_free(&ctx->fdata);
    ctx->fdata.frame_count = 0;

    if (frame_data_load(&ctx->fdata, game_ids[0]) != 0)
        return;

    if (search_index_build(&ctx->index, &ctx->fdata) < 0)
        return;

    run_search(ctx);
}

static void clear_replays(struct plugin_ctx* ctx)
{

}

static struct replay_interface replays = {
    select_replays,
    clear_replays
};

static struct plugin_info info = {
    "Search",
    "analysis",
    "TheComet",
    "@TheComet93",
    "Search engine for smash ultimate"
};

PLUGIN_API struct plugin_interface vh_plugin = {
    PLUGIN_VERSION,
    0,
    &info,
    create,
    destroy,
    NULL,
    &ui,
    &replays,
    NULL
};
