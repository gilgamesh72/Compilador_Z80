#include "tac.h"
#include <iostream>

using namespace std;

void TACInstr::imprimir() const {
    switch (op) {
        case TACOp::ADD:    cout << result << " = " << arg1 << " + "  << arg2 << "\n"; break;
        case TACOp::SUB:    cout << result << " = " << arg1 << " - "  << arg2 << "\n"; break;
        case TACOp::MUL:    cout << result << " = " << arg1 << " * "  << arg2 << "\n"; break;
        case TACOp::DIV:    cout << result << " = " << arg1 << " / "  << arg2 << "\n"; break;
        case TACOp::LT:     cout << result << " = " << arg1 << " < "  << arg2 << "\n"; break;
        case TACOp::GT:     cout << result << " = " << arg1 << " > "  << arg2 << "\n"; break;
        case TACOp::EQ:     cout << result << " = " << arg1 << " == " << arg2 << "\n"; break;
        case TACOp::NEQ:    cout << result << " = " << arg1 << " != " << arg2 << "\n"; break;
        case TACOp::LEQ:    cout << result << " = " << arg1 << " <= " << arg2 << "\n"; break;
        case TACOp::GEQ:    cout << result << " = " << arg1 << " >= " << arg2 << "\n"; break;
        case TACOp::AND:    cout << result << " = " << arg1 << " && " << arg2 << "\n"; break;
        case TACOp::OR:     cout << result << " = " << arg1 << " || " << arg2 << "\n"; break;
        case TACOp::NEG:    cout << result << " = -"  << arg1 << "\n"; break;
        case TACOp::NOT:    cout << result << " = !"  << arg1 << "\n"; break;
        case TACOp::ASSIGN: cout << result << " = "   << arg1 << "\n"; break;
        case TACOp::POKE:   cout << "poke(" << arg1 << ", " << arg2 << ")\n"; break;
        case TACOp::PEEK:   cout << result << " = peek(" << arg1 << ")\n"; break;
        case TACOp::JUMP:   cout << "goto " << result << "\n"; break;
        case TACOp::JUMPF:  cout << "ifFalse " << arg1 << " goto " << result << "\n"; break;
        case TACOp::LABEL:  cout << result << ":\n"; break;
        case TACOp::RETURN: cout << "return " << arg1 << "\n"; break;
        case TACOp::PARAM_PUSH: cout << "param_push " << arg1 << "\n"; break;
        case TACOp::CALL:   cout << result << " = call " << arg1 << "(" << arg2 << " args)\n"; break;
        case TACOp::FUNC_BEGIN: cout << "func_begin " << arg1 << " [frame=" << result << "]\n"; break;
        case TACOp::FUNC_END:   cout << "func_end "   << arg1 << "\n"; break;
        case TACOp::LOCAL_VAR:  cout << "local_var "  << arg1 << " offset=" << arg2 << "\n"; break;
        case TACOp::PARAM_DECL: cout << "param "      << arg1 << " index="  << arg2 << "\n"; break;
        case TACOp::ARRAY_DECL: cout << "array_decl " << arg1 << " elem=" << arg2 << " n=" << result << "\n"; break;
        case TACOp::ARRAY_LOAD: cout << result << " = " << arg1 << "[" << arg2 << "]\n"; break;
        case TACOp::ARRAY_STORE:cout << arg1 << "[" << arg2 << "] = " << result << "\n"; break;
        case TACOp::NOP:    cout << "nop\n"; break;
        default: break;
    }
}

GeneradorTAC::GeneradorTAC() : tempCount(0), labelCount(0), inFunction(false) {}

string GeneradorTAC::nuevaTemporal() { return "t" + to_string(++tempCount); }
string GeneradorTAC::nuevaEtiqueta() { return "L" + to_string(++labelCount); }

void GeneradorTAC::emitir(TACOp op, const string& a1, const string& a2, const string& res) {
    instructions.push_back({op, a1, a2, res});
}

void GeneradorTAC::generar(NodoAST* root) { if (root) root->aceptar(*this); }

