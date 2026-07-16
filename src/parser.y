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

/* ============================================================
   Unión de valores semánticos
   ============================================================ */
%union {
    int        num;
    string*    id;
    NodoAST*   node;
    Expresion* expr;
    Sentencia* stmt;
    Bloque*    block;
    int        typeVal;               // Almacena DataType (enum backed by int)
    vector<Parametro*>*  params;      // Lista de parámetros de función
    vector<Expresion*>*  args;        // Lista de argumentos de llamada
}

/* ============================================================
   Declaración de tokens
   ============================================================ */
%token T_BYTE T_CHAR T_INT T_VOID
%token T_IF T_ELSE T_WHILE T_FOR
%token T_POKE T_PEEK T_RETURN
%token T_IN T_OUT

%token <num>  T_NUM
%token <num>  T_CHARLIT
%token <id>   T_ID
%token <id>   T_STRLIT

%token T_EQ T_NEQ T_LE T_GE
%token T_AND T_OR
%token T_LSHIFT T_RSHIFT

/* ============================================================
   Tipos no-terminales
   ============================================================ */
%type <expr>    expression
%type <stmt>    statement for_init for_step
%type <expr>    for_cond
%type <block>   block program statement_list
%type <typeVal> type
%type <params>  param_list param_list_ne
%type <args>    arg_list arg_list_ne

/* ============================================================
   Precedencia y asociatividad (de MENOR a MAYOR prioridad)
   ============================================================ */
%right '='
%left  T_OR
%left  T_AND
%left  '|'
%left  '^'
%left  '&'
%left  T_EQ T_NEQ
%left  '<' '>' T_LE T_GE
%left  T_LSHIFT T_RSHIFT
%left  '+' '-'
%left  '*' '/' '%'
%right '!' '~' UMINUS

%%

/* ============================================================
   Reglas gramaticales
   ============================================================ */

program:
    statement_list { program_root = $1; }
    ;

statement_list:
    statement {
        $$ = new Bloque();
        if ($1) $$->agregarSentencia($1);
    }
    | statement_list statement {
        if ($2) $1->agregarSentencia($2);
        $$ = $1;
    }
    ;

block:
    '{' statement_list '}' { $$ = $2; }
    | '{' '}'              { $$ = new Bloque(); }
    ;

/* ----------------------------------------------------------
   No-terminal unificado de tipo
   ---------------------------------------------------------- */
type:
    T_BYTE { $$ = (int)DataType::BYTE; }
    | T_CHAR { $$ = (int)DataType::CHAR; }
    | T_INT  { $$ = (int)DataType::INT; }
    | T_VOID { $$ = (int)DataType::VOID; }
    ;

/* ----------------------------------------------------------
   Listas de parámetros de función
   ---------------------------------------------------------- */
param_list:
    /* vacío */      { $$ = new vector<Parametro*>(); }
    | param_list_ne  { $$ = $1; }
    ;

param_list_ne:
    type T_ID {
        $$ = new vector<Parametro*>();
        $$->push_back(new Parametro((DataType)$1, *$2));
        delete $2;
    }
    | param_list_ne ',' type T_ID {
        $1->push_back(new Parametro((DataType)$3, *$4));
        delete $4;
        $$ = $1;
    }
    ;

/* ----------------------------------------------------------
   Listas de argumentos de llamada
   ---------------------------------------------------------- */
arg_list:
    /* vacío */     { $$ = new vector<Expresion*>(); }
    | arg_list_ne   { $$ = $1; }
    ;

arg_list_ne:
    expression {
        $$ = new vector<Expresion*>();
        $$->push_back($1);
    }
    | arg_list_ne ',' expression {
        $1->push_back($3);
        $$ = $1;
    }
    ;

/* ----------------------------------------------------------
   Sentencias
   ---------------------------------------------------------- */
statement:
    /* Declaración variable simple: tipo nombre; */
    type T_ID ';' {
        $$ = new DeclaracionVariable((DataType)$1, *$2);
        delete $2;
    }

    /* Declaración de arreglo:  tipo nombre[tamaño]; */
    | type T_ID '[' T_NUM ']' ';' {
        $$ = new DeclaracionArreglo((DataType)$1, *$2, $4);
        delete $2;
    }

    /* Definición de función:  tipo nombre(params) { body } */
    | type T_ID '(' param_list ')' block {
        $$ = new DeclaracionFuncion((DataType)$1, *$2, $4, $6);
        delete $2;
    }

    /* Asignación simple:  x = expr; */
    | T_ID '=' expression ';' {
        $$ = new Asignacion(*$1, $3);
        delete $1;
    }

    /* Asignación a arreglo:  arr[idx] = expr; */
    | T_ID '[' expression ']' '=' expression ';' {
        $$ = new AsignacionArreglo(*$1, $3, $6);
        delete $1;
    }

    /* if / else */
    | T_IF '(' expression ')' block {
        $$ = new SentenciaSi($3, $5, nullptr);
    }
    | T_IF '(' expression ')' block T_ELSE block {
        $$ = new SentenciaSi($3, $5, $7);
    }

    /* while */
    | T_WHILE '(' expression ')' block {
        $$ = new SentenciaMientras($3, $5);
    }

    /* for (init; cond; step) { body } */
    | T_FOR '(' for_init for_cond ';' for_step ')' block {
        $$ = new SentenciaPara($3, $4, $6, $8);
    }

    /* poke(addr, val); */
    | T_POKE '(' expression ',' expression ')' ';' {
        $$ = new SentenciaPoke($3, $5);
    }

    /* out(port, val); */
    | T_OUT '(' expression ',' expression ')' ';' {
        $$ = new SentenciaOut($3, $5);
    }

    /* return expr; / return; */
    | T_RETURN expression ';' {
        $$ = new SentenciaRetorno($2);
    }
    | T_RETURN ';' {
        $$ = new SentenciaRetorno(nullptr);
    }

    /* Llamada a función como sentencia:  foo(args); */
    | T_ID '(' arg_list ')' ';' {
        $$ = new SentenciaLlamadaFuncion(*$1, $3);
        delete $1;
    }
    ;

