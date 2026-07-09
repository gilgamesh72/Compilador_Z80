%{
#include "ast.h"
#include <iostream>
#include <vector>

using namespace std;

extern int yylex();
extern int yylineno;
extern char* yytext;

// Contador global de errores sintácticos (permite recuperación)
int parse_errors = 0;

void yyerror(const char* s) {
    cerr << "Error sintáctico en línea " << yylineno << ": " << s << "\n";
    parse_errors++;
}

// Raíz del AST
Bloque* program_root;
%}

%union {
    int num;
    string* id;
    NodoAST* node;
    Expresion* expr;
    Sentencia* stmt;
    Bloque* block;
    DataType dtype;
    std::vector<Expresion*>* exprList;
    std::vector<Parametro>* paramList;
}

/* --- Tokens --- */
%token T_BYTE T_CHAR T_INT T_VOID
%token T_IF T_ELSE T_WHILE T_POKE T_PEEK T_RETURN
%token T_EQ T_NEQ T_LEQ T_GEQ T_AND T_OR T_INC T_DEC
%token <num> T_NUM
%token <num> T_CHARLIT
%token <id>  T_ID

/* --- Tipos de no-terminales --- */
%type <expr>      expression
%type <stmt>      statement
%type <block>     block program statement_list
%type <dtype>     tipo
%type <exprList>  arg_list arg_list_ne
%type <paramList> param_list param_list_ne

/* --- Precedencia (de menor a mayor) --- */
%left T_OR
%left T_AND
%left T_EQ T_NEQ
%left '<' '>' T_LEQ T_GEQ
%left '+' '-'
%left '*' '/'
%right UMINUS '!'

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

/* Regla unificada de tipo */
tipo:
    T_BYTE  { $$ = DataType::BYTE; }
    | T_CHAR  { $$ = DataType::CHAR; }
    | T_INT   { $$ = DataType::INT;  }
    | T_VOID  { $$ = DataType::VOID; }
    ;

/* Lista de parámetros de función */
param_list:
    /* vacío */ { $$ = new std::vector<Parametro>(); }
    | param_list_ne { $$ = $1; }
    ;

param_list_ne:
    tipo T_ID {
        $$ = new std::vector<Parametro>();
        $$->push_back({$1, *$2, 0});
        delete $2;
    }
    | param_list_ne ',' tipo T_ID {
        $1->push_back({$3, *$4, 0});
        delete $4;
        $$ = $1;
    }
    ;

/* Lista de argumentos de llamada a función */
arg_list:
    /* vacío */   { $$ = new std::vector<Expresion*>(); }
    | arg_list_ne { $$ = $1; }
    ;

arg_list_ne:
    expression {
        $$ = new std::vector<Expresion*>();
        $$->push_back($1);
    }
    | arg_list_ne ',' expression {
        $1->push_back($3);
        $$ = $1;
    }
    ;

statement:
    /* Declaración de variable */
    tipo T_ID ';' {
        $$ = new DeclaracionVariable($1, *$2);
        delete $2;
    }
    /* Declaración de arreglo */
    | tipo T_ID '[' T_NUM ']' ';' {
        $$ = new DeclaracionArreglo($1, *$2, $4);
        delete $2;
    }
    /* Definición de función */
    | tipo T_ID '(' param_list ')' block {
        $$ = new DefinicionFuncion($1, *$2, *$4, $6);
        delete $2;
        delete $4;
    }
    /* Asignación simple */
    | T_ID '=' expression ';' {
        $$ = new Asignacion(*$1, $3);
        delete $1;
    }
    /* Asignación a arreglo */
    | T_ID '[' expression ']' '=' expression ';' {
        $$ = new AsignacionArreglo(*$1, $3, $6);
        delete $1;
    }
    /* Incremento  x++ */
    | T_ID T_INC ';' {
        auto lhs = new ExprIdentificador(*$1);
        $$ = new Asignacion(*$1, new ExprBinaria('+', lhs, new ExprNumero(1)));
        delete $1;
    }
    /* Decremento x-- */
    | T_ID T_DEC ';' {
        auto lhs = new ExprIdentificador(*$1);
        $$ = new Asignacion(*$1, new ExprBinaria('-', lhs, new ExprNumero(1)));
        delete $1;
    }
    /* If con y sin else */
    | T_IF '(' expression ')' block {
        $$ = new SentenciaSi($3, $5, nullptr);
    }
    | T_IF '(' expression ')' block T_ELSE block {
        $$ = new SentenciaSi($3, $5, $7);
    }
    /* While */
    | T_WHILE '(' expression ')' block {
        $$ = new SentenciaMientras($3, $5);
    }
    /* Poke (escritura directa en memoria) */
    | T_POKE '(' expression ',' expression ')' ';' {
        $$ = new SentenciaPoke($3, $5);
    }
    /* Return con y sin valor */
    | T_RETURN expression ';' {
        $$ = new SentenciaRetorno($2);
    }
    | T_RETURN ';' {
        $$ = new SentenciaRetorno(nullptr);
    }
    /* Llamada a función como sentencia (resultado descartado) */
    | T_ID '(' arg_list ')' ';' {
        auto args = *$3; delete $3;
        $$ = new SentenciaExpresion(new ExprLlamadaFuncion(*$1, args));
        delete $1;
    }
    /* Recuperación de errores: salta hasta el siguiente ';' */
    | error ';' {
        yyerrok;
        $$ = new SentenciaNula();
    }
    ;

expression:
    T_NUM     { $$ = new ExprNumero($1); }
    | T_CHARLIT { $$ = new ExprCaracter($1); }
    | T_ID    { $$ = new ExprIdentificador(*$1); delete $1; }
    /* Llamada a función en expresión */
    | T_ID '(' arg_list ')' {
        auto args = *$3; delete $3;
        $$ = new ExprLlamadaFuncion(*$1, args);
        delete $1;
    }
    /* Acceso a arreglo */
    | T_ID '[' expression ']' {
        $$ = new ExprAccesoArreglo(*$1, $3);
        delete $1;
    }
    /* Peek (lectura de memoria) */
    | T_PEEK '(' expression ')' { $$ = new ExprLeerMemoria($3); }
    /* Operadores unarios */
    | '!' expression             { $$ = new ExprUnaria('!', $2); }
    | '-' expression %prec UMINUS { $$ = new ExprUnaria('-', $2); }
    /* Operadores binarios aritméticos */
    | expression '+' expression  { $$ = new ExprBinaria('+', $1, $3); }
    | expression '-' expression  { $$ = new ExprBinaria('-', $1, $3); }
    | expression '*' expression  { $$ = new ExprBinaria('*', $1, $3); }
    | expression '/' expression  { $$ = new ExprBinaria('/', $1, $3); }
    /* Operadores relacionales */
    | expression '<'    expression { $$ = new ExprBinaria('<', $1, $3); }
    | expression '>'    expression { $$ = new ExprBinaria('>', $1, $3); }
    | expression T_EQ   expression { $$ = new ExprBinaria('E', $1, $3); }
    | expression T_NEQ  expression { $$ = new ExprBinaria('N', $1, $3); }
    | expression T_LEQ  expression { $$ = new ExprBinaria('L', $1, $3); }
    | expression T_GEQ  expression { $$ = new ExprBinaria('G', $1, $3); }
    /* Operadores lógicos */
    | expression T_AND  expression { $$ = new ExprBinaria('A', $1, $3); }
    | expression T_OR   expression { $$ = new ExprBinaria('O', $1, $3); }
    /* Agrupación */
    | '(' expression ')'           { $$ = $2; }
    ;

%%
