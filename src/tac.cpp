#include "tac.h"
#include <iostream>
#include <map>

using namespace std;

// ============================================================
//  Impresión de instrucciones TAC (para depuración)
// ============================================================

void TACInstr::imprimir() const {
    switch (op) {
        // Aritméticas
        case TACOp::ADD:    cout << result << " = " << arg1 << " + "  << arg2 << "\n"; break;
        case TACOp::SUB:    cout << result << " = " << arg1 << " - "  << arg2 << "\n"; break;
        case TACOp::MUL:    cout << result << " = " << arg1 << " * "  << arg2 << "\n"; break;
        case TACOp::DIV:    cout << result << " = " << arg1 << " / "  << arg2 << "\n"; break;
        case TACOp::MOD:    cout << result << " = " << arg1 << " % "  << arg2 << "\n"; break;
        // Comparaciones
        case TACOp::LT:     cout << result << " = " << arg1 << " < "  << arg2 << "\n"; break;
        case TACOp::GT:     cout << result << " = " << arg1 << " > "  << arg2 << "\n"; break;
        case TACOp::EQ:     cout << result << " = " << arg1 << " == " << arg2 << "\n"; break;
        case TACOp::NEQ:    cout << result << " = " << arg1 << " != " << arg2 << "\n"; break;
        case TACOp::LE:     cout << result << " = " << arg1 << " <= " << arg2 << "\n"; break;
        case TACOp::GE:     cout << result << " = " << arg1 << " >= " << arg2 << "\n"; break;
        // Lógicas
        case TACOp::AND:    cout << result << " = " << arg1 << " && " << arg2 << "\n"; break;
        case TACOp::OR:     cout << result << " = " << arg1 << " || " << arg2 << "\n"; break;
        case TACOp::NOT:    cout << result << " = !" << arg1 << "\n"; break;
        // Bitwise
        case TACOp::BAND:   cout << result << " = " << arg1 << " & "  << arg2 << "\n"; break;
        case TACOp::BOR:    cout << result << " = " << arg1 << " | "  << arg2 << "\n"; break;
        case TACOp::BXOR:   cout << result << " = " << arg1 << " ^ "  << arg2 << "\n"; break;
        case TACOp::BNOT:   cout << result << " = ~" << arg1 << "\n"; break;
        case TACOp::LSHIFT: cout << result << " = " << arg1 << " << " << arg2 << "\n"; break;
        case TACOp::RSHIFT: cout << result << " = " << arg1 << " >> " << arg2 << "\n"; break;
        // Memoria
        case TACOp::ASSIGN: cout << result << " = " << arg1 << "\n"; break;
        case TACOp::POKE:   cout << "poke(" << arg1 << ", " << arg2 << ")\n"; break;
        case TACOp::PEEK:   cout << result << " = peek(" << arg1 << ")\n"; break;
        // Arreglos
        case TACOp::ARRAY_LOAD:  cout << result << " = " << arg1 << "[" << arg2 << "]\n"; break;
        case TACOp::ARRAY_STORE: cout << arg1 << "[" << arg2 << "] = " << result << "\n"; break;
        case TACOp::ADDR_OF:     cout << result << " = &" << arg1 << "\n"; break;
        // Puertos
        case TACOp::PORT_IN:  cout << result << " = IN(" << arg1 << ")\n"; break;
        case TACOp::PORT_OUT: cout << "OUT(" << arg1 << ", " << arg2 << ")\n"; break;
        // Flujo
        case TACOp::JUMP:   cout << "goto " << result << "\n"; break;
        case TACOp::JUMPF:  cout << "ifFalse " << arg1 << " goto " << result << "\n"; break;
        case TACOp::LABEL:  cout << result << ":\n"; break;
        case TACOp::RETURN: cout << "return " << arg1 << "\n"; break;
        // Funciones
        case TACOp::FUNC_BEGIN:
            cout << "func_begin " << arg1 << " (" << arg2 << " params)\n"; break;
        case TACOp::FUNC_END:
            cout << "func_end " << arg1 << "\n"; break;
        case TACOp::PARAM_DECL:
            cout << "param_decl " << arg1 << " @ IX+" << arg2 << "\n"; break;
        case TACOp::PARAM:
            cout << "param " << arg1 << "\n"; break;
        case TACOp::CALL:
            cout << result << " = call " << arg1 << " (" << arg2 << " args)\n"; break;
        // Cadenas
        case TACOp::STRING_DECL:
            cout << "STRING " << arg1 << " = \"" << arg2 << "\"\n"; break;
        default: break;
    }
}

// ============================================================
//  Constructor e inicialización
// ============================================================

GeneradorTAC::GeneradorTAC()
    : tempCount(0), labelCount(0), stringCount(0) {}

string GeneradorTAC::nuevaTemporal() {
    return "t" + to_string(++tempCount);
}

string GeneradorTAC::nuevaEtiqueta() {
    return "L" + to_string(++labelCount);
}

string GeneradorTAC::nuevaEtiquetaString() {
    return "STR_" + to_string(stringCount++);
}

