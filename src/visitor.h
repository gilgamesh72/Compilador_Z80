#pragma once

// Forward declarations for all AST node types
class ExprNumero;
class ExprCaracter;
class ExprIdentificador;
class ExprLeerMemoria;
class ExprBinaria;
class ExprUnaria;
class ExprLlamadaFuncion;
class ExprAccesoArreglo;
class Bloque;
class DeclaracionVariable;
class DeclaracionArreglo;
class Asignacion;
class AsignacionArreglo;
class SentenciaSi;
class SentenciaMientras;
class SentenciaPoke;
class SentenciaRetorno;
class SentenciaExpresion;
class SentenciaNula;
class DefinicionFuncion;

class VisitanteAST {
public:
    virtual ~VisitanteAST() = default;

    // Expressions
    virtual void visitar(ExprNumero* node) = 0;
    virtual void visitar(ExprCaracter* node) = 0;
    virtual void visitar(ExprIdentificador* node) = 0;
    virtual void visitar(ExprLeerMemoria* node) = 0;
    virtual void visitar(ExprBinaria* node) = 0;
    virtual void visitar(ExprUnaria* node) = 0;
    virtual void visitar(ExprLlamadaFuncion* node) = 0;
    virtual void visitar(ExprAccesoArreglo* node) = 0;

    // Statements
    virtual void visitar(Bloque* node) = 0;
    virtual void visitar(DeclaracionVariable* node) = 0;
    virtual void visitar(DeclaracionArreglo* node) = 0;
    virtual void visitar(Asignacion* node) = 0;
    virtual void visitar(AsignacionArreglo* node) = 0;
    virtual void visitar(SentenciaSi* node) = 0;
    virtual void visitar(SentenciaMientras* node) = 0;
    virtual void visitar(SentenciaPoke* node) = 0;
    virtual void visitar(SentenciaRetorno* node) = 0;
    virtual void visitar(SentenciaExpresion* node) = 0;
    virtual void visitar(SentenciaNula* node) = 0;
    virtual void visitar(DefinicionFuncion* node) = 0;
};
