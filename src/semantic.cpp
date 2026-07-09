#include "semantic.h"
#include <iostream>

using namespace std;

AnalizadorSemantico::AnalizadorSemantico() : currentEnv(new SymbolTable()), hasError(false) {}

AnalizadorSemantico::~AnalizadorSemantico() {
    // Liberar ambientes
    while (currentEnv) {
        SymbolTable* parent = currentEnv->parent;
        delete currentEnv;
        currentEnv = parent;
    }
}

void AnalizadorSemantico::analizar(NodoAST* root) {
    if (root) {
        root->aceptar(*this);
    }
}

void AnalizadorSemantico::visitar(ExprNumero* node) {
    // Literal numbers default to INT, unless truncated later
    node->exprType = DataType::INT;
}

void AnalizadorSemantico::visitar(ExprCaracter* node) {
    node->exprType = DataType::CHAR;
}

static bool esFueraDeRangoByte(Expresion* expr) {
    ExprNumero* num = dynamic_cast<ExprNumero*>(expr);
    return num && (num->value < 0 || num->value > 255);
}

static bool esTipoDe8Bits(DataType type) {
    return type == DataType::BYTE || type == DataType::CHAR;
}

void AnalizadorSemantico::visitar(ExprIdentificador* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Variable no declarada '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
    } else {
        node->exprType = sym->type;
    }
}

void AnalizadorSemantico::visitar(ExprLeerMemoria* node) {
    if (node->address) node->address->aceptar(*this);
    node->exprType = DataType::BYTE; // peek devuelve un byte de memoria
}

void AnalizadorSemantico::visitar(ExprBinaria* node) {
    if (node->left) node->left->aceptar(*this);
    if (node->right) node->right->aceptar(*this);
    
    // Simplificado: promover a INT si alguno es INT, si no BYTE
    DataType l = node->left->exprType;
    DataType r = node->right->exprType;
    
    if (l == DataType::UNKNOWN || r == DataType::UNKNOWN) {
        node->exprType = DataType::UNKNOWN;
        return;
    }

    if (node->op == '<' || node->op == '>' || node->op == 'E' || node->op == 'A' || node->op == 'O') {
        node->exprType = DataType::BYTE; // boolean resulta en byte
    } else {
        if (l == DataType::INT || r == DataType::INT) {
            node->exprType = DataType::INT;
        } else if (l == DataType::CHAR || r == DataType::CHAR) {
            node->exprType = DataType::CHAR;
        } else {
            node->exprType = DataType::BYTE;
        }
    }
}

void AnalizadorSemantico::visitar(Bloque* node) {
    // Crear un nuevo scope
    SymbolTable* newEnv = new SymbolTable(currentEnv);
    currentEnv = newEnv;

    for (auto stmt : node->statements) {
        stmt->aceptar(*this);
    }

    // Volver al scope anterior
    SymbolTable* oldEnv = currentEnv;
    currentEnv = currentEnv->parent;
    delete oldEnv;
}

void AnalizadorSemantico::visitar(DeclaracionVariable* node) {
    if (!currentEnv->insert(node->name, node->type)) {
        cerr << "Error semántico: Variable ya declarada '" << node->name << "' en el mismo scope\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);
    
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Asignación a variable no declarada '" << node->name << "'\n";
        hasError = true;
    } else if (node->value->exprType == DataType::VOID) {
         cerr << "Error semántico: No se puede asignar tipo VOID a la variable '" << node->name << "'\n";
         hasError = true;
    } else if (esTipoDe8Bits(sym->type) && esFueraDeRangoByte(node->value)) {
        ExprNumero* num = dynamic_cast<ExprNumero*>(node->value);
        cerr << "Error semántico: El valor " << num->value
             << " está fuera del rango de 8 bits (0-255) para la variable '" << node->name << "'\n";
        hasError = true;
    }
}

void AnalizadorSemantico::visitar(SentenciaSi* node) {
    if (node->condition) node->condition->aceptar(*this);
    if (node->thenBlock) node->thenBlock->aceptar(*this);
    if (node->elseBlock) node->elseBlock->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaMientras* node) {
    if (node->condition) node->condition->aceptar(*this);
    if (node->body) node->body->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
}

void AnalizadorSemantico::visitar(SentenciaRetorno* node) {
    if (node->value) {
        node->value->aceptar(*this);
    }
}
