%require "3.8"
%code top
{
    struct ast;

    #include "search/parser.y.h"
    #include "search/scanner.lex.h"
    #include "search/ast.h"

    static void qperror(qpscan_t scanner, const char* msg, ...);
}

%code requires
{
    /* Required if the prefix is changed
    #define YYSTYPE QPSTYPE
    #define YYLTYPE QPLTYPE

    typedef void* qpscan_t;
    typedef struct qppstate qppstate;*/
}

/*
 * This is the bison equivalent of Flex's %option reentrant, in the sense that it also makes formerly global
 * variables into local ones. Unlike the lexer, there is no state structure for Bison. All the formerly global
 * variables become local to the yyparse() method. Which really begs the question: why were they ever global?
 * Although it is similar in nature to Flex's %option reentrant, this is truly the counterpart of
 * Flex's %option bison-bridge. Adding this declaration is what causes Bison to invoke yylval(YYSTYPE*) instead
 * of yylval(void), which is the same change that %option bison-bridge does in Flex.
 */
%define api.pure full

/*
 * As far as the grammar file goes, this is the only change needed to tell Bison to switch to a push-parser
 * interface instead of a pull-parser interface. Bison has the capability to generate both, but that is a
 * far more advanced case not covered here.
 */
%define api.push-pull push

//%define api.prefix {qp}

/* Tell bison where and how it should include the generated header file */
%define api.header.include {"search/parser.y.h"}

/*
 * These two options are related to Flex's %option reentrant, These options add an argument to
 * each of the yylex() call and the yyparse() method, respectively.
 * Since the %option reentrant in Flex added an argument to make the yylex(void) method into yylex(yyscan_t),
 * Bison must be told to pass that new argument when it invokes the lexer. This is what the %lex-param declaration does.
 * How Bison obtains an instance of yyscan_t is up to you, but the most sensible way is to pass it into the
 * yyparse(void) method, making the  new signature yyparse(yyscan_t). This is what the %parse-param does.
 */
%parse-param {struct ast** root}

%define api.token.prefix {TOK_}

/* This is the union that will become known as QPSTYPE in the generated code */
%union {
    char* string_value;
    int integer_value;
    uint8_t ctx_flags;
    struct ast* node_value;
}

%destructor { mem_free($$); } <string_value>
%destructor { ast_destroy_single($$); } <node_value>

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
  : stmnts                        { *ast = $1; }
  ;
stmnts
  : stmnts INTO stmnt             { $$ = ast_statement($1, $3); }
  | stmnt                         { $$ = $1; }
  ;
stmnt
  : pre_qual union post_qual      { $$ = ast_context_qualifier($2, $1 | $3); }
  | union post_qual               { $$ = ast_context_qualifier($1, $2); }
  | pre_qual union                { $$ = ast_context_qualifier($2, $1); }
  | union                         { $$ = $1; }
  ;
union
  : union '|' union               { $$ = QueryASTNode::newUnion($1, $3); }
  | repitition                    { $$ = $1; }
  ;
repitition
  : inversion '+'                 { $$ = ast_repititiona($1, 1, -1); }
  | inversion '*'                 { $$ = ast_repititiona($1, 0, -1); }
  | inversion '?'                 { $$ = ast_repititiona($1, 0, 1); }
  | inversion NUM                 { $$ = ast_repititiona($1, $2, $2); }
  | inversion NUM ',' NUM         { $$ = ast_repititiona($1, $2, $4); }
  | inversion NUM ',' '+'         { $$ = ast_repititiona($1, $2, -1); }
  | inversion NUM ',' '*'         { $$ = ast_repititiona($1, $2, -1); }
  | inversion                     { $$ = $1; }
  ;
inversion
  : '!' label                     { $$ = ast_inversion($2); }
  | label                         { $$ = $1; }
  | '.'                           { $$ = ast_wildcard(); }
  | '(' stmnts ')'                { $$ = $2; }
  ;
label
  : LABEL                         { $$ = ast_label_steal($1); }
  ;
pre_qual
  : pre_qual '|' pre_qual         { $$ = $1; $$ |= $3; }
  | '(' pre_qual ')'              { $$ = $2; }
  | IDJ                           { $$ = AST_IDJ; }
  | FALLING                       { $$ = AST_FALLING; }
  | RISING                        { $$ = AST_RISING; }
  ;
post_qual
  : post_qual '|' post_qual       { $$ = $1; $$ |= $3; }
  | '(' post_qual ')'             { $$ = $2; }
  | OS                            { $$ = AST_OS; }
  | OOS                           { $$ = AST_OOS; }
  | HIT                           { $$ = AST_HIT; }
  | WHIFF                         { $$ = AST_WHIFF; }
  ;
%%

static void qperror(qpscan_t scanner, const char* msg, ...)
{
}