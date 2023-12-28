#include "search/asm.h"
#include "search/ast.h"
#include "search/ast_post.h"
#include "search/dfa.h"
#include "search/search_index.h"
#include "search/nfa.h"
#include "search/parser.h"

#include "vh/db.h"
#include "vh/frame_data.h"
#include "vh/hm.h"
#include "vh/log.h"
#include "vh/mem.h"
#include "vh/plugin.h"
#include "vh/str.h"

#include <gtk/gtk.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

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

struct search
{
    struct parser parser;
    struct ast ast;
    struct asm_dfa assembly;
    struct frame_data fdata;
    struct search_index index;

    int fighter_id;
    int fighter_idx;
};

static void
search_init(struct search* search)
{
    parser_init(&search->parser);
    ast_init(&search->ast);
    frame_data_init(&search->fdata);
    search_index_init(&search->index);
    asm_init(&search->assembly);

    search->fighter_id = -1;
    search->fighter_idx = -1;
}

static void
search_deinit(struct search* search)
{
    asm_deinit(&search->assembly);
    search_index_deinit(&search->index);
    frame_data_deinit(&search->fdata);
    ast_deinit(&search->ast);
    parser_deinit(&search->parser);
}

static int
search_compile(struct search* search, const char* text, struct db_interface* dbi, struct db* db)
{
    struct nfa_graph nfa;
    struct dfa_table dfa;
    int fighter_id = 8;

    ast_clear(&search->ast);

    if (parser_parse(&search->parser, text, &search->ast) < 0)
        goto parse_failed;
    ast_export_dot(&search->ast, "ast.dot");

    if (ast_post_labels_to_motions(&search->ast, dbi, db, fighter_id) < 0)
        goto patch_motions_failed;
    ast_export_dot(&search->ast, "ast.dot");

    if (nfa_compile(&nfa, &search->ast))
        goto nfa_compile_failed;
    nfa_export_dot(&nfa, "nfa.dot");

    dfa_init(&dfa);
    if (dfa_from_nfa(&dfa, &nfa))
        goto dfa_compile_failed;
    dfa_export_dot(&dfa, "dfa.dot");

    if (asm_compile(&search->assembly, &dfa))
        goto assemble_failed;

    dfa_deinit(&dfa);
    nfa_deinit(&nfa);

    return 0;

    assemble_failed      : dfa_deinit(&dfa);
    dfa_compile_failed   : nfa_deinit(&nfa);
    nfa_compile_failed   :
    patch_motions_failed :
    parse_failed         : return -1;
}

static int
on_notation_label(const char* label, void* user_data)
{
    struct str* str = user_data;
    cstr_set(str, label);
    return 0;
}

static void
search_run(struct search* search, struct db_interface* dbi, struct db* db)
{
    const union symbol* symbols;
    struct range window;
    struct vec results;
    struct sequence seq;
    struct str label;
    int i;
    int fighter_id = 8;
    int usage_id = 1;  /* hard coded for now to "NOTATION" */

    if (!search_index_has_data(&search->index))
        return;
    if (!asm_is_compiled(&search->assembly))
        return;

    vec_init(&results, sizeof(struct range));
    str_init(&label);

    symbols = search_index_symbols(&search->index, 0);
    window = search_index_range(&search->index, 0);
    asm_find_all(&results, &search->assembly, symbols, window);

    fprintf(stderr, "Matching Ranges (window %d-%d):\n", window.start, window.end);
    VEC_FOR_EACH(&results, struct range, r)
        fprintf(stderr, "  %d-%d: ", r->start, r->end);
        for (i = r->start; i != r->end; ++i)
        {
            uint64_t motion = ((uint64_t)symbols[i].motionh << 32) | symbols[i].motionl;
            str_clear(&label);
            if (dbi->motion_label.to_notation_label(db, fighter_id, motion, usage_id, on_notation_label, &label) != 0)
                str_fmt(&label, "0x%" PRIx64, motion);
            if (i != r->start) fprintf(stderr, " -> ");
            fprintf(stderr, "%.*s", label.len, label.data);
        }
        fprintf(stderr, "\n");
    VEC_END_EACH

    sequence_init(&seq);
    fprintf(stderr, "Matching Sequences (window %d-%d):\n", window.start, window.end);
    VEC_FOR_EACH(&results, struct range, r)
        fprintf(stderr, "  %d-%d: ", r->start, r->end);
        sequence_from_search_result(&seq, symbols, *r, &search->ast);
        SEQ_FOR_EACH(&seq, i)
            uint64_t motion = ((uint64_t)symbols[i].motionh << 32) | symbols[i].motionl;
            str_clear(&label);
            if (dbi->motion_label.to_notation_label(db, fighter_id, motion, usage_id, on_notation_label, &label) != 0)
                str_fmt(&label, "0x%" PRIx64, motion);
            if (i != seq_first(&seq)) fprintf(stderr, " -> ");
            fprintf(stderr, "%.*s", label.len, label.data);
        SEQ_END_EACH
        fprintf(stderr, "\n");
        sequence_clear(&seq);
    VEC_END_EACH
    sequence_deinit(&seq);

    str_deinit(&label);
    vec_deinit(&results);
}

