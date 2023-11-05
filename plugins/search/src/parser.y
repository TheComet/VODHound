%require "3.8"
%code requires
{
    #include <stdint.h>
    #include "search/ast.h"

    typedef struct YYLTYPE YYLTYPE;
    struct YYLTYPE
    {
        int begin, end;
    };
    #define YYLTYPE_IS_DECLARED

    typedef void* yyscan_t;
}
%code top
{
    #include "search/parser.y.h"
    #include "search/scanner.lex.h"
    #include "search/ast.h"
    #include "search/ast_ops.h"
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

    static void yyerror(yyscan_t scanner, struct ast* ast, const char* msg, ...);
}

%define api.pure full
%define api.push-pull push
%define api.token.prefix {TOK_}
%define api.header.include {"search/parser.y.h"}
%define parse.error verbose
%locations

%parse-param {struct ast* ast}

/* This is the union that will become known as QPSTYPE in the generated code */
%union {
    struct strlist_str string_value;
    int integer_value;
    uint64_t motion_value;
    enum ast_ctx_flags ctx_flags;
    int node_value;
}

%token '.' '*' '+' '?' '(' ')' '|' '!'
%token INTO
%token OS
%token OOS
%token HIT
%token WHIFF
%token CLANK
%token TRADE
%token KILL
%token DIE
%token BURY
%token BURIED
%token RISING
%token FALLING
%token SH
%token FH
%token DJ
%token FS
%token IDJ
%token<integer_value> TIMING
%token<integer_value> NUM
%token<integer_value> PERCENT
%token<string_value> LABEL
%token<motion_value> MOTION

%type<node_value> stmts stmt timing_stmt rep rep_short rep_range union inversion label
%type<ctx_flags> pre_ctx_flags post_ctx_flags

%right '|'

%start query

%%
query
  : stmts                               { ast_set_root(ast, $1); }
  ;
stmts
  : stmts INTO stmt                     { $$ = ast_statement(ast, $1, $3, &@$); }
  | stmt                                { $$ = $1; }
  ;
stmt
  : pre_ctx_flags timing_stmt post_ctx_flags { $$ = ast_context_qualifier(ast, $2, $1 | $3, &@$); }
  | timing_stmt post_ctx_flags          { $$ = ast_context_qualifier(ast, $1, $2, &@$); }
  | pre_ctx_flags timing_stmt           { $$ = ast_context_qualifier(ast, $2, $1, &@$); }
  | timing_stmt                         { $$ = $1; }
  ;
timing_stmt
  : TIMING '-' NUM ',' stmt union       { $$ = ast_timing(ast, $6, $5, $1, $3, &@$); }
  | TIMING '-' NUM union                { $$ = ast_timing(ast, $4, -1, $1, $3, &@$); }
  | TIMING ',' stmt union               { $$ = ast_timing(ast, $4, $3, $1, -1, &@$); }
  | TIMING union                        { $$ = ast_timing(ast, $2, -1, $1, -1, &@$); }
  | union                               { $$ = $1; }
  ;
union
  : union '|' union                     { $$ = ast_union(ast, $1, $3, &@$); }
  | rep                                 { $$ = $1; }
  ;
rep
  : rep_short                           { $$ = $1; }
  | rep_range                           { $$ = $1; }
  | inversion                           { $$ = $1; }
  ;
rep_short
  : inversion '+'                       { $$ = ast_repetition(ast, $1, 1, -1, &@$); }
  | inversion '*'                       { $$ = ast_repetition(ast, $1, 0, -1, &@$); }
  | inversion '?'                       { $$ = ast_repetition(ast, $1, 0, 1, &@$); }
  ;
rep_range
  : inversion NUM                       { $$ = ast_repetition(ast, $1, $2, $2, &@$); }
  | inversion NUM '-' NUM               { $$ = ast_repetition(ast, $1, $2, $4, &@$); }
  | inversion NUM '-'                   { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  | inversion NUM '-' '+'               { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  | inversion NUM '-' '*'               { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  | inversion NUM ',' NUM               { $$ = ast_repetition(ast, $1, $2, $4, &@$); }
  | inversion NUM ','                   { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  | inversion NUM ',' '+'               { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  | inversion NUM ',' '*'               { $$ = ast_repetition(ast, $1, $2, -1, &@$); }
  /* Be compatible with more traditional regex syntax */
  | inversion '{' NUM '}'               { $$ = ast_repetition(ast, $1, $3, $3, &@$); }
  | inversion '{' NUM '-' NUM '}'       { $$ = ast_repetition(ast, $1, $3, $5, &@$); }
  | inversion '{' NUM '-' '}'           { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  | inversion '{' NUM '-' '+' '}'       { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  | inversion '{' NUM '-' '*' '}'       { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  | inversion '{' NUM ',' NUM '}'       { $$ = ast_repetition(ast, $1, $3, $5, &@$); }
  | inversion '{' NUM ',' '}'           { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  | inversion '{' NUM ',' '+' '}'       { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  | inversion '{' NUM ',' '*' '}'       { $$ = ast_repetition(ast, $1, $3, -1, &@$); }
  ;
inversion
  : '!' label                           { $$ = ast_inversion(ast, $2, &@$); }
  | label                               { $$ = $1; }
  | '.'                                 { $$ = ast_wildcard(ast, &@$); }
  | '(' stmts ')'                       { $$ = $2; }
  ;
label
  : LABEL                               { $$ = ast_label(ast, $1, &@$); }
  | MOTION                              { $$ = ast_motion(ast, $1, &@$); }
  ;
pre_ctx_flags
  : pre_ctx_flags '|' pre_ctx_flags     { $$ = $1; $$ |= $3; }
/*  | '(' pre_qual ')'                    { $$ = $2; }*/
  | SH                                  { $$ = AST_CTX_SH; }
  | FH                                  { $$ = AST_CTX_FH; }
  | DJ                                  { $$ = AST_CTX_DJ; }
  | FS                                  { $$ = AST_CTX_FS; }
  | IDJ                                 { $$ = AST_CTX_IDJ; }
  | FALLING                             { $$ = AST_CTX_FALLING; }
  | RISING                              { $$ = AST_CTX_RISING; }
  ;
post_ctx_flags
  : post_ctx_flags '|' post_ctx_flags   { $$ = $1; $$ |= $3; }
/*  | '(' post_qual ')'                 { $$ = $2; }*/
  | OS                                  { $$ = AST_CTX_OS; }
  | OOS                                 { $$ = AST_CTX_OOS; }
  | HIT                                 { $$ = AST_CTX_HIT; }
  | WHIFF                               { $$ = AST_CTX_WHIFF; }
  | CLANK                               { $$ = AST_CTX_CLANK; }
  | TRADE                               { $$ = AST_CTX_TRADE; }
  | KILL                                { $$ = AST_CTX_KILL; }
  | DIE                                 { $$ = AST_CTX_DIE; }
  ;
%%

static void yyerror(yyscan_t scanner, struct ast* ast, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n");
}
