#include "search/asm.h"
#include "search/ast.h"
#include "search/ast_post.h"
#include "search/dfa.h"
#include "search/search_index.h"
#include "search/nfa.h"
#include "search/parser.h"

#include "vh/frame_data.h"
#include "vh/hm.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"

#include "iup.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct plugin_ctx
{
    struct db_interface* dbi;
    struct db* db;
    Ihandle* error_label;
    struct parser parser;
    struct hm original_labels;
    struct asm_dfa asm_dfa;
    struct frame_data fdata;
    struct search_index index;
};

static hash32
hash_hash40(const void* data, int len)
{
    const uint64_t* motion = data;
    return *motion & 0xFFFFFFFF;
}

static struct plugin_ctx*
create(struct db_interface* dbi, struct db* db)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);
    ctx->dbi = dbi;
    ctx->db = db;
    parser_init(&ctx->parser);
    hm_init_with_options(&ctx->original_labels, sizeof(uint64_t), sizeof(char*), VH_HM_MIN_CAPACITY, hash_hash40, (hm_compare_func)memcmp);
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
    HM_FOR_EACH(&ctx->original_labels, uint64_t, char*, motion, label)
        mem_free(*label);
    HM_END_EACH
    hm_deinit(&ctx->original_labels);
    parser_deinit(&ctx->parser);
    mem_free(ctx);
}

struct sequence
{
    struct vec idxs;
};

void
sequence_init(struct sequence* seq)
{
    vec_init(&seq->idxs, sizeof(int));
}
void
sequence_deinit(struct sequence* seq)
{
    vec_deinit(&seq->idxs);
}
void
sequence_clear(struct sequence* seq)
{
    vec_clear(&seq->idxs);
}
#define seq_first(s) (*(int*)vec_front(&(s)->idxs))
#define SEQ_FOR_EACH(s, var) VEC_FOR_EACH(&(s)->idxs, int, seq_##var) int var = *seq_##var;
#define SEQ_END_EACH VEC_END_EACH

int
sequence_from_search_result(struct sequence* seq, const union symbol* symbols, struct range range, const struct hm* labels)
{
    int s1, s2;
    for (s1 = range.start; s1 < range.end; ++s1)
    {
        uint64_t motion1 = ((uint64_t)symbols[s1].motionh << 32) | symbols[s1].motionl;
        char** label1 = hm_find(labels, &motion1);

        if (vec_push(&seq->idxs, &s1) < 0)
            return -1;

        if (label1)
            for (s2 = s1 + 1; s2 < range.end; ++s2)
            {
                uint64_t motion2 = ((uint64_t)symbols[s1].motionh << 32) | symbols[s1].motionl;
                char** label2 = hm_find(labels, &motion2);
                if (label2 && strcmp(*label1, *label2) == 0)
                    s1++;
                else
                    break;
            }
    }

    return 0;
}

static void
run_search(struct plugin_ctx* ctx)
{
    const union symbol* symbols;
    struct range window;
    struct vec results;
    struct sequence seq;
    if (ctx->asm_dfa.size == 0)
        return;
    if (!search_index_has_data(&ctx->index))
        return;

    vec_init(&results, sizeof(struct range));
    symbols = search_index_symbols(&ctx->index, 0);
    window = search_index_range(&ctx->index, 0);
    asm_find_all(&results, &ctx->asm_dfa, symbols, window);

    fprintf(stderr, "Matching Ranges (window %d-%d):\n", window.start, window.end);
    VEC_FOR_EACH(&results, struct range, r)
        fprintf(stderr, "  %d-%d: ", r->start, r->end);
        for (int i = r->start; i != r->end; ++i)
        {
            uint64_t motion = ((uint64_t)symbols[i].motionh << 32) | symbols[i].motionl;
            char** label = hm_find(&ctx->original_labels, &motion);
            if (i != r->start) fprintf(stderr, " -> ");
            if (label)
                fprintf(stderr, "%s", *label);
            else
                fprintf(stderr, "0x%" PRIx64, motion);
        }
        fprintf(stderr, "\n");
    VEC_END_EACH

    sequence_init(&seq);
    fprintf(stderr, "Matching Sequences (window %d-%d):\n", window.start, window.end);
    VEC_FOR_EACH(&results, struct range, r)
        fprintf(stderr, "  %d-%d: ", r->start, r->end);
        sequence_from_search_result(&seq, symbols, *r, &ctx->original_labels);
        SEQ_FOR_EACH(&seq, i)
            uint64_t motion = ((uint64_t)symbols[i].motionh << 32) | symbols[i].motionl;
            char** label = hm_find(&ctx->original_labels, &motion);
            if (i != seq_first(&seq)) fprintf(stderr, " -> ");
            if (label)
                fprintf(stderr, "%s", *label);
            else
                fprintf(stderr, "0x%" PRIx64, motion);
        SEQ_END_EACH
        fprintf(stderr, "\n");
    VEC_END_EACH
    sequence_deinit(&seq);

    vec_deinit(&results);
}

static int
on_search_text_changed(Ihandle* search_box, int c, char* new_value)
{
    struct ast ast;
    struct nfa_graph nfa;
    struct dfa_table dfa;
    struct plugin_ctx* ctx = (struct plugin_ctx*)IupGetAttribute(search_box, "plugin_ctx");
    int fighter_id = 8;

    if (ctx->asm_dfa.size)
    {
        asm_deinit(&ctx->asm_dfa);
        ctx->asm_dfa.next_state = NULL;
        ctx->asm_dfa.size = 0;
    }
    HM_FOR_EACH(&ctx->original_labels, uint64_t, char*, motion, label)
        mem_free(*label);
    HM_END_EACH
    hm_clear(&ctx->original_labels);

    if (parser_parse(&ctx->parser, new_value, &ast) < 0)
        goto parse_failed;
    ast_export_dot(&ast, "ast.dot");
    if (ast_post_patch_motions(&ast, ctx->dbi, ctx->db, fighter_id, &ctx->original_labels) < 0)
        goto patch_motions_failed;
    ast_export_dot(&ast, "ast.dot");
    if (nfa_compile(&nfa, &ast))
        goto nfa_compile_failed;
    nfa_export_dot(&nfa, "nfa.dot");
    if (dfa_compile(&dfa, &nfa))
        goto dfa_compile_failed;
    dfa_export_dot(&dfa, "dfa.dot");
    if (asm_compile(&ctx->asm_dfa, &dfa))
        goto assemble_failed;

    run_search(ctx);

    assemble_failed      : dfa_deinit(&dfa);
    dfa_compile_failed   : nfa_deinit(&nfa);
    nfa_compile_failed   :
    patch_motions_failed : ast_deinit(&ast);
    parse_failed         : return IUP_DEFAULT;
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
