#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

enum ast_type
{
    AST_STATEMENT,
    AST_REPETITION,
    AST_UNION,
    AST_INVERSION,
    AST_WILDCARD,
    AST_LABEL,
    AST_MOTION,
    AST_CONTEXT_QUALIFIER
};

enum ast_ctx_flags {
    AST_CTX_OS      = 0x0001,
    AST_CTX_OOS     = 0x0002,
    AST_CTX_HIT     = 0x0004,
    AST_CTX_WHIFF   = 0x0008,
    AST_CTX_RISING  = 0x0010,
    AST_CTX_FALLING = 0x0020,
    AST_CTX_IDJ     = 0x0040
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

    struct context_qualifier {
        struct info info;
        int child;
        int _padding;
        uint8_t flags;
    } context_qualifier;

    struct labels {
        struct info info;
        int _padding1;
        int _padding2;
        char* label;
        char* opponent_label;
    } labels;

    struct motion {
        struct info info;
        int _padding1;
        int _padding2;
        uint64_t motion;
    } motion;
};

struct ast
{
    union ast_node* nodes;
    int node_count;
    int node_capacity;
};

struct YYLTYPE;

int ast_statement(struct ast* ast, int child, int next, struct YYLTYPE* loc);
int ast_repetition(struct ast* ast, int child, int min_reps, int max_reps, struct YYLTYPE* loc);
int ast_union(struct ast* ast, int child, int next, struct YYLTYPE* loc);
int ast_inversion(struct ast* ast, int child, struct YYLTYPE* loc);
int ast_wildcard(struct ast* ast, struct YYLTYPE* loc);
int ast_context_qualifier(struct ast* ast, int child, uint8_t flags, struct YYLTYPE* loc);
int ast_label_steal(struct ast* ast, char* label, struct YYLTYPE* loc);
int ast_labels_steal(struct ast* ast, char* label, char* opponent_label, struct YYLTYPE* loc);
int ast_motion(struct ast* ast, uint64_t motion, struct YYLTYPE* loc);

void ast_set_root(struct ast* ast, int node);
void ast_swap_nodes(struct ast* ast, int n1, int n2);
void ast_collapse_into(struct ast* ast, int node, int target);

int ast_init(struct ast* ast);
void ast_deinit(struct ast* ast);
void ast_deinit_node(struct ast* ast, int node);

int ast_export_dot(const struct ast* ast, const char* file_name);

#if defined(__cplusplus)
}
#endif
