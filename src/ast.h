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
    ARRAY,// Tipo arreglo (el tipo base se especifica aparte)
    STRING, // Puntero a literal de cadena (16 bits)
    UNKNOWN
};

// Tabla de Simbolos
struct Symbol {
    string   name;
    DataType type;
    int      offset;    // Para ubicar variables en memoria
    DataType baseType;  // Tipo base para arreglos (BYTE, INT…)
    int      arraySize; // Número de elementos (0 si no es arreglo)
    bool     isArray;

    Symbol()
        : type(DataType::UNKNOWN), offset(0),
          baseType(DataType::UNKNOWN), arraySize(0), isArray(false) {}
};

class SymbolTable {
public:
    map<string, Symbol> symbols;
    SymbolTable* parent;
    int currentOffset; // Para asignar offsets a variables locales

    SymbolTable(SymbolTable* p = nullptr) : parent(p), currentOffset(0) {}

    // Insertar variable simple
    bool insert(const string& name, DataType type) {
        if (symbols.find(name) != symbols.end()) return false;
        Symbol s;
        s.name    = name;
        s.type    = type;
        s.offset  = currentOffset;
        s.isArray = false;
        symbols[name] = s;
        currentOffset += (type == DataType::BYTE || type == DataType::CHAR) ? 1 : 2;
        return true;
    }

    // Insertar arreglo: basetype[size]
    bool insertArray(const string& name, DataType baseType, int size) {
        if (symbols.find(name) != symbols.end()) return false;
        Symbol s;
        s.name      = name;
        s.type      = DataType::ARRAY;
        s.baseType  = baseType;
        s.arraySize = size;
        s.offset    = currentOffset;
        s.isArray   = true;
        symbols[name] = s;
        int elemSize = (baseType == DataType::BYTE || baseType == DataType::CHAR) ? 1 : 2;
        currentOffset += elemSize * size;
        return true;
    }

    Symbol* lookup(const string& name) {
        if (symbols.count(name)) return &symbols[name];
        if (parent) return parent->lookup(name);
        return nullptr;
    }
};

// Clases Base del AST

class NodoAST {
public:
    virtual ~NodoAST() = default;
    virtual void aceptar(VisitanteAST& visitante) = 0;
};

// Clase base para las expresiones
class Expresion : public NodoAST {
public:
    DataType exprType = DataType::UNKNOWN;
    string   tmpVar;   // Variable temporal TAC que almacena el resultado
};

// Clase base para las sentencias
class Sentencia : public NodoAST {
};

// Expresiones

