#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include "visitor.h"

using namespace std;

// --- Tipos de datos del lenguaje ---
enum class DataType {
    VOID,
    BYTE,    // 8 bits sin signo
    CHAR,    // 8 bits carácter
    INT,     // 16 bits
    UNKNOWN
};

// --- Parámetro de función (con offset asignado por el semántico) ---
struct Parametro {
    DataType type;
    string   name;
    int      frameOffset; // índice del param (0-based), set by semantic
};

// --- Entrada de la tabla de símbolos ---
struct Symbol {
    string   name;
    DataType type;
    int      offset;       // offset de frame (locals) o índice (params)
    bool     initialized;  // ¿ha sido escrita alguna vez?
    bool     isArray;      // ¿es un arreglo?
    int      arraySize;    // cantidad de elementos
    bool     isParam;      // ¿es parámetro de función?
};

// --- Tabla de Símbolos ---
class SymbolTable {
public:
    map<string, Symbol> symbols;
    SymbolTable* parent;
    int currentOffset; // Offset acumulado para variables locales (siempre 2 bytes/var)

    SymbolTable(SymbolTable* p = nullptr) : parent(p), currentOffset(0) {}

    // Para variables globales (1 o 2 bytes según tipo)
    bool insert(const string& name, DataType type) {
        if (symbols.count(name)) return false;
        int sz = (type == DataType::BYTE || type == DataType::CHAR) ? 1 : 2;
        symbols[name] = {name, type, currentOffset, false, false, 1, false};
        currentOffset += sz;
        return true;
    }

    // Para variables locales de función (siempre 2 bytes en el stack)
    bool insertLocal(const string& name, DataType type, int resolvedOff) {
        if (symbols.count(name)) return false;
        symbols[name] = {name, type, resolvedOff, false, false, 1, false};
        return true;
    }

    // Para arreglos globales
    bool insertArray(const string& name, DataType type, int size) {
        if (symbols.count(name)) return false;
        int sz = (type == DataType::BYTE || type == DataType::CHAR) ? 1 : 2;
        symbols[name] = {name, type, currentOffset, false, true, size, false};
        currentOffset += sz * size;
        return true;
    }

    // Para arreglos locales (2 bytes/elem en stack)
    bool insertLocalArray(const string& name, DataType type, int size, int resolvedOff) {
        if (symbols.count(name)) return false;
        symbols[name] = {name, type, resolvedOff, false, true, size, false};
        return true;
    }

    // Para parámetros de función (offset = índice del param, 0-based)
    bool insertParam(const string& name, DataType type, int paramIndex) {
        if (symbols.count(name)) return false;
        symbols[name] = {name, type, paramIndex, true /*ya inicializado*/, false, 1, true};
        return true;
    }

    Symbol* lookup(const string& name) {
        if (symbols.count(name)) return &symbols[name];
        if (parent) return parent->lookup(name);
        return nullptr;
    }
};

// ===================================================================
// --- Clases base del AST ---
// ===================================================================

class NodoAST {
public:
    virtual ~NodoAST() = default;
    virtual void aceptar(VisitanteAST& v) = 0;
};

class Expresion : public NodoAST {
public:
    DataType exprType = DataType::UNKNOWN;
    string   tmpVar;   // Variable temporal TAC que almacena el resultado
};

class Sentencia : public NodoAST {};

// ===================================================================
// --- Expresiones ---
// ===================================================================

