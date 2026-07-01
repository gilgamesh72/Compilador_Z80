#include "semantic.h"
#include <iostream>

using namespace std;

SemanticAnalyzer::SemanticAnalyzer() : currentEnv(new SymbolTable()), hasError(false) {}

SemanticAnalyzer::~SemanticAnalyzer() {
    // Liberar ambientes
    while (currentEnv) {
        SymbolTable* parent = currentEnv->parent;
        delete currentEnv;
        currentEnv = parent;
    }
}

void SemanticAnalyzer::analyze(ASTNode* root) {
    if (root) {
        root->accept(*this);
    }
}

void SemanticAnalyzer::visit(NumExpr* node) {
    // Literal numbers default to INT, unless truncated later
    node->exprType = DataType::INT;
}

void SemanticAnalyzer::visit(IdExpr* node) {
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Variable no declarada '" << node->name << "'\n";
        hasError = true;
        node->exprType = DataType::UNKNOWN;
    } else {
        node->exprType = sym->type;
    }
}

void SemanticAnalyzer::visit(PeekExpr* node) {
    if (node->address) node->address->accept(*this);
    node->exprType = DataType::BYTE; // peek devuelve un byte de memoria
}

void SemanticAnalyzer::visit(BinaryExpr* node) {
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);
    
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
        node->exprType = (l == DataType::INT || r == DataType::INT) ? DataType::INT : DataType::BYTE;
    }
}

void SemanticAnalyzer::visit(Block* node) {
    // Crear un nuevo scope
    SymbolTable* newEnv = new SymbolTable(currentEnv);
    currentEnv = newEnv;

    for (auto stmt : node->statements) {
        stmt->accept(*this);
    }

    // Volver al scope anterior
    SymbolTable* oldEnv = currentEnv;
    currentEnv = currentEnv->parent;
    delete oldEnv;
}

void SemanticAnalyzer::visit(VarDecl* node) {
    if (!currentEnv->insert(node->name, node->type)) {
        cerr << "Error semántico: Variable ya declarada '" << node->name << "' en el mismo scope\n";
        hasError = true;
    }
}

void SemanticAnalyzer::visit(Assign* node) {
    if (node->value) node->value->accept(*this);
    
    Symbol* sym = currentEnv->lookup(node->name);
    if (!sym) {
        cerr << "Error semántico: Asignación a variable no declarada '" << node->name << "'\n";
        hasError = true;
    } else if (node->value->exprType == DataType::VOID) {
         cerr << "Error semántico: No se puede asignar tipo VOID a la variable '" << node->name << "'\n";
         hasError = true;
    }
}

void SemanticAnalyzer::visit(IfStmt* node) {
    if (node->condition) node->condition->accept(*this);
    if (node->thenBlock) node->thenBlock->accept(*this);
    if (node->elseBlock) node->elseBlock->accept(*this);
}

void SemanticAnalyzer::visit(WhileStmt* node) {
    if (node->condition) node->condition->accept(*this);
    if (node->body) node->body->accept(*this);
}

void SemanticAnalyzer::visit(PokeStmt* node) {
    if (node->address) node->address->accept(*this);
    if (node->value) node->value->accept(*this);
}

void SemanticAnalyzer::visit(ReturnStmt* node) {
    if (node->value) {
        node->value->accept(*this);
    }
}
