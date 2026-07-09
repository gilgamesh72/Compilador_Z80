#pragma once

using namespace std;

// Declaraciones previas de los nodos del AST
class ExprNumero;
class ExprCaracter;
class ExprIdentificador;
class ExprLeerMemoria;
class ExprBinaria;
class Bloque;
class DeclaracionVariable;
class Asignacion;
class SentenciaSi;
class SentenciaMientras;
class SentenciaPoke;
class SentenciaRetorno;

class VisitanteAST {
public:
    virtual ~VisitanteAST() = default;

    virtual void visitar(ExprNumero* node) = 0;
    virtual void visitar(ExprCaracter* node) = 0;
    virtual void visitar(ExprIdentificador* node) = 0;
    virtual void visitar(ExprLeerMemoria* node) = 0;
    virtual void visitar(ExprBinaria* node) = 0;
    
    virtual void visitar(Bloque* node) = 0;
    virtual void visitar(DeclaracionVariable* node) = 0;
    virtual void visitar(Asignacion* node) = 0;
    virtual void visitar(SentenciaSi* node) = 0;
    virtual void visitar(SentenciaMientras* node) = 0;
    virtual void visitar(SentenciaPoke* node) = 0;
    virtual void visitar(SentenciaRetorno* node) = 0;
};
