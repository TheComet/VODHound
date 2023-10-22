%require "3.8"
%code requires
{
    #include <stdint.h>

    typedef struct YYLTYPE YYLTYPE;
    struct YYLTYPE
    {
        int begin, end;
    };
    #define YYLTYPE_IS_DECLARED

    typedef void* yyscan_t;
    union ast_node;
}
%code top
{
    #include "search/parser.y.h"
    #include "search/scanner.lex.h"
    #include "search/ast.h"
    #include "vh/mem.h"
    #include <stdarg.h>

    #define YYLLOC_DEFAULT(Current, Rhs, N) do {                            \
            if (N) {                                                        \
                (Current).begin = YYRHSLOC (Rhs, 1).begin;                  \
                (Current).end   = YYRHSLOC (Rhs, N).end;                    \
            } else {                                                        \
                (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;    \
            }                                                               \
        } while (0)

    static void yyerror(yyscan_t scanner, union ast_node** root, const char* msg, ...);
}

%define api.pure full
%define api.push-pull push
%define api.token.prefix {TOK_}
%define api.header.include {"search/parser.y.h"}

%parse-param {union ast_node** root}
%locations
%define parse.error verbose

/* This is the union that will become known as QPSTYPE in the generated code */
%union {
    char* string_value;
    int integer_value;
    uint8_t ctx_flags;
    union ast_node* node_value;
}

%destructor { mem_free($$); } <string_value>
%destructor { ast_destroy_recurse($$); } <node_value>

%token '.' '*' '+' '?' '(' ')' '|' '!'
%token INTO
%token OS
%token OOS
%token HIT
%token WHIFF
%token RISING
%token FALLING
%token SH
%token FH
%token DJ
%token IDJ
%token<integer_value> NUM
%token<integer_value> PERCENT
%token<string_value> LABEL

%type<node_value> stmnts stmnt repitition union inversion label
%type<ctx_flags> pre_qual post_qual

%right '|'

%start query

%%
query
  : stmnts                        { *root = $1; }
  ;
stmnts
  : stmnts INTO stmnt             { $$ = ast_statement($1, $3, &@$); }
  | stmnt                         { $$ = $1; }
  ;
stmnt
  : pre_qual union post_qual      { $$ = ast_context_qualifier($2, $1 | $3, &@$); }
  | union post_qual               { $$ = ast_context_qualifier($1, $2, &@$); }
  | pre_qual union                { $$ = ast_context_qualifier($2, $1, &@$); }
  | union                         { $$ = $1; }
  ;
union
  : union '|' union               { $$ = ast_union($1, $3, &@$); }
  | repitition                    { $$ = $1; }
  ;
repitition
  : inversion '+'                 { $$ = ast_repetition($1, 1, -1, &@$); }
  | inversion '*'                 { $$ = ast_repetition($1, 0, -1, &@$); }
  | inversion '?'                 { $$ = ast_repetition($1, 0, 1, &@$); }
  | inversion NUM                 { $$ = ast_repetition($1, $2, $2, &@$); }
  | inversion NUM ',' NUM         { $$ = ast_repetition($1, $2, $4, &@$); }
  | inversion NUM ',' '+'         { $$ = ast_repetition($1, $2, -1, &@$); }
  | inversion NUM ',' '*'         { $$ = ast_repetition($1, $2, -1, &@$); }
  | inversion                     { $$ = $1; }
  ;
inversion
  : '!' label                     { $$ = ast_inversion($2, &@$); }
  | label                         { $$ = $1; }
  | '.'                           { $$ = ast_wildcard(&@$); }
  | '(' stmnts ')'                { $$ = $2; }
  ;
label
  : LABEL                         { $$ = ast_label_steal($1, &@$); }
  ;
pre_qual
  : pre_qual '|' pre_qual         { $$ = $1; $$ |= $3; }
  | '(' pre_qual ')'              { $$ = $2; }
  | IDJ                           { $$ = AST_CTX_IDJ; }
  | FALLING                       { $$ = AST_CTX_FALLING; }
  | RISING                        { $$ = AST_CTX_RISING; }
  ;
post_qual
  : post_qual '|' post_qual       { $$ = $1; $$ |= $3; }
  | '(' post_qual ')'             { $$ = $2; }
  | OS                            { $$ = AST_CTX_OS; }
  | OOS                           { $$ = AST_CTX_OOS; }
  | HIT                           { $$ = AST_CTX_HIT; }
  | WHIFF                         { $$ = AST_CTX_WHIFF; }
  ;
%%

static void yyerror(yyscan_t scanner, union ast_node** root, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n");
}