void GeneradorTAC::emitir(TACOp op,
                           const string& arg1,
                           const string& arg2,
                           const string& result) {
    instructions.push_back({op, arg1, arg2, result});
}

void GeneradorTAC::generar(NodoAST* root) {
    if (root) root->aceptar(*this);
}

void GeneradorTAC::imprimirTodo() const {
    for (const auto& i : instructions) i.imprimir();
}

// ============================================================
//  Expresiones
// ============================================================

void GeneradorTAC::visitar(ExprNumero* node) {
    node->tmpVar = to_string(node->value);
}

void GeneradorTAC::visitar(ExprCaracter* node) {
    node->tmpVar = to_string(node->value);
}

void GeneradorTAC::visitar(ExprString* node) {
    // Emitir la declaración del literal en la sección de datos
    string lbl = nuevaEtiquetaString();
    node->label  = lbl;
    emitir(TACOp::STRING_DECL, lbl, node->value, "");
    // La "variable temporal" es la etiqueta (un puntero 16 bits)
    node->tmpVar = lbl;
}

void GeneradorTAC::visitar(ExprIdentificador* node) {
    node->tmpVar = node->name;
}

void GeneradorTAC::visitar(ExprLeerMemoria* node) {
    if (node->address) node->address->aceptar(*this);
    string tmp = nuevaTemporal();
    emitir(TACOp::PEEK, node->address->tmpVar, "", tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprBinaria* node) {
    if (node->left)  node->left->aceptar(*this);
    if (node->right) node->right->aceptar(*this);

    string tmp = nuevaTemporal();

    // Mapeo de operador string → TACOp
    static const map<string, TACOp> opMap = {
        {"+",  TACOp::ADD},  {"-",  TACOp::SUB},
        {"*",  TACOp::MUL},  {"/",  TACOp::DIV}, {"%",  TACOp::MOD},
        {"<",  TACOp::LT},   {">",  TACOp::GT},
        {"<=", TACOp::LE},   {">=", TACOp::GE},
        {"==", TACOp::EQ},   {"!=", TACOp::NEQ},
        {"&&", TACOp::AND},  {"||", TACOp::OR},
        {"&",  TACOp::BAND}, {"|",  TACOp::BOR}, {"^",  TACOp::BXOR},
        {"<<", TACOp::LSHIFT}, {">>", TACOp::RSHIFT},
    };

    auto it = opMap.find(node->op);
    TACOp op = (it != opMap.end()) ? it->second : TACOp::ADD;

    emitir(op, node->left->tmpVar, node->right->tmpVar, tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprUnaria* node) {
    if (node->operand) node->operand->aceptar(*this);

    string tmp = nuevaTemporal();
    if (node->op == "!") {
        emitir(TACOp::NOT,  node->operand->tmpVar, "", tmp);
    } else if (node->op == "~") {
        emitir(TACOp::BNOT, node->operand->tmpVar, "", tmp);
    } else if (node->op == "-") {
        // Negación: 0 - operand
        emitir(TACOp::SUB, "0", node->operand->tmpVar, tmp);
    }
    node->tmpVar = tmp;
}

// Acceso a arreglo:  result = arr[index]
void GeneradorTAC::visitar(ExprAccesoArreglo* node) {
    if (node->index) node->index->aceptar(*this);
    string tmp = nuevaTemporal();
    emitir(TACOp::ARRAY_LOAD, node->name, node->index->tmpVar, tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprIn* node) {
    if (node->port) node->port->aceptar(*this);
    string tmp = nuevaTemporal();
    emitir(TACOp::PORT_IN, node->port->tmpVar, "", tmp);
    node->tmpVar = tmp;
}

// ============================================================
//  NUEVO: Llamada a función como expresión   foo(a, b)
//  Convención cdecl: argumentos de derecha a izquierda
// ============================================================
void GeneradorTAC::visitar(ExprLlamadaFuncion* node) {
    // Evaluar argumentos de izquierda a derecha (necesitamos sus tmpVars)
    for (auto arg : node->args) {
        arg->aceptar(*this);
    }
    // Emitir PARAM de derecha a izquierda (cdecl)
    for (int i = (int)node->args.size() - 1; i >= 0; i--) {
        emitir(TACOp::PARAM, node->args[i]->tmpVar, "", "");
    }
    // Emitir CALL; el resultado (valor de retorno) va a una temporal
    string tmp = nuevaTemporal();
    emitir(TACOp::CALL, node->name, to_string(node->args.size()), tmp);
    node->tmpVar = tmp;
}

// ============================================================
//  Sentencias
// ============================================================

void GeneradorTAC::visitar(Bloque* node) {
    for (auto stmt : node->statements) {
        stmt->aceptar(*this);
    }
}

void GeneradorTAC::visitar(DeclaracionVariable* node) {
    // Sin código TAC para declaraciones simples (se resuelven en el generador Z80)
    (void)node;
}

void GeneradorTAC::visitar(DeclaracionArreglo* node) {
    // Emitir ADDR_OF para registrar la dirección base del arreglo
    emitir(TACOp::ADDR_OF, node->name, to_string(node->size), node->name + "_base");
}

// ============================================================
//  NUEVO: Definición de función
// ============================================================
void GeneradorTAC::visitar(DeclaracionFuncion* node) {
    // Marcar inicio de la función
    emitir(TACOp::FUNC_BEGIN, node->name, to_string(node->params.size()), "");

    // Declarar cada parámetro con su offset en IX
    // Prologue: PUSH IX; LD IX,0; ADD IX,SP
    //   → IX+0/1 = saved IX,  IX+2/3 = ret addr
    //   → IX+4/5 = param[0],  IX+6/7 = param[1], ...
    for (int i = 0; i < (int)node->params.size(); i++) {
        int offset = 4 + i * 2;
        emitir(TACOp::PARAM_DECL, node->params[i]->name, to_string(offset), "");
    }

    // Generar cuerpo
    if (node->body) node->body->aceptar(*this);

    // Epilogo implícito (para funciones void sin return explícito)
    emitir(TACOp::FUNC_END, node->name, "", "");
}

void GeneradorTAC::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);
    emitir(TACOp::ASSIGN, node->value->tmpVar, "", node->name);
}

// Asignación a arreglo:  arr[index] = value
void GeneradorTAC::visitar(AsignacionArreglo* node) {
    if (node->index) node->index->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
    emitir(TACOp::ARRAY_STORE, node->name, node->index->tmpVar, node->value->tmpVar);
}

void GeneradorTAC::visitar(SentenciaSi* node) {
    if (node->condition) node->condition->aceptar(*this);

    string labelFalse = nuevaEtiqueta();
    string labelEnd   = node->elseBlock ? nuevaEtiqueta() : labelFalse;

    emitir(TACOp::JUMPF, node->condition->tmpVar, "", labelFalse);

    if (node->thenBlock) node->thenBlock->aceptar(*this);

    if (node->elseBlock) {
        emitir(TACOp::JUMP,  "", "", labelEnd);
        emitir(TACOp::LABEL, "", "", labelFalse);
        node->elseBlock->aceptar(*this);
    }

    emitir(TACOp::LABEL, "", "", labelEnd);
}

void GeneradorTAC::visitar(SentenciaMientras* node) {
    string labelStart = nuevaEtiqueta();
    string labelEnd   = nuevaEtiqueta();

    emitir(TACOp::LABEL, "", "", labelStart);
    if (node->condition) node->condition->aceptar(*this);
    emitir(TACOp::JUMPF, node->condition->tmpVar, "", labelEnd);

    if (node->body) node->body->aceptar(*this);

    emitir(TACOp::JUMP,  "", "", labelStart);
    emitir(TACOp::LABEL, "", "", labelEnd);
}

// for (init; cond; step) { body }
void GeneradorTAC::visitar(SentenciaPara* node) {
    // Inicialización (puede ser nullptr)
    if (node->init) node->init->aceptar(*this);

    string labelStart = nuevaEtiqueta();
    string labelEnd   = nuevaEtiqueta();

    emitir(TACOp::LABEL, "", "", labelStart);

    // Condición (si es nullptr, el bucle es infinito)
    if (node->condition) {
        node->condition->aceptar(*this);
        emitir(TACOp::JUMPF, node->condition->tmpVar, "", labelEnd);
    }

    // Cuerpo
    if (node->body) node->body->aceptar(*this);

    // Paso (incremento)
    if (node->step) node->step->aceptar(*this);

    emitir(TACOp::JUMP,  "", "", labelStart);
    emitir(TACOp::LABEL, "", "", labelEnd);
}

void GeneradorTAC::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value)   node->value->aceptar(*this);
    emitir(TACOp::POKE, node->address->tmpVar, node->value->tmpVar, "");
}

void GeneradorTAC::visitar(SentenciaOut* node) {
    if (node->port)  node->port->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
    emitir(TACOp::PORT_OUT, node->port->tmpVar, node->value->tmpVar, "");
}

void GeneradorTAC::visitar(SentenciaRetorno* node) {
    if (node->value) {
        node->value->aceptar(*this);
        emitir(TACOp::RETURN, node->value->tmpVar, "", "");
    } else {
        emitir(TACOp::RETURN, "", "", "");
    }
}

// ============================================================
//  NUEVO: Llamada a función como sentencia   foo(a, b);
// ============================================================
void GeneradorTAC::visitar(SentenciaLlamadaFuncion* node) {
    // Evaluar argumentos
    for (auto arg : node->args) {
        arg->aceptar(*this);
    }
    // Emitir PARAM de derecha a izquierda (cdecl)
    for (int i = (int)node->args.size() - 1; i >= 0; i--) {
        emitir(TACOp::PARAM, node->args[i]->tmpVar, "", "");
    }
    // CALL sin usar el valor de retorno (result = "")
    emitir(TACOp::CALL, node->name, to_string(node->args.size()), "");
}