class ExprNumero : public Expresion {
public:
    int value;
    ExprNumero(int v) : value(v) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprCaracter : public Expresion {
public:
    int value;
    ExprCaracter(int v) : value(v) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprIdentificador : public Expresion {
public:
    string name;
    ExprIdentificador(const string& n) : name(n) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprLeerMemoria : public Expresion {
public:
    Expresion* address;
    ExprLeerMemoria(Expresion* addr) : address(addr) {}
    ~ExprLeerMemoria() { delete address; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprBinaria : public Expresion {
public:
    char op;  // '+','-','*','/','<','>','E'(==),'N'(!=),'L'(<=),'G'(>=),'A'(&&),'O'(||)
    Expresion* left;
    Expresion* right;
    ExprBinaria(char o, Expresion* l, Expresion* r) : op(o), left(l), right(r) {}
    ~ExprBinaria() { delete left; delete right; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprUnaria : public Expresion {
public:
    char op;          // '!' = NOT lógico, '-' = negación aritmética
    Expresion* operand;
    ExprUnaria(char o, Expresion* e) : op(o), operand(e) {}
    ~ExprUnaria() { delete operand; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprLlamadaFuncion : public Expresion {
public:
    string funcName;
    vector<Expresion*> args;
    ExprLlamadaFuncion(const string& n, vector<Expresion*> a)
        : funcName(n), args(move(a)) {}
    ~ExprLlamadaFuncion() { for (auto e : args) delete e; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class ExprAccesoArreglo : public Expresion {
public:
    string name;
    Expresion* index;
    ExprAccesoArreglo(const string& n, Expresion* i) : name(n), index(i) {}
    ~ExprAccesoArreglo() { delete index; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// ===================================================================
// --- Sentencias ---
// ===================================================================

class Bloque : public Sentencia {
public:
    vector<Sentencia*> statements;
    ~Bloque() { for (auto s : statements) delete s; }
    void agregarSentencia(Sentencia* s) { statements.push_back(s); }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class DeclaracionVariable : public Sentencia {
public:
    DataType type;
    string   name;
    int      resolvedOffset; // Asignado por el semántico (-1 si global)
    DeclaracionVariable(DataType t, const string& n)
        : type(t), name(n), resolvedOffset(-1) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class DeclaracionArreglo : public Sentencia {
public:
    DataType type;
    string   name;
    int      size;
    int      resolvedOffset; // Asignado por el semántico (-1 si global)
    DeclaracionArreglo(DataType t, const string& n, int s)
        : type(t), name(n), size(s), resolvedOffset(-1) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class Asignacion : public Sentencia {
public:
    string     name;
    Expresion* value;
    Asignacion(const string& n, Expresion* v) : name(n), value(v) {}
    ~Asignacion() { delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class AsignacionArreglo : public Sentencia {
public:
    string     name;
    Expresion* index;
    Expresion* value;
    AsignacionArreglo(const string& n, Expresion* i, Expresion* v)
        : name(n), index(i), value(v) {}
    ~AsignacionArreglo() { delete index; delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class SentenciaSi : public Sentencia {
public:
    Expresion* condition;
    Bloque*    thenBlock;
    Bloque*    elseBlock;
    SentenciaSi(Expresion* cond, Bloque* thenB, Bloque* elseB = nullptr)
        : condition(cond), thenBlock(thenB), elseBlock(elseB) {}
    ~SentenciaSi() { delete condition; delete thenBlock; delete elseBlock; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class SentenciaMientras : public Sentencia {
public:
    Expresion* condition;
    Bloque*    body;
    SentenciaMientras(Expresion* cond, Bloque* b) : condition(cond), body(b) {}
    ~SentenciaMientras() { delete condition; delete body; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class SentenciaPoke : public Sentencia {
public:
    Expresion* address;
    Expresion* value;
    SentenciaPoke(Expresion* addr, Expresion* val) : address(addr), value(val) {}
    ~SentenciaPoke() { delete address; delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class SentenciaRetorno : public Sentencia {
public:
    Expresion* value;
    SentenciaRetorno(Expresion* val) : value(val) {}
    ~SentenciaRetorno() { delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Expresión usada como sentencia (ej: llamada a función void)
class SentenciaExpresion : public Sentencia {
public:
    Expresion* expr;
    SentenciaExpresion(Expresion* e) : expr(e) {}
    ~SentenciaExpresion() { delete expr; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Sentencia vacía generada por recuperación de errores
class SentenciaNula : public Sentencia {
public:
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

class DefinicionFuncion : public Sentencia {
public:
    DataType         returnType;
    string           name;
    vector<Parametro> params;
    Bloque*          body;
    int              frameSize; // Calculado por el semántico (bytes totales de locales)

    DefinicionFuncion(DataType rt, const string& n, vector<Parametro> ps, Bloque* b)
        : returnType(rt), name(n), params(move(ps)), body(b), frameSize(0) {}
    ~DefinicionFuncion() { delete body; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};
