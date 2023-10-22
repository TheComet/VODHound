#pragma once

#include <stdint.h>

enum ast_type
{
    AST_STATEMENT,
    AST_REPETITION,
    AST_UNION,
    AST_INVERSION,
    AST_WILDCARD,
    AST_LABEL,
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
        union ast_node* left;
        union ast_node* right;
    } base;

    struct statement {
        struct info info;
        union ast_node* next;
        union ast_node* child;
    } statement;

    struct repetition {
        struct info info;
        union ast_node* child;
        union ast_node* _padding;
        int min_reps;
        int max_reps;
    } repetition;

    struct union_ {
        struct info info;
        union ast_node* next;
        union ast_node* child;
    } union_;

    struct inversion {
        struct info info;
        union ast_node* child;
        union ast_node* _padding;
    } inversion;

    struct context_qualifier {
        struct info info;
        union ast_node* child;
        union ast_node* _padding;
        uint8_t flags;
    } context_qualifier;

    struct labels {
        struct info info;
        union ast_node* _padding1;
        union ast_node* _padding2;
        char* label;
        char* opponent_label;
    } labels;
};

struct YYLTYPE;

union ast_node* ast_statement(union ast_node* child, union ast_node* next, struct YYLTYPE* loc);
union ast_node* ast_repetition(union ast_node* child, int min_reps, int max_reps, struct YYLTYPE* loc);
union ast_node* ast_union(union ast_node* child, union ast_node* next, struct YYLTYPE* loc);
union ast_node* ast_inversion(union ast_node* child, struct YYLTYPE* loc);
union ast_node* ast_context_qualifier(union ast_node* child, uint8_t flags, struct YYLTYPE* loc);
union ast_node* ast_label_steal(char* label, struct YYLTYPE* loc);
union ast_node* ast_labels_steal(char* label, char* opponent_label, struct YYLTYPE* loc);
union ast_node* ast_wildcard(struct YYLTYPE* loc);

void ast_destroy_single(union ast_node* node);
void ast_destroy_recurse(union ast_node* node);

int ast_export_dot(union ast_node* root, const char* file_name);
