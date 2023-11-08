%{
#define YY_USER_ACTION                          \
    yylloc->begin = yylloc->end;                \
    for(int i = 0; yytext[i] != '\0'; i++) {    \
        yylloc->end++;                          \
    }

#include "search/parser.y.h"
#include "search/ast.h"
#include "vh/str.h"

#define labels ((struct strlist*)yyget_extra(yyg))

%}

%option nodefault
%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations
%option extra-type="struct strlist*"

%%
[\.\(\)\|\?\+\*!\,\{\}]    { return yytext[0]; }
"->"                       { return TOK_INTO; }
"os"                       { yylval->ctx_flag_value = AST_CTX_OS; return TOK_POST_CTX; }
"oos"                      { yylval->ctx_flag_value = AST_CTX_OOS; return TOK_POST_CTX; }
"hit"                      { yylval->ctx_flag_value = AST_CTX_HIT; return TOK_POST_CTX; }
"whiff"                    { yylval->ctx_flag_value = AST_CTX_WHIFF; return TOK_POST_CTX; }
"clank"                    { yylval->ctx_flag_value = AST_CTX_CLANK; return TOK_POST_CTX; }
"trade"                    { yylval->ctx_flag_value = AST_CTX_TRADE; return TOK_POST_CTX; }
"crossup"                  { yylval->ctx_flag_value = AST_CTX_CROSSUP; return TOK_POST_CTX; }
"cross"                    { yylval->ctx_flag_value = AST_CTX_CROSSUP; return TOK_POST_CTX; }
"kill"                     { yylval->ctx_flag_value = AST_CTX_KILL; return TOK_POST_CTX; }
"die"                      { yylval->ctx_flag_value = AST_CTX_DIE; return TOK_POST_CTX; }
"bury"                     { yylval->ctx_flag_value = AST_CTX_BURY; return TOK_POST_CTX; }
"buried"                   { yylval->ctx_flag_value = AST_CTX_BURIED; return TOK_POST_CTX; }
"rising"                   { yylval->ctx_flag_value = AST_CTX_RISING; return TOK_PRE_CTX; }
"ris"                      { yylval->ctx_flag_value = AST_CTX_RISING; return TOK_PRE_CTX; }
"falling"                  { yylval->ctx_flag_value = AST_CTX_FALLING; return TOK_PRE_CTX; }
"fal"                      { yylval->ctx_flag_value = AST_CTX_FALLING; return TOK_PRE_CTX; }
"sh"                       { yylval->ctx_flag_value = AST_CTX_SH; return TOK_PRE_CTX; }
"fh"                       { yylval->ctx_flag_value = AST_CTX_FH; return TOK_PRE_CTX; }
"dj"                       { yylval->ctx_flag_value = AST_CTX_DJ; return TOK_PRE_CTX; }
"fs"                       { yylval->ctx_flag_value = AST_CTX_FS; return TOK_PRE_CTX; }
"idj"                      { yylval->ctx_flag_value = AST_CTX_IDJ; return TOK_PRE_CTX; }
"f"[0-9]+                  { yylval->integer_value = atoi(&yytext[1]); return TOK_TIMING; }
"0x"[0-9a-fA-F]+           { str_hex_to_u64(cstr_view(yytext), &yylval->motion_value); return TOK_MOTION; }
[0-9]+                     { yylval->integer_value = atoi(yytext); return TOK_NUM; }
[a-zA-Z_][a-zA-Z0-9_]*     {
    if (strlist_add(labels, cstr_view(yytext)) < 0)
        yyterminate();
    yylval->string_value = strlist_last(labels);
    return TOK_LABEL; }
[ \t\r\n]
.                          { return yytext[0]; }
%%
