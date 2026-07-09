#pragma once
#include "ast.h"
#include "visitor.h"
#include <map>
#include <vector>
#include <string>

using namespace std;

// Información de una función (para validar llamadas)
struct FunctionInfo {
    string           name;
    DataType         returnType;
    vector<DataType> paramTypes;
    vector<string>   paramNames;
};

class AnalizadorSemantico : public VisitanteAST {
private:
    SymbolTable* currentEnv;
    bool hasError;

    // Contexto de función activa
    bool     inFunction;
    DataType currentFunctionReturnType;
    string   currentFunctionName;

    // Tabla global de funciones declaradas
    map<string, FunctionInfo> functionTable;

    // --- Helpers ---
    // ¿Es una expresión booleana válida para if/while?
    static bool esExpresionBooleana(Expresion* expr);

    // ¿Todos los caminos de un bloque terminan en return?
    bool todosLosCaminosRetornan(Bloque* block);
    bool sentenciaRetorna(Sentencia* stmt);

    // Pre-scan recursivo para asignar offsets de frame a las declaraciones locales
    void asignarOffsetsLocales(Bloque* block, int& offset);

public:
    AnalizadorSemantico();
    ~AnalizadorSemantico();

    void analizar(NodoAST* root);
    bool tieneErrores() const { return hasError; }

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
