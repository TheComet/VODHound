#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/* #include "search/parser.y.h" */
typedef void* yyscan_t;
typedef struct yypstate yypstate;

union ast_node;

struct parser
{
    yyscan_t scanner;
    yypstate* parser;
};

int
parser_init(struct parser* parser);

void
parser_deinit(struct parser* parser);

union ast_node*
parser_parse(struct parser* parser, const char* text);

#if defined(__cplusplus)
}
#endif