struct plugin_ctx
{
    struct db_interface* dbi;
    struct db* db;

    struct search search;
};

static struct plugin_ctx*
create(GTypeModule* type_module, struct db_interface* dbi, struct db* db)
{
    struct plugin_ctx* ctx = mem_alloc(sizeof(struct plugin_ctx));
    memset(ctx, 0, sizeof *ctx);

    ctx->dbi = dbi;
    ctx->db = db;

    search_init(&ctx->search);

    return ctx;
}

static void
destroy(GTypeModule* type_module, struct plugin_ctx* ctx)
{
    search_deinit(&ctx->search);
    mem_free(ctx);
}

int
sequence_from_search_result(struct sequence* seq, const union symbol* symbols, struct range range, const struct ast* ast)
{
    int s1, s2;
    for (s1 = range.start; s1 < range.end; ++s1)
    {
        uint64_t motion1 = ((uint64_t)symbols[s1].motionh << 32) | symbols[s1].motionl;
        struct strlist_str* label1 = hm_find(&ast->merged_labels, &motion1);

        if (vec_push(&seq->idxs, &s1) < 0)
            return -1;

        if (label1)
            for (s2 = s1 + 1; s2 < range.end; ++s2)
            {
                uint64_t motion2 = ((uint64_t)symbols[s2].motionh << 32) | symbols[s2].motionl;
                struct strlist_str* label2 = hm_find(&ast->merged_labels, &motion2);
                if (label2 && str_equal(
                        strlist_to_view(&ast->labels, *label1),
                        strlist_to_view(&ast->labels, *label2)))
                    s1++;
                else
                    break;
            }
    }

    return 0;
}

static int
on_search_text_changed(GtkEntry* self, struct plugin_ctx* ctx)
{
    const char* text = gtk_editable_get_text(GTK_EDITABLE(self));
    if (search_compile(&ctx->search, text, ctx->dbi, ctx->db) < 0)
        return -1;

    search_run(&ctx->search, ctx->dbi, ctx->db);
    return 0;
}

static GtkWidget* ui_center_create(struct plugin_ctx* ctx)
{
    GtkWidget* search_box;
    GtkWidget* label;
    GtkWidget* vbox;
    
    search_box = gtk_entry_new();
    g_signal_connect(search_box, "changed", G_CALLBACK(on_search_text_changed), ctx);

    label = gtk_label_new("Search:");
    gtk_label_set_xalign(GTK_LABEL(label), 0);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_box_append(GTK_BOX(vbox), search_box);

    return g_object_ref_sink(vbox);
}
static void ui_center_destroy(struct plugin_ctx* ctx, GtkWidget* ui)
{
    g_object_unref(ui);
}

static struct ui_pane_interface ui_center = {
    ui_center_create,
    ui_center_destroy
};

static void select_replays(struct plugin_ctx* ctx, const int* game_ids, int count)
{
    search_index_clear(&ctx->search.index);

    if (frame_data_load(&ctx->search.fdata, game_ids[0]) != 0)
        return;

    if (search_index_build(&ctx->search.index, &ctx->search.fdata) < 0)
        return;

    search_run(&ctx->search, ctx->dbi, ctx->db);
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
    create, destroy,
    NULL,
    &ui_center,
    &replays,
    NULL
};
