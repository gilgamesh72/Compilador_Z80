%{
#include "ast.h"
#include "parser.tab.h" // Archivo generado automáticamente por Bison
#include <iostream>

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
"int"       { return T_INT; }
"if"        { return T_IF; }
"else"      { return T_ELSE; }
"while"     { return T_WHILE; }
"poke"      { return T_POKE; }
"peek"      { return T_PEEK; }
"return"    { return T_RETURN; }

[0-9]+      { 
    yylval.num = atoi(yytext); 
    return T_NUM; 
}

[a-zA-Z_][a-zA-Z0-9_]* { 
    yylval.id = new string(yytext); 
    return T_ID; 
}

"=="        { return T_EQ; }
"&&"        { return T_AND; }
"||"        { return T_OR; }
"<"         { return '<'; }
">"         { return '>'; }
"+"         { return '+'; }
"-"         { return '-'; }
"="         { return '='; }
"("         { return '('; }
")"         { return ')'; }
"{"         { return '{'; }
"}"         { return '}'; }
";"         { return ';'; }
","         { return ','; }

[ \t\r\n]+  { /* Ignorar espacios en blanco, tabulaciones y saltos de línea */ }
"//".*      { /* Ignorar comentarios de una línea */ }

.           { lexer_error("Caracter no reconocido"); }

%%
