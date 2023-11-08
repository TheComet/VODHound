#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include "vh/hm.h"
#include "vh/str.h"

enum ast_type
{
    AST_STATEMENT,
    AST_REPETITION,
    AST_UNION,
    AST_INVERSION,
    AST_WILDCARD,
    AST_LABEL,
    AST_MOTION,
    AST_CONTEXT,
    AST_TIMING
};

enum ast_ctx_flags {
    AST_CTX_OS      = (1 << 0),
    AST_CTX_OOS     = (1 << 1),
    AST_CTX_HIT     = (1 << 2),
    AST_CTX_WHIFF   = (1 << 3),
    AST_CTX_CLANK   = (1 << 4),
    AST_CTX_TRADE   = (1 << 5),
    AST_CTX_CROSSUP = (1 << 6),
    AST_CTX_KILL    = (1 << 7),
    AST_CTX_DIE     = (1 << 8),
    AST_CTX_BURY    = (1 << 9),
    AST_CTX_BURIED  = (1 << 10),
    AST_CTX_RISING  = (1 << 11),
    AST_CTX_FALLING = (1 << 12),
    AST_CTX_SH      = (1 << 13),
    AST_CTX_FH      = (1 << 14),
    AST_CTX_DJ      = (1 << 15),
    AST_CTX_FS      = (1 << 16),
    AST_CTX_IDJ     = (1 << 17)
};

struct ast_location
{
    int begin;
    int end;
};

union ast_node
{
    struct info {
        enum ast_type type;
        struct ast_location loc;
    } info;

    struct base {
        struct info info;
        int left;
        int right;
    } base;

    struct statement {
        struct info info;
        int child;
        int next;
    } statement;

    struct repetition {
        struct info info;
        int child;
        int _padding;
        int min_reps;
        int max_reps;
    } repetition;

    struct union_ {
        struct info info;
        int child;
        int next;
    } union_;

    struct inversion {
        struct info info;
        int child;
        int _padding;
    } inversion;

    struct context {
        struct info info;
        int child;
        int _padding;
        enum ast_ctx_flags flags;
    } context;

    struct label {
        struct info info;
        int _padding1;
        int _padding2;
        struct strlist_str label;
    } label;

    struct motion {
        struct info info;
        int _padding1;
        int _padding2;
        uint64_t motion;
    } motion;

    struct timing {
        struct info info;
        int child;
        int rel_to;
        int start;
        int end;
        int rel_to_ref;
    } timing;
};

struct ast
{
    union ast_node* nodes;
    struct strlist labels;
    struct hm merged_labels;
    int node_count;
    int node_capacity;
};

struct YYLTYPE;

int ast_init(struct ast* ast);
void ast_deinit(struct ast* ast);
void ast_clear(struct ast* ast);

int ast_statement(struct ast* ast, int child, int next, const struct YYLTYPE* loc);
int ast_repetition(struct ast* ast, int child, int min_reps, int max_reps, const struct YYLTYPE* loc);
int ast_union(struct ast* ast, int child, int next, const struct YYLTYPE* loc);
int ast_inversion(struct ast* ast, int child, const struct YYLTYPE* loc);
int ast_wildcard(struct ast* ast, const struct YYLTYPE* loc);
int ast_label(struct ast* ast, struct strlist_str label, const struct YYLTYPE* loc);
int ast_motion(struct ast* ast, uint64_t motion, const struct YYLTYPE* loc);
int ast_context(struct ast* ast, int child, enum ast_ctx_flags flags, const struct YYLTYPE* loc);
int ast_timing(struct ast* ast, int child, int rel_to, int start, int end, const struct YYLTYPE* loc);

int ast_export_dot(const struct ast* ast, const char* file_name);

#if defined(__cplusplus)
}
#endif
