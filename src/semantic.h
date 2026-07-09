#pragma once
#include "ast.h"
#include "visitor.h"

using namespace std;

class AnalizadorSemantico : public VisitanteAST {
private:
    SymbolTable* currentEnv;
    bool hasError;

public:
    AnalizadorSemantico();
    ~AnalizadorSemantico();

    void analizar(NodoAST* root);
    bool tieneErrores() const { return hasError; }

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
