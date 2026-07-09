%{
#include "ast.h"
#include <iostream>

using namespace std;

extern int yylex();
extern int yylineno;
extern char* yytext;

void yyerror(const char* s) {
    cerr << "Error sintáctico en la línea " << yylineno << ": " << s << endl;
}

// Variable global para almacenar la raíz de nuestro AST
Bloque* program_root;
%}

%union {
    int num;
    string* id;
    NodoAST* node;
    Expresion* expr;
    Sentencia* stmt;
    Bloque* block;
}

%token T_BYTE T_CHAR T_INT T_IF T_ELSE T_WHILE T_POKE T_PEEK T_RETURN
%token <num> T_NUM
%token <num> T_CHARLIT
%token <id> T_ID
%token T_EQ T_AND T_OR

%type <expr> expression
%type <stmt> statement
%type <block> block program statement_list

// Precedencia de operadores para resolver ambigüedades (de menor a mayor precedencia)
%left T_OR
%left T_AND
%left T_EQ '<' '>'
%left '+' '-'

%%

program:
    statement_list { program_root = $1; }
    ;

statement_list:
    statement { 
        $$ = new Bloque(); 
        $$->agregarSentencia($1); 
    }
    | statement_list statement { 
        $1->agregarSentencia($2); 
        $$ = $1; 
    }
    ;

block:
    '{' statement_list '}' { $$ = $2; }
    | '{' '}' { $$ = new Bloque(); }
    ;

statement:
    T_BYTE T_ID ';' { 
        $$ = new DeclaracionVariable(DataType::BYTE, *$2); 
        delete $2; 
    }
    | T_CHAR T_ID ';' { 
        $$ = new DeclaracionVariable(DataType::CHAR, *$2); 
        delete $2; 
    }
    | T_INT T_ID ';' { 
        $$ = new DeclaracionVariable(DataType::INT, *$2); 
        delete $2; 
    }
    | T_ID '=' expression ';' { 
        $$ = new Asignacion(*$1, $3); 
        delete $1; 
    }
    | T_IF '(' expression ')' block { 
        $$ = new SentenciaSi($3, $5, nullptr); 
    }
    | T_IF '(' expression ')' block T_ELSE block { 
        $$ = new SentenciaSi($3, $5, $7); 
    }
    | T_WHILE '(' expression ')' block { 
        $$ = new SentenciaMientras($3, $5); 
    }
    | T_POKE '(' expression ',' expression ')' ';' { 
        $$ = new SentenciaPoke($3, $5); 
    }
    | T_RETURN expression ';' { 
        $$ = new SentenciaRetorno($2); 
    }
    | T_RETURN ';' { 
        $$ = new SentenciaRetorno(nullptr); 
    }
    ;

expression:
    T_NUM { 
        $$ = new ExprNumero($1); 
    }
    | T_CHARLIT {
        $$ = new ExprCaracter($1);
    }
    | T_ID { 
        $$ = new ExprIdentificador(*$1); 
        delete $1; 
    }
    | T_PEEK '(' expression ')' { 
        $$ = new ExprLeerMemoria($3); 
    }
    | expression '+' expression { $$ = new ExprBinaria('+', $1, $3); }
    | expression '-' expression { $$ = new ExprBinaria('-', $1, $3); }
    | expression '<' expression { $$ = new ExprBinaria('<', $1, $3); }
    | expression '>' expression { $$ = new ExprBinaria('>', $1, $3); }
    | expression T_EQ expression { $$ = new ExprBinaria('E', $1, $3); /* 'E' por == */ }
    | expression T_AND expression { $$ = new ExprBinaria('A', $1, $3); /* 'A' por && */ }
    | expression T_OR expression { $$ = new ExprBinaria('O', $1, $3); /* 'O' por || */ }
    | '(' expression ')' { $$ = $2; }
    ;

%%
