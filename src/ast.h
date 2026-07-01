#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "visitor.h"

using namespace std;

// Tipos de datos soportados por el lenguaje
enum class DataType {
    VOID,
    BYTE, // Entero de 8 bits sin signo / con signo
    INT,  // Entero de 16 bits
    UNKNOWN
};

// --- Tabla de Símbolos ---
struct Symbol {
    string name;
    DataType type;
    int offset; // Para ubicar variables en memoria
};

class SymbolTable {
public:
    map<string, Symbol> symbols;
    SymbolTable* parent;
    int currentOffset; // Para asignar offsets a variables locales

    SymbolTable(SymbolTable* p = nullptr) : parent(p), currentOffset(0) {}

    bool insert(const string& name, DataType type) {
        if (symbols.find(name) != symbols.end()) {
            return false;
        }
        symbols[name] = {name, type, currentOffset};
        currentOffset += (type == DataType::BYTE) ? 1 : 2;
        return true;
    }

    Symbol* lookup(const string& name) {
        if (symbols.count(name)) {
            return &symbols[name];
        }
        if (parent) {
            return parent->lookup(name);
        }
        return nullptr;
    }
};

// --- Clases Base del AST ---

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

// Clase base para las expresiones
class Expr : public ASTNode {
public:
    DataType exprType = DataType::UNKNOWN; // Agregado para el análisis semántico
    string tmpVar; // Agregado para el TAC (variable temporal que almacena su resultado)
};

// Clase base para las sentencias
class Stmt : public ASTNode {
};

// --- Expresiones ---

class NumExpr : public Expr {
public:
    int value;
    NumExpr(int v) : value(v) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class IdExpr : public Expr {
public:
    string name;
    IdExpr(const string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class PeekExpr : public Expr {
public:
    Expr* address;
    PeekExpr(Expr* addr) : address(addr) {}
    ~PeekExpr() { delete address; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class BinaryExpr : public Expr {
public:
    char op; 
    Expr* left;
    Expr* right;
    BinaryExpr(char o, Expr* l, Expr* r) : op(o), left(l), right(r) {}
    ~BinaryExpr() { delete left; delete right; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

// --- Sentencias ---

class Block : public Stmt {
public:
    vector<Stmt*> statements;
    ~Block() { 
        for(auto s : statements) delete s; 
    }
    void addStatement(Stmt* s) { statements.push_back(s); }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class VarDecl : public Stmt {
public:
    DataType type;
    string name;
    VarDecl(DataType t, const string& n) : type(t), name(n) {}
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class Assign : public Stmt {
public:
    string name;
    Expr* value;
    Assign(const string& n, Expr* v) : name(n), value(v) {}
    ~Assign() { delete value; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class IfStmt : public Stmt {
public:
    Expr* condition;
    Block* thenBlock;
    Block* elseBlock;
    IfStmt(Expr* cond, Block* thenB, Block* elseB = nullptr) 
        : condition(cond), thenBlock(thenB), elseBlock(elseB) {}
    ~IfStmt() { delete condition; delete thenBlock; delete elseBlock; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class WhileStmt : public Stmt {
public:
    Expr* condition;
    Block* body;
    WhileStmt(Expr* cond, Block* b) : condition(cond), body(b) {}
    ~WhileStmt() { delete condition; delete body; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class PokeStmt : public Stmt {
public:
    Expr* address;
    Expr* value;
    PokeStmt(Expr* addr, Expr* val) : address(addr), value(val) {}
    ~PokeStmt() { delete address; delete value; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};

class ReturnStmt : public Stmt {
public:
    Expr* value; 
    ReturnStmt(Expr* val) : value(val) {}
    ~ReturnStmt() { delete value; }
    void accept(ASTVisitor& visitor) override { visitor.visit(this); }
};