void GeneradorTAC::imprimirTodo() const {
    for (const auto& i : instructions) i.imprimir();
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

void GeneradorTAC::visitar(ExprNumero* node)     { node->tmpVar = to_string(node->value); }
void GeneradorTAC::visitar(ExprCaracter* node)   { node->tmpVar = to_string(node->value); }
void GeneradorTAC::visitar(ExprIdentificador* node) { node->tmpVar = node->name; }

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
    TACOp op;
    switch (node->op) {
        case '+': op = TACOp::ADD; break;  case '-': op = TACOp::SUB; break;
        case '*': op = TACOp::MUL; break;  case '/': op = TACOp::DIV; break;
        case '<': op = TACOp::LT;  break;  case '>': op = TACOp::GT;  break;
        case 'E': op = TACOp::EQ;  break;  case 'N': op = TACOp::NEQ; break;
        case 'L': op = TACOp::LEQ; break;  case 'G': op = TACOp::GEQ; break;
        case 'A': op = TACOp::AND; break;  case 'O': op = TACOp::OR;  break;
        default:  op = TACOp::ADD; break;
    }
    emitir(op, node->left->tmpVar, node->right->tmpVar, tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprUnaria* node) {
    if (node->operand) node->operand->aceptar(*this);
    string tmp = nuevaTemporal();
    TACOp op = (node->op == '!') ? TACOp::NOT : TACOp::NEG;
    emitir(op, node->operand->tmpVar, "", tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprLlamadaFuncion* node) {
    // 1. Evaluar todos los argumentos (izquierda a derecha)
    for (auto arg : node->args) arg->aceptar(*this);
    // 2. Empujar en orden inverso (convención: último arg se apila primero)
    for (int i = (int)node->args.size() - 1; i >= 0; i--)
        emitir(TACOp::PARAM_PUSH, node->args[i]->tmpVar, "", "");
    // 3. Emitir CALL
    string tmp = nuevaTemporal();
    emitir(TACOp::CALL, node->funcName, to_string(node->args.size()), tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprAccesoArreglo* node) {
    if (node->index) node->index->aceptar(*this);
    string tmp = nuevaTemporal();
    emitir(TACOp::ARRAY_LOAD, node->name, node->index->tmpVar, tmp);
    node->tmpVar = tmp;
}

// ---------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------

void GeneradorTAC::visitar(Bloque* node) {
    for (auto s : node->statements) s->aceptar(*this);
}

void GeneradorTAC::visitar(DeclaracionVariable* node) {
    if (inFunction && node->resolvedOffset >= 0) {
        // Informar al Z80gen del offset de este local en el frame
        emitir(TACOp::LOCAL_VAR, node->name, to_string(node->resolvedOffset), "");
    }
    // Las globales no generan instrucciones TAC (van a BSS)
}

void GeneradorTAC::visitar(DeclaracionArreglo* node) {
    if (inFunction && node->resolvedOffset >= 0) {
        emitir(TACOp::LOCAL_VAR, node->name, to_string(node->resolvedOffset), "");
    } else {
        // Arreglo global: declarar su tipo y tamaño para BSS
        int elemSize = (node->type == DataType::BYTE || node->type == DataType::CHAR) ? 1 : 2;
        emitir(TACOp::ARRAY_DECL, node->name, to_string(elemSize), to_string(node->size));
    }
}

void GeneradorTAC::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);
    emitir(TACOp::ASSIGN, node->value->tmpVar, "", node->name);
}

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

void GeneradorTAC::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value)   node->value->aceptar(*this);
    emitir(TACOp::POKE, node->address->tmpVar, node->value->tmpVar, "");
}

void GeneradorTAC::visitar(SentenciaRetorno* node) {
    if (node->value) {
        node->value->aceptar(*this);
        emitir(TACOp::RETURN, node->value->tmpVar, "", "");
    } else {
        emitir(TACOp::RETURN, "", "", "");
    }
}

void GeneradorTAC::visitar(SentenciaExpresion* node) {
    if (node->expr) node->expr->aceptar(*this);
    // El resultado de la expresión se descarta
}

void GeneradorTAC::visitar(SentenciaNula* node) {
    emitir(TACOp::NOP, "", "", "");
}

void GeneradorTAC::visitar(DefinicionFuncion* node) {
    // Emitir inicio de función con tamaño del frame
    emitir(TACOp::FUNC_BEGIN, node->name, "", to_string(node->frameSize));

    // Declarar parámetros (para que el Z80gen los mapee a IX+4, IX+6, ...)
    for (int i = 0; i < (int)node->params.size(); i++)
        emitir(TACOp::PARAM_DECL, node->params[i].name, to_string(i), "");

    // Generar cuerpo de la función
    inFunction = true;
    if (node->body) node->body->aceptar(*this);
    inFunction = false;

    emitir(TACOp::FUNC_END, node->name, "", "");
}
