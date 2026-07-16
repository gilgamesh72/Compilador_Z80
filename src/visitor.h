#pragma once

using namespace std;

// Declaraciones previas de los nodos del AST
// --- Expresiones ---
class ExprNumero;
class ExprCaracter;
class ExprString;
class ExprIdentificador;
class ExprLeerMemoria;
class ExprBinaria;
class ExprUnaria;
class ExprAccesoArreglo;
class ExprIn;
class ExprLlamadaFuncion;     // NUEVO: llamada a función como expresión

// --- Sentencias ---
class Bloque;
class DeclaracionVariable;
class DeclaracionArreglo;
class DeclaracionFuncion;      // NUEVO: definición de función
class Asignacion;
class AsignacionArreglo;
class SentenciaSi;
class SentenciaMientras;
class SentenciaPara;
class SentenciaPoke;
class SentenciaOut;
class SentenciaRetorno;
class SentenciaLlamadaFuncion; // NUEVO: llamada a función como sentencia

class VisitanteAST {
public:
    virtual ~VisitanteAST() = default;

    // Expresiones
    virtual void visitar(ExprNumero* node) = 0;
    virtual void visitar(ExprCaracter* node) = 0;
    virtual void visitar(ExprString* node) = 0;
    virtual void visitar(ExprIdentificador* node) = 0;
    virtual void visitar(ExprLeerMemoria* node) = 0;
    virtual void visitar(ExprBinaria* node) = 0;
    virtual void visitar(ExprUnaria* node) = 0;
    virtual void visitar(ExprAccesoArreglo* node) = 0;
    virtual void visitar(ExprIn* node) = 0;
    virtual void visitar(ExprLlamadaFuncion* node) = 0; // NUEVO

    // Sentencias
    virtual void visitar(Bloque* node) = 0;
    virtual void visitar(DeclaracionVariable* node) = 0;
    virtual void visitar(DeclaracionArreglo* node) = 0;
    virtual void visitar(DeclaracionFuncion* node) = 0;         // NUEVO
    virtual void visitar(Asignacion* node) = 0;
    virtual void visitar(AsignacionArreglo* node) = 0;
    virtual void visitar(SentenciaSi* node) = 0;
    virtual void visitar(SentenciaMientras* node) = 0;
    virtual void visitar(SentenciaPara* node) = 0;
    virtual void visitar(SentenciaPoke* node) = 0;
    virtual void visitar(SentenciaOut* node) = 0;
    virtual void visitar(SentenciaRetorno* node) = 0;
    virtual void visitar(SentenciaLlamadaFuncion* node) = 0;   // NUEVO
};
