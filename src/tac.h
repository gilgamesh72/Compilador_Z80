#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "visitor.h"

using namespace std;

// ============================================================
//  Operaciones TAC extendidas
// ============================================================
enum class TACOp {
    // Aritméticas
    ADD, SUB, MUL, DIV, MOD,
    // Comparaciones
    LT, GT, EQ, NEQ, LE, GE,
    // Lógicas
    AND, OR, NOT,
    // Bits
    BAND, BOR, BXOR, BNOT, LSHIFT, RSHIFT,
    // Memoria
    ASSIGN,
    POKE, PEEK,
    // Arreglos: result = base[index]  /  base[index] = arg1
    ARRAY_LOAD,   // result = arr[index]      arg1=arr arg2=index
    ARRAY_STORE,  // arr[index] = val          arg1=arr arg2=index result=val
    ADDR_OF,      // result = &arr             (dirección base del arreglo)
    // Puertos Z80
    PORT_IN,      // result = IN(port)
    PORT_OUT,     // OUT(port, val)
    // Control de flujo
    JUMP, JUMPF,
    LABEL,
    RETURN,
    // ── Funciones ──────────────────────────────────────────
    FUNC_BEGIN,   // arg1=nombre  arg2=num_params
    FUNC_END,     // arg1=nombre  (epilogo implícito si no hubo RETURN)
    PARAM_DECL,   // arg1=nombre_param  arg2=offset_IX  (callee: mapear IX+N → var)
    PARAM,        // arg1=valor  (caller: push argumento antes del CALL)
    CALL,         // arg1=nombre_func  arg2=num_args  result=tmp_retval
    // Literales de cadena
    STRING_DECL   // Define un literal de cadena: arg1=label, arg2=contenido
};

struct TACInstr {
    TACOp  op;
    string arg1;
    string arg2;
    string result;

    void imprimir() const;
};

// ============================================================
//  Generador de TAC (patrón Visitor)
// ============================================================
class GeneradorTAC : public VisitanteAST {
private:
    int tempCount;
    int labelCount;
    int stringCount;                    // Contador de literales de cadena
    vector<TACInstr> instructions;

    string nuevaTemporal();
    string nuevaEtiqueta();
    string nuevaEtiquetaString();
    void emitir(TACOp op,
                const string& arg1,
                const string& arg2,
                const string& result);

public:
    GeneradorTAC();

    void generar(NodoAST* root);
    const vector<TACInstr>& getInstructions() const { return instructions; }
    void imprimirTodo() const;

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
    void visitar(ExprLlamadaFuncion* node) override;   // NUEVO

    // --- Sentencias ---
    void visitar(Bloque*             node) override;
    void visitar(DeclaracionVariable* node) override;
    void visitar(DeclaracionArreglo*  node) override;
    void visitar(DeclaracionFuncion*  node) override;          // NUEVO
    void visitar(Asignacion*         node) override;
    void visitar(AsignacionArreglo*  node) override;
    void visitar(SentenciaSi*        node) override;
    void visitar(SentenciaMientras*  node) override;
    void visitar(SentenciaPara*      node) override;
    void visitar(SentenciaPoke*      node) override;
    void visitar(SentenciaOut*       node) override;
    void visitar(SentenciaRetorno*   node) override;
    void visitar(SentenciaLlamadaFuncion* node) override;     // NUEVO
};
