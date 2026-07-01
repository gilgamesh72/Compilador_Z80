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
Block* program_root;
%}

%union {
    int num;
    string* id;
    ASTNode* node;
    Expr* expr;
    Stmt* stmt;
    Block* block;
}

%token T_BYTE T_INT T_IF T_ELSE T_WHILE T_POKE T_PEEK T_RETURN
%token <num> T_NUM
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
        $$ = new Block(); 
        $$->addStatement($1); 
    }
    | statement_list statement { 
        $1->addStatement($2); 
        $$ = $1; 
    }
    ;

block:
    '{' statement_list '}' { $$ = $2; }
    | '{' '}' { $$ = new Block(); }
    ;

statement:
    T_BYTE T_ID ';' { 
        $$ = new VarDecl(DataType::BYTE, *$2); 
        delete $2; 
    }
    | T_INT T_ID ';' { 
        $$ = new VarDecl(DataType::INT, *$2); 
        delete $2; 
    }
    | T_ID '=' expression ';' { 
        $$ = new Assign(*$1, $3); 
        delete $1; 
    }
    | T_IF '(' expression ')' block { 
        $$ = new IfStmt($3, $5, nullptr); 
    }
    | T_IF '(' expression ')' block T_ELSE block { 
        $$ = new IfStmt($3, $5, $7); 
    }
    | T_WHILE '(' expression ')' block { 
        $$ = new WhileStmt($3, $5); 
    }
    | T_POKE '(' expression ',' expression ')' ';' { 
        $$ = new PokeStmt($3, $5); 
    }
    | T_RETURN expression ';' { 
        $$ = new ReturnStmt($2); 
    }
    | T_RETURN ';' { 
        $$ = new ReturnStmt(nullptr); 
    }
    ;

expression:
    T_NUM { 
        $$ = new NumExpr($1); 
    }
    | T_ID { 
        $$ = new IdExpr(*$1); 
        delete $1; 
    }
    | T_PEEK '(' expression ')' { 
        $$ = new PeekExpr($3); 
    }
    | expression '+' expression { $$ = new BinaryExpr('+', $1, $3); }
    | expression '-' expression { $$ = new BinaryExpr('-', $1, $3); }
    | expression '<' expression { $$ = new BinaryExpr('<', $1, $3); }
    | expression '>' expression { $$ = new BinaryExpr('>', $1, $3); }
    | expression T_EQ expression { $$ = new BinaryExpr('E', $1, $3); /* 'E' por == */ }
    | expression T_AND expression { $$ = new BinaryExpr('A', $1, $3); /* 'A' por && */ }
    | expression T_OR expression { $$ = new BinaryExpr('O', $1, $3); /* 'O' por || */ }
    | '(' expression ')' { $$ = $2; }
    ;

%%