// Literal numérico entero
class ExprNumero : public Expresion {
public:
    int value;
    ExprNumero(int v) : value(v) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Literal de carácter  'x'
class ExprCaracter : public Expresion {
public:
    int value;
    ExprCaracter(int v) : value(v) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Literal de cadena  "texto"
class ExprString : public Expresion {
public:
    string value;           // Contenido sin comillas
    string label;           // Etiqueta ASM donde se almacena
    ExprString(const string& s) : value(s) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Variable simple  foo
class ExprIdentificador : public Expresion {
public:
    string name;
    ExprIdentificador(const string& n) : name(n) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// peek(addr)  — lectura de memoria
class ExprLeerMemoria : public Expresion {
public:
    Expresion* address;
    ExprLeerMemoria(Expresion* addr) : address(addr) {}
    ~ExprLeerMemoria() { delete address; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Operación binaria: +, -, *, /, %, <, >, ==, !=, <=, >=, &&, ||, |, ^, &, <<, >>
class ExprBinaria : public Expresion {
public:
    string    op;   // Representación textual del operador (permite >2 chars)
    Expresion* left;
    Expresion* right;
    ExprBinaria(const string& o, Expresion* l, Expresion* r) : op(o), left(l), right(r) {}
    ~ExprBinaria() { delete left; delete right; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Operación unaria: !, ~, - (negación)
class ExprUnaria : public Expresion {
public:
    string    op;
    Expresion* operand;
    ExprUnaria(const string& o, Expresion* e) : op(o), operand(e) {}
    ~ExprUnaria() { delete operand; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Acceso a arreglo  arr[idx]
class ExprAccesoArreglo : public Expresion {
public:
    string     name;  // Nombre del arreglo
    Expresion* index; // Índice
    ExprAccesoArreglo(const string& n, Expresion* i) : name(n), index(i) {}
    ~ExprAccesoArreglo() { delete index; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Lectura de puerto hardware  in(puerto)
class ExprIn : public Expresion {
public:
    Expresion* port;
    ExprIn(Expresion* p) : port(p) {}
    ~ExprIn() { delete port; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Llamada a funcion como expresion
class ExprLlamadaFuncion : public Expresion {
public:
    string             name;    // Nombre de la función
    vector<Expresion*> args;    // Argumentos

    ExprLlamadaFuncion(const string& n, vector<Expresion*>* a)
        : name(n), args(a ? *a : vector<Expresion*>()) {
        delete a; // Tomar posesión y liberar el puntero al vector
    }
    ~ExprLlamadaFuncion() { for (auto a : args) delete a; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Sentencias

// Bloque { ... }
class Bloque : public Sentencia {
public:
    vector<Sentencia*> statements;
    ~Bloque() { for (auto s : statements) delete s; }
    void agregarSentencia(Sentencia* s) { statements.push_back(s); }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Declaración de variable simple:  int x;
class DeclaracionVariable : public Sentencia {
public:
    DataType type;
    string   name;
    DeclaracionVariable(DataType t, const string& n) : type(t), name(n) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Declaración de arreglo:  int tablero[200];
class DeclaracionArreglo : public Sentencia {
public:
    DataType baseType;  // Tipo de cada elemento
    string   name;      // Nombre del arreglo
    int      size;      // Cantidad de elementos
    DeclaracionArreglo(DataType bt, const string& n, int sz)
        : baseType(bt), name(n), size(sz) {}
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Parametro de funcion
struct Parametro {
    DataType type;
    string   name;
    Parametro(DataType t, const string& n) : type(t), name(n) {}
};

// Definicion de funcion
class DeclaracionFuncion : public Sentencia {
public:
    DataType           returnType;
    string             name;
    vector<Parametro*> params;
    Bloque*            body;

    DeclaracionFuncion(DataType rt, const string& n,
                       vector<Parametro*>* ps, Bloque* b)
        : returnType(rt), name(n), body(b) {
        if (ps) { params = *ps; delete ps; }
    }
    ~DeclaracionFuncion() {
        for (auto p : params) delete p;
        delete body;
    }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Asignación simple:  x = expr;
class Asignacion : public Sentencia {
public:
    string    name;
    Expresion* value;
    Asignacion(const string& n, Expresion* v) : name(n), value(v) {}
    ~Asignacion() { delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Asignación a arreglo:  arr[idx] = expr;
class AsignacionArreglo : public Sentencia {
public:
    string     name;  // Nombre del arreglo
    Expresion* index; // Índice
    Expresion* value; // Valor a asignar
    AsignacionArreglo(const string& n, Expresion* i, Expresion* v)
        : name(n), index(i), value(v) {}
    ~AsignacionArreglo() { delete index; delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Sentencia if / else
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

// Sentencia while
class SentenciaMientras : public Sentencia {
public:
    Expresion* condition;
    Bloque*    body;
    SentenciaMientras(Expresion* cond, Bloque* b) : condition(cond), body(b) {}
    ~SentenciaMientras() { delete condition; delete body; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Sentencia for (init; cond; step) { body }
//   init  -> puede ser nullptr, una Asignacion, o una DeclaracionVariable
//   step  -> puede ser nullptr o una Asignacion
class SentenciaPara : public Sentencia {
public:
    Sentencia* init;      // Inicialización (puede ser nullptr)
    Expresion* condition; // Condición (puede ser nullptr → bucle infinito)
    Sentencia* step;      // Incremento (puede ser nullptr)
    Bloque*    body;

    SentenciaPara(Sentencia* i, Expresion* c, Sentencia* s, Bloque* b)
        : init(i), condition(c), step(s), body(b) {}
    ~SentenciaPara() {
        delete init;
        delete condition;
        delete step;
        delete body;
    }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// poke(addr, val) — escritura directa en memoria
class SentenciaPoke : public Sentencia {
public:
    Expresion* address;
    Expresion* value;
    SentenciaPoke(Expresion* addr, Expresion* val) : address(addr), value(val) {}
    ~SentenciaPoke() { delete address; delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// out(puerto, valor) — escritura en puerto hardware Z80
class SentenciaOut : public Sentencia {
public:
    Expresion* port;
    Expresion* value;
    SentenciaOut(Expresion* p, Expresion* v) : port(p), value(v) {}
    ~SentenciaOut() { delete port; delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// return expr;  /  return;
class SentenciaRetorno : public Sentencia {
public:
    Expresion* value;
    SentenciaRetorno(Expresion* val) : value(val) {}
    ~SentenciaRetorno() { delete value; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};

// Llamada a funcion como sentencia
class SentenciaLlamadaFuncion : public Sentencia {
public:
    string             name;
    vector<Expresion*> args;

    SentenciaLlamadaFuncion(const string& n, vector<Expresion*>* a)
        : name(n), args(a ? *a : vector<Expresion*>()) {
        delete a;
    }
    ~SentenciaLlamadaFuncion() { for (auto a : args) delete a; }
    void aceptar(VisitanteAST& v) override { v.visitar(this); }
};
