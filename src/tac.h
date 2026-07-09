#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "visitor.h"

using namespace std;

enum class TACOp {
    // Aritmética
    ADD, SUB, MUL, DIV,
    // Comparaciones
    LT, GT, EQ, NEQ, LEQ, GEQ,
    // Lógicos
    AND, OR,
    // Unarios
    NEG, NOT,
    // Asignación y memoria
    ASSIGN, POKE, PEEK,
    // Control de flujo
    JUMP, JUMPF, LABEL,
    RETURN,
    // Llamadas a función
    PARAM_PUSH, CALL,
    // Información de frame (consumida por Z80gen, no genera instrucciones Z80)
    FUNC_BEGIN,  // arg1=nombre, result=frameSize
    FUNC_END,    // arg1=nombre
    LOCAL_VAR,   // arg1=nombre, arg2=resolvedOffset
    PARAM_DECL,  // arg1=nombre, arg2=paramIndex
    // Arreglos
    ARRAY_DECL,  // arg1=nombre, arg2=elemSize, result=count
    ARRAY_LOAD,  // arg1=arrNombre, arg2=indexVar, result=tmp
    ARRAY_STORE, // arg1=arrNombre, arg2=indexVar, result=valueVar
    // Recuperación de errores
    NOP
};

struct TACInstr {
    TACOp  op;
    string arg1;
    string arg2;
    string result;
    void imprimir() const;
};

class GeneradorTAC : public VisitanteAST {
private:
    int tempCount;
    int labelCount;
    vector<TACInstr> instructions;
    bool inFunction; // ¿Generando código dentro de una función?

    string nuevaTemporal();
    string nuevaEtiqueta();
    void emitir(TACOp op, const string& a1, const string& a2, const string& res);

public:
    GeneradorTAC();

    void generar(NodoAST* root);
    const vector<TACInstr>& getInstructions() const { return instructions; }
    void imprimirTodo() const;

    // Expresiones
    void visitar(ExprNumero* node) override;
    void visitar(ExprCaracter* node) override;
    void visitar(ExprIdentificador* node) override;
    void visitar(ExprLeerMemoria* node) override;
    void visitar(ExprBinaria* node) override;
    void visitar(ExprUnaria* node) override;
    void visitar(ExprLlamadaFuncion* node) override;
    void visitar(ExprAccesoArreglo* node) override;

    // Sentencias
    void visitar(Bloque* node) override;
    void visitar(DeclaracionVariable* node) override;
    void visitar(DeclaracionArreglo* node) override;
    void visitar(Asignacion* node) override;
    void visitar(AsignacionArreglo* node) override;
    void visitar(SentenciaSi* node) override;
    void visitar(SentenciaMientras* node) override;
    void visitar(SentenciaPoke* node) override;
    void visitar(SentenciaRetorno* node) override;
    void visitar(SentenciaExpresion* node) override;
    void visitar(SentenciaNula* node) override;
    void visitar(DefinicionFuncion* node) override;
};
