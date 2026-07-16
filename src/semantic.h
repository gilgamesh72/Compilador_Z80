#pragma once
#include "ast.h"
#include "visitor.h"
#include <map>
#include <vector>
#include <utility>

using namespace std;

// Informacion de funcion
struct FunctionInfo {
    DataType                         returnType;
    vector<pair<DataType, string>>   params;  // (tipo, nombre)
};

class AnalizadorSemantico : public VisitanteAST {
private:
    SymbolTable* currentEnv;
    bool hasError;

    // Tabla de funciones declaradas
    map<string, FunctionInfo> functionTable;

    // Tipo de retorno esperado dentro de la función actual
    DataType currentReturnType;

    // Indica si estamos dentro de una función
    bool insideFunction;

    // Helpers internos
    bool esTipoDe8Bits(DataType type);
    bool esFueraDeRangoByte(Expresion* expr);

public:
    AnalizadorSemantico();
    ~AnalizadorSemantico();

    void analizar(NodoAST* root);
    bool tieneErrores() const { return hasError; }

    // --- Expresiones ---
    void visitar(ExprNumero*        node) override;
    void visitar(ExprCaracter*      node) override;
    void visitar(ExprString*        node) override;
    void visitar(ExprIdentificador* node) override;
    void visitar(ExprLeerMemoria*   node) override;
    void visitar(ExprBinaria*       node) override;
    void visitar(ExprUnaria*        node) override;
    void visitar(ExprAccesoArreglo* node) override;
    void visitar(ExprIn*            node) override;
    void visitar(ExprLlamadaFuncion* node) override;  // NUEVO

    // --- Sentencias ---
    void visitar(Bloque*             node) override;
    void visitar(DeclaracionVariable* node) override;
    void visitar(DeclaracionArreglo*  node) override;
    void visitar(DeclaracionFuncion*  node) override;         // NUEVO
    void visitar(Asignacion*         node) override;
    void visitar(AsignacionArreglo*  node) override;
    void visitar(SentenciaSi*        node) override;
    void visitar(SentenciaMientras*  node) override;
    void visitar(SentenciaPara*      node) override;
    void visitar(SentenciaPoke*      node) override;
    void visitar(SentenciaOut*       node) override;
    void visitar(SentenciaRetorno*   node) override;
    void visitar(SentenciaLlamadaFuncion* node) override;    // NUEVO
};
