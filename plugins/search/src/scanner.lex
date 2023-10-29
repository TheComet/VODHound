%{
#define YY_USER_ACTION                          \
    yylloc->begin = yylloc->end;                \
    for(int i = 0; yytext[i] != '\0'; i++) {    \
        yylloc->end++;                          \
    }

#include "search/parser.y.h"
#include "vh/mem.h"
#include <string.h>
#include <stdlib.h>

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

%%
[\.\(\)\|\?\+\*!\,]        { return yytext[0]; }
"->"                       { return TOK_INTO; }
"os"                       { return TOK_OS; }
"oos"                      { return TOK_OOS; }
"hit"                      { return TOK_HIT; }
"whiff"                    { return TOK_WHIFF; }
"rising"                   { return TOK_RISING; }
"falling"                  { return TOK_FALLING; }
"idj"                      { return TOK_IDJ; }
"0x"[0-9a-fA-F]+           { if (!(yylval->string_value = yytext_dup(yytext))) yyterminate(); return TOK_LABEL; }
[0-9]+                     { yylval->integer_value = atoi(yytext); return TOK_NUM; }
[a-zA-Z_][a-zA-Z0-9_]*     { if (!(yylval->string_value = yytext_dup(yytext))) yyterminate(); return TOK_LABEL; }
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
