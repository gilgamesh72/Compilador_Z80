%{
#include "ast.h"
#include "parser.tab.h" // Archivo generado automáticamente por Bison
#include <iostream>
#include <cstring>

using namespace std;

int lex_errors = 0;

// Función para manejar errores simples en Flex
void lexer_error(const char* msg) {
    cerr << "Error léxico: " << msg << " en '" << yytext << "'" << endl;
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
"for"       { return T_FOR; }
"poke"      { return T_POKE; }
"peek"      { return T_PEEK; }
"return"    { return T_RETURN; }
"in"        { return T_IN; }
"out"       { return T_OUT; }

0[xX][0-9a-fA-F]+ {
    yylval.num = (int)strtol(yytext, nullptr, 16);
    return T_NUM;
}

[0-9]+      { 
    yylval.num = atoi(yytext); 
    return T_NUM; 
}

\'([^\\\n]|\\[nrt'\"\\0])\' {
    int value = 0;
    if (yytext[1] == '\\') {
        switch (yytext[2]) {
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '0': value = '\0'; break;
            default: value = yytext[2]; break;
        }
    } else {
        value = static_cast<unsigned char>(yytext[1]);
    }
    yylval.num = value;
    return T_CHARLIT;
}

\"([^\"\n\\]|\\.)*\" {
    // Cadena de texto: copiar el contenido sin comillas
    int len = yyleng - 2;             // Sin las comillas
    char* buf = new char[len + 1];
    strncpy(buf, yytext + 1, len);
    buf[len] = '\0';
    yylval.id = new string(buf);      // Reutilizamos el campo 'id' del union
    delete[] buf;
    return T_STRLIT;
}

[a-zA-Z_][a-zA-Z0-9_]* { 
    yylval.id = new string(yytext); 
    return T_ID; 
}

"<<"        { return T_LSHIFT; }
">>"        { return T_RSHIFT; }
"=="        { return T_EQ; }
"!="        { return T_NEQ; }
"<="        { return T_LE; }
">="        { return T_GE; }
"&&"        { return T_AND; }
"||"        { return T_OR; }

"<"         { return '<'; }
">"         { return '>'; }
"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"/"         { return '/'; }
"%"         { return '%'; }
"|"         { return '|'; }
"^"         { return '^'; }
"~"         { return '~'; }
"&"         { return '&'; }
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

[ \t\r\n]+  { /* Ignorar espacios en blanco, tabulaciones y saltos de línea */ }
"//".*      { /* Ignorar comentarios de una línea */ }

.           { lexer_error("Caracter no reconocido"); }

%%
