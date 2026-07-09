#include "tac.h"
#include <iostream>

using namespace std;

void TACInstr::imprimir() const {
    switch (op) {
        case TACOp::ADD:    cout << result << " = " << arg1 << " + " << arg2 << "\n"; break;
        case TACOp::SUB:    cout << result << " = " << arg1 << " - " << arg2 << "\n"; break;
        case TACOp::LT:     cout << result << " = " << arg1 << " < " << arg2 << "\n"; break;
        case TACOp::GT:     cout << result << " = " << arg1 << " > " << arg2 << "\n"; break;
        case TACOp::EQ:     cout << result << " = " << arg1 << " == " << arg2 << "\n"; break;
        case TACOp::AND:    cout << result << " = " << arg1 << " && " << arg2 << "\n"; break;
        case TACOp::OR:     cout << result << " = " << arg1 << " || " << arg2 << "\n"; break;
        case TACOp::ASSIGN: cout << result << " = " << arg1 << "\n"; break;
        case TACOp::POKE:   cout << "poke(" << arg1 << ", " << arg2 << ")\n"; break;
        case TACOp::PEEK:   cout << result << " = peek(" << arg1 << ")\n"; break;
        case TACOp::JUMP:   cout << "goto " << result << "\n"; break;
        case TACOp::JUMPF:  cout << "ifFalse " << arg1 << " goto " << result << "\n"; break;
        case TACOp::LABEL:  cout << result << ":\n"; break;
        case TACOp::RETURN: cout << "return " << arg1 << "\n"; break;
        default: break;
    }
}

GeneradorTAC::GeneradorTAC() : tempCount(0), labelCount(0) {}

string GeneradorTAC::nuevaTemporal() {
    return "t" + to_string(++tempCount);
}

string GeneradorTAC::nuevaEtiqueta() {
    return "L" + to_string(++labelCount);
}

void GeneradorTAC::emitir(TACOp op, const string& arg1, const string& arg2, const string& result) {
    instructions.push_back({op, arg1, arg2, result});
}

void GeneradorTAC::generar(NodoAST* root) {
    if (root) root->aceptar(*this);
}

void GeneradorTAC::imprimirTodo() const {
    for (const auto& instr : instructions) {
        instr.imprimir();
    }
}

void GeneradorTAC::visitar(ExprNumero* node) {
    node->tmpVar = to_string(node->value); // Literal is its own temp
}

void GeneradorTAC::visitar(ExprCaracter* node) {
    node->tmpVar = to_string(node->value); // El backend usa el código ASCII/Unicode del carácter
}

void GeneradorTAC::visitar(ExprIdentificador* node) {
    node->tmpVar = node->name; // Variable is its own temp
}

void GeneradorTAC::visitar(ExprLeerMemoria* node) {
    if (node->address) node->address->aceptar(*this);
    string tmp = nuevaTemporal();
    emitir(TACOp::PEEK, node->address->tmpVar, "", tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(ExprBinaria* node) {
    if (node->left) node->left->aceptar(*this);
    if (node->right) node->right->aceptar(*this);
    
    string tmp = nuevaTemporal();
    TACOp op;
    switch(node->op) {
        case '+': op = TACOp::ADD; break;
        case '-': op = TACOp::SUB; break;
        case '<': op = TACOp::LT; break;
        case '>': op = TACOp::GT; break;
        case 'E': op = TACOp::EQ; break;
        case 'A': op = TACOp::AND; break;
        case 'O': op = TACOp::OR; break;
        default:  op = TACOp::ADD; break;
    }
    emitir(op, node->left->tmpVar, node->right->tmpVar, tmp);
    node->tmpVar = tmp;
}

void GeneradorTAC::visitar(Bloque* node) {
    for (auto stmt : node->statements) {
        stmt->aceptar(*this);
    }
}

void GeneradorTAC::visitar(DeclaracionVariable* node) {
    // Declaraciones pueden no requerir TAC en variables locales de pila 
    // pero si es Z80 Dummy y pre-asignamos todo como globales, no hacemos nada aquí.
}

void GeneradorTAC::visitar(Asignacion* node) {
    if (node->value) node->value->aceptar(*this);
    emitir(TACOp::ASSIGN, node->value->tmpVar, "", node->name);
}

void GeneradorTAC::visitar(SentenciaSi* node) {
    if (node->condition) node->condition->aceptar(*this);
    
    string labelFalse = nuevaEtiqueta();
    string labelEnd = node->elseBlock ? nuevaEtiqueta() : labelFalse;
    
    emitir(TACOp::JUMPF, node->condition->tmpVar, "", labelFalse);
    
    if (node->thenBlock) node->thenBlock->aceptar(*this);
    
    if (node->elseBlock) {
        emitir(TACOp::JUMP, "", "", labelEnd);
        emitir(TACOp::LABEL, "", "", labelFalse);
        node->elseBlock->aceptar(*this);
    }
    
    emitir(TACOp::LABEL, "", "", labelEnd);
}

void GeneradorTAC::visitar(SentenciaMientras* node) {
    string labelStart = nuevaEtiqueta();
    string labelEnd = nuevaEtiqueta();
    
    emitir(TACOp::LABEL, "", "", labelStart);
    if (node->condition) node->condition->aceptar(*this);
    emitir(TACOp::JUMPF, node->condition->tmpVar, "", labelEnd);
    
    if (node->body) node->body->aceptar(*this);
    
    emitir(TACOp::JUMP, "", "", labelStart);
    emitir(TACOp::LABEL, "", "", labelEnd);
}

void GeneradorTAC::visitar(SentenciaPoke* node) {
    if (node->address) node->address->aceptar(*this);
    if (node->value) node->value->aceptar(*this);
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
