#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "visitor.h"

using namespace std;

enum class TACOp {
    ADD, SUB, LT, GT, EQ, AND, OR,
    ASSIGN,
    POKE, PEEK,
    JUMP, JUMPF,
    LABEL,
    RETURN,
    PARAM, CALL // Por si el lenguaje crece
};

struct TACInstr {
    TACOp op;
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

    string nuevaTemporal();
    string nuevaEtiqueta();
    void emitir(TACOp op, const string& arg1, const string& arg2, const string& result);

public:
    GeneradorTAC();
    
    void generar(NodoAST* root);
    const vector<TACInstr>& getInstructions() const { return instructions; }
    void imprimirTodo() const;

    void visitar(ExprNumero* node) override;
    void visitar(ExprCaracter* node) override;
    void visitar(ExprIdentificador* node) override;
    void visitar(ExprLeerMemoria* node) override;
    void visitar(ExprBinaria* node) override;
    
    void visitar(Bloque* node) override;
    void visitar(DeclaracionVariable* node) override;
    void visitar(Asignacion* node) override;
    void visitar(SentenciaSi* node) override;
    void visitar(SentenciaMientras* node) override;
    void visitar(SentenciaPoke* node) override;
    void visitar(SentenciaRetorno* node) override;
};
