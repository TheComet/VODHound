%require "3.8"
%code requires
{
    #include <stdint.h>
    #include "search/ast.h"
    
    #define YYLTYPE_IS_DECLARED
    typedef struct YYLTYPE YYLTYPE;
    struct YYLTYPE
    {
        int begin, end;
    };

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

/* This is the union that will become known as YYSTYPE in the generated code */
%union {
    struct strlist_str string_value;
    int integer_value;
    uint64_t motion_value;
    enum ast_ctx_flags ctx_flag_value;
    int node_value;
}

%token '.' '*' '+' '?' '(' ')' '|' '!' '{' '}'
%token INTO
%token GE LE
%token<ctx_flag_value> PRE_CTX POST_CTX
%token<integer_value> TIMING
%token<integer_value> NUM
%token<integer_value> DAMAGE
%token<string_value> LABEL
%token<motion_value> MOTION

%type<node_value> stmts stmt
%type<node_value> timing
%type<node_value> rep rep_short rep_range
%type<node_value> union
%type<node_value> inversion
%type<node_value> dmg_label qual_label label

%left '|'

%start query

%%
query
  : stmts                               { ast_set_root(ast, $1); }
  ;
stmts
  : stmts INTO union                    { $$ = ast_statement(ast, $1, $3, &@$); }
  | union                               { $$ = $1; }
  ;
stmt
  : union                               { $$ = $1; }
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
  : '!' dmg_label                       { $$ = ast_inversion(ast, $2, &@$); }
  | dmg_label                           { $$ = $1; }
  ;
dmg_label
  : dmg_label '>' DAMAGE                { $$ = ast_damage(ast, $1, (float)$3 + 0.1f, 999.f, &@$); }
  | dmg_label '<' DAMAGE                { $$ = ast_damage(ast, $1, 0.f, (float)$3 - 0.1f, &@$); }
  | dmg_label GE DAMAGE                 { $$ = ast_damage(ast, $1, (float)$3, 999.f, &@$); }
  | dmg_label LE DAMAGE                 { $$ = ast_damage(ast, $1, 0.f, (float)$3, &@$); }
  | dmg_label DAMAGE '-' DAMAGE         { $$ = ast_damage(ast, $1, (float)$2, (float)$4, &@$); }
  | qual_label                          { $$ = $1; }
  ;
qual_label
  : PRE_CTX timing POST_CTX             { $$ = ast_context(ast, $2, $1 | $3, &@$); }
  | PRE_CTX timing                      { $$ = ast_context(ast, $2, $1, &@$); }
  | timing POST_CTX                     { $$ = ast_context(ast, $1, $2, &@$); }
  | timing                              { $$ = $1; }
  ;
timing
  : TIMING '-' NUM ',' stmt label       { $$ = ast_timing(ast, $5, $6, $1, $3, &@$); }
  | TIMING '-' NUM label                { $$ = ast_timing(ast, $1, $4, $1, $3, &@$); }
  | TIMING ',' stmt label               { $$ = ast_timing(ast, $3, $4, $1, -1, &@$); }
  | TIMING label                        { $$ = ast_timing(ast, -1, $2, $1, -1, &@$); }
  | label                               { $$ = $1; }
  | '(' stmts ')'                       { $$ = $2; }
  ;
label
  : LABEL                               { $$ = ast_label(ast, $1, &@$); }
  | MOTION                              { $$ = ast_motion(ast, $1, &@$); }
  | '.'                                 { $$ = ast_wildcard(ast, &@$); }
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
