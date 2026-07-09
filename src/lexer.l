%{
#include "ast.h"
#include "parser.tab.h"
#include <iostream>

using namespace std;

int lex_errors = 0;

void lexer_error(const char* msg) {
    cerr << "Error léxico en línea " << yylineno << ": " << msg
         << " en '" << yytext << "'\n";
    lex_errors++;
}
%}

%option noyywrap
%option yylineno

%%

"byte"      { return T_BYTE; }
"char"      { return T_CHAR; }
"int"       { return T_INT; }
"void"      { return T_VOID; }
"if"        { return T_IF; }
"else"      { return T_ELSE; }
"while"     { return T_WHILE; }
"poke"      { return T_POKE; }
"peek"      { return T_PEEK; }
"return"    { return T_RETURN; }

[0-9]+      { yylval.num = atoi(yytext); return T_NUM; }

\'([^\\\n]|\\[nrt'"\\0])\' {
    int value = 0;
    if (yytext[1] == '\\') {
        switch (yytext[2]) {
            case 'n':  value = '\n'; break;
            case 'r':  value = '\r'; break;
            case 't':  value = '\t'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '0':  value = '\0'; break;
            default:   value = yytext[2]; break;
        }
    } else {
        value = static_cast<unsigned char>(yytext[1]);
    }
    yylval.num = value;
    return T_CHARLIT;
}

[a-zA-Z_][a-zA-Z0-9_]* { yylval.id = new string(yytext); return T_ID; }

"=="        { return T_EQ; }
"!="        { return T_NEQ; }
"<="        { return T_LEQ; }
">="        { return T_GEQ; }
"&&"        { return T_AND; }
"||"        { return T_OR; }
"++"        { return T_INC; }
"--"        { return T_DEC; }
"<"         { return '<'; }
">"         { return '>'; }
"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"/"         { return '/'; }
"!"         { return '!'; }
"="         { return '='; }
"("         { return '('; }
")"         { return ')'; }
"{"         { return '{'; }
"}"         { return '}'; }
"["         { return '['; }
"]"         { return ']'; }
";"         { return ';'; }
","         { return ','; }

[ \t\r\n]+  { /* ignorar espacios y saltos de línea */ }
"//".*      { /* ignorar comentarios de una línea */ }

"/*"        {
    /* Comentarios de bloque */
    int c;
    while ((c = yyinput()) != 0) {
        if (c == '\n') yylineno++;
        if (c == '*') {
            int nc = yyinput();
            if (nc == '/') break;
            unput(nc);
        }
    }
}

.           { lexer_error("Caracter no reconocido"); }

%%
