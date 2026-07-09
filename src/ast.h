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
    CHAR, // Carácter de 8 bits
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
        currentOffset += (type == DataType::BYTE || type == DataType::CHAR) ? 1 : 2;
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

class NodoAST {
public:
    virtual ~NodoAST() = default;
    virtual void aceptar(VisitanteAST& visitante) = 0;
};

// Clase base para las expresiones
class Expresion : public NodoAST {
public:
    DataType exprType = DataType::UNKNOWN; // Agregado para el análisis semántico
    string tmpVar; // Agregado para el TAC (variable temporal que almacena su resultado)
};

// Clase base para las sentencias
class Sentencia : public NodoAST {
};

// --- Expresiones ---

class ExprNumero : public Expresion {
public:
    int value;
    ExprNumero(int v) : value(v) {}
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class ExprCaracter : public Expresion {
public:
    int value;
    ExprCaracter(int v) : value(v) {}
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class ExprIdentificador : public Expresion {
public:
    string name;
    ExprIdentificador(const string& n) : name(n) {}
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class ExprLeerMemoria : public Expresion {
public:
    Expresion* address;
    ExprLeerMemoria(Expresion* addr) : address(addr) {}
    ~ExprLeerMemoria() { delete address; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class ExprBinaria : public Expresion {
public:
    char op; 
    Expresion* left;
    Expresion* right;
    ExprBinaria(char o, Expresion* l, Expresion* r) : op(o), left(l), right(r) {}
    ~ExprBinaria() { delete left; delete right; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

// --- Sentencias ---

class Bloque : public Sentencia {
public:
    vector<Sentencia*> statements;
    ~Bloque() { 
        for(auto s : statements) delete s; 
    }
    void agregarSentencia(Sentencia* s) { statements.push_back(s); }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class DeclaracionVariable : public Sentencia {
public:
    DataType type;
    string name;
    DeclaracionVariable(DataType t, const string& n) : type(t), name(n) {}
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class Asignacion : public Sentencia {
public:
    string name;
    Expresion* value;
    Asignacion(const string& n, Expresion* v) : name(n), value(v) {}
    ~Asignacion() { delete value; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class SentenciaSi : public Sentencia {
public:
    Expresion* condition;
    Bloque* thenBlock;
    Bloque* elseBlock;
    SentenciaSi(Expresion* cond, Bloque* thenB, Bloque* elseB = nullptr) 
        : condition(cond), thenBlock(thenB), elseBlock(elseB) {}
    ~SentenciaSi() { delete condition; delete thenBlock; delete elseBlock; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class SentenciaMientras : public Sentencia {
public:
    Expresion* condition;
    Bloque* body;
    SentenciaMientras(Expresion* cond, Bloque* b) : condition(cond), body(b) {}
    ~SentenciaMientras() { delete condition; delete body; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class SentenciaPoke : public Sentencia {
public:
    Expresion* address;
    Expresion* value;
    SentenciaPoke(Expresion* addr, Expresion* val) : address(addr), value(val) {}
    ~SentenciaPoke() { delete address; delete value; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};

class SentenciaRetorno : public Sentencia {
public:
    Expresion* value; 
    SentenciaRetorno(Expresion* val) : value(val) {}
    ~SentenciaRetorno() { delete value; }
    void aceptar(VisitanteAST& visitante) override { visitante.visitar(this); }
};