/* ----------------------------------------------------------
   Partes del for
   ---------------------------------------------------------- */

/* Inicialización del for: puede ser vacía, una asignación o una decl. */
for_init:
    ';'                        { $$ = nullptr; }
    | type T_ID '=' expression ';' {
        $$ = new Asignacion(*$2, $4);
        delete $2;
    }
    | T_ID '=' expression ';' {
        $$ = new Asignacion(*$1, $3);
        delete $1;
    }
    ;

/* Condición del for: puede ser vacía (bucle infinito) o una expresión */
for_cond:
    /* vacío */   { $$ = nullptr; }
    | expression  { $$ = $1; }
    ;

/* Paso del for: puede ser vacío o una asignación */
for_step:
    /* vacío */              { $$ = nullptr; }
    | T_ID '=' expression   {
        $$ = new Asignacion(*$1, $3);
        delete $1;
    }
    | T_ID '[' expression ']' '=' expression {
        $$ = new AsignacionArreglo(*$1, $3, $6);
        delete $1;
    }
    ;

/* ----------------------------------------------------------
   Expresiones
   ---------------------------------------------------------- */
expression:
    /* Literales */
    T_NUM                   { $$ = new ExprNumero($1); }
    | T_CHARLIT             { $$ = new ExprCaracter($1); }
    | T_STRLIT              { $$ = new ExprString(*$1); delete $1; }

    /* Variable */
    | T_ID                  { $$ = new ExprIdentificador(*$1); delete $1; }

    /* Acceso a arreglo:  arr[idx] */
    | T_ID '[' expression ']' {
        $$ = new ExprAccesoArreglo(*$1, $3);
        delete $1;
    }

    /* Llamada a función como expresión:  foo(args) */
    | T_ID '(' arg_list ')' {
        $$ = new ExprLlamadaFuncion(*$1, $3);
        delete $1;
    }

    /* peek(addr) */
    | T_PEEK '(' expression ')' {
        $$ = new ExprLeerMemoria($3);
    }

    /* in(port) */
    | T_IN '(' expression ')' {
        $$ = new ExprIn($3);
    }

    /* Operaciones aritméticas */
    | expression '+' expression  { $$ = new ExprBinaria("+",  $1, $3); }
    | expression '-' expression  { $$ = new ExprBinaria("-",  $1, $3); }
    | expression '*' expression  { $$ = new ExprBinaria("*",  $1, $3); }
    | expression '/' expression  { $$ = new ExprBinaria("/",  $1, $3); }
    | expression '%' expression  { $$ = new ExprBinaria("%",  $1, $3); }

    /* Comparaciones */
    | expression '<'    expression { $$ = new ExprBinaria("<",  $1, $3); }
    | expression '>'    expression { $$ = new ExprBinaria(">",  $1, $3); }
    | expression T_LE   expression { $$ = new ExprBinaria("<=", $1, $3); }
    | expression T_GE   expression { $$ = new ExprBinaria(">=", $1, $3); }
    | expression T_EQ   expression { $$ = new ExprBinaria("==", $1, $3); }
    | expression T_NEQ  expression { $$ = new ExprBinaria("!=", $1, $3); }

    /* Lógicas */
    | expression T_AND  expression { $$ = new ExprBinaria("&&", $1, $3); }
    | expression T_OR   expression { $$ = new ExprBinaria("||", $1, $3); }

    /* Bitwise */
    | expression '&'    expression { $$ = new ExprBinaria("&",  $1, $3); }
    | expression '|'    expression { $$ = new ExprBinaria("|",  $1, $3); }
    | expression '^'    expression { $$ = new ExprBinaria("^",  $1, $3); }
    | expression T_LSHIFT expression { $$ = new ExprBinaria("<<", $1, $3); }
    | expression T_RSHIFT expression { $$ = new ExprBinaria(">>", $1, $3); }

    /* Unarias */
    | '!' expression        { $$ = new ExprUnaria("!", $2); }
    | '~' expression        { $$ = new ExprUnaria("~", $2); }
    | '-' expression %prec UMINUS { $$ = new ExprUnaria("-", $2); }

    /* Agrupación */
    | '(' expression ')'   { $$ = $2; }
    ;

%%
