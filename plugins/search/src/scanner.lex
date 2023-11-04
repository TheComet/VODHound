%{
#define YY_USER_ACTION                          \
    yylloc->begin = yylloc->end;                \
    for(int i = 0; yytext[i] != '\0'; i++) {    \
        yylloc->end++;                          \
    }

#include "search/parser.y.h"
#include "vh/mem.h"
#include "vh/str.h"
#include <string.h>
#include <stdlib.h>

#define labels ((struct strlist*)yyget_extra(yyg))

static char* yytext_dup(const char* text);

/*
"fh"                    { return TOK_FH; }
"sh"                    { return TOK_SH; }
"dj"                    { return TOK_DJ; }*/

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
"os"                       { return TOK_OS; }
"oos"                      { return TOK_OOS; }
"hit"                      { return TOK_HIT; }
"whiff"                    { return TOK_WHIFF; }
"clank"                    { return TOK_CLANK; }
"trade"                    { return TOK_TRADE; }
"kill"                     { return TOK_KILL; }
"die"                      { return TOK_DIE; }
"rising"                   { return TOK_RISING; }
"ris"                      { return TOK_RISING; }
"falling"                  { return TOK_FALLING; }
"fal"                      { return TOK_FALLING; }
"sh"                       { return TOK_SH; }
"fh"                       { return TOK_FH; }
"dj"                       { return TOK_DJ; }
"fs"                       { return TOK_FS; }
"idj"                      { return TOK_IDJ; }
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

static char* yytext_dup(const char* text)
{
    int len = (int)strlen(text);
    char* dup = mem_alloc(len + 1);
    if (dup == NULL)
        return NULL;
    strcpy(dup, text);
    return dup;
}
