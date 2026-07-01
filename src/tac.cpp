#include "tac.h"
#include <iostream>

using namespace std;

void TACInstr::print() const {
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

TACGenerator::TACGenerator() : tempCount(0), labelCount(0) {}

string TACGenerator::newTemp() {
    return "t" + to_string(++tempCount);
}

string TACGenerator::newLabel() {
    return "L" + to_string(++labelCount);
}

void TACGenerator::emit(TACOp op, const string& arg1, const string& arg2, const string& result) {
    instructions.push_back({op, arg1, arg2, result});
}

void TACGenerator::generate(ASTNode* root) {
    if (root) root->accept(*this);
}

void TACGenerator::printAll() const {
    for (const auto& instr : instructions) {
        instr.print();
    }
}

void TACGenerator::visit(NumExpr* node) {
    node->tmpVar = to_string(node->value); // Literal is its own temp
}

void TACGenerator::visit(IdExpr* node) {
    node->tmpVar = node->name; // Variable is its own temp
}

void TACGenerator::visit(PeekExpr* node) {
    if (node->address) node->address->accept(*this);
    string tmp = newTemp();
    emit(TACOp::PEEK, node->address->tmpVar, "", tmp);
    node->tmpVar = tmp;
}

void TACGenerator::visit(BinaryExpr* node) {
    if (node->left) node->left->accept(*this);
    if (node->right) node->right->accept(*this);
    
    string tmp = newTemp();
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
    emit(op, node->left->tmpVar, node->right->tmpVar, tmp);
    node->tmpVar = tmp;
}

void TACGenerator::visit(Block* node) {
    for (auto stmt : node->statements) {
        stmt->accept(*this);
    }
}

void TACGenerator::visit(VarDecl* node) {
    // Declaraciones pueden no requerir TAC en variables locales de pila 
    // pero si es Z80 Dummy y pre-asignamos todo como globales, no hacemos nada aquí.
}

void TACGenerator::visit(Assign* node) {
    if (node->value) node->value->accept(*this);
    emit(TACOp::ASSIGN, node->value->tmpVar, "", node->name);
}

void TACGenerator::visit(IfStmt* node) {
    if (node->condition) node->condition->accept(*this);
    
    string labelFalse = newLabel();
    string labelEnd = node->elseBlock ? newLabel() : labelFalse;
    
    emit(TACOp::JUMPF, node->condition->tmpVar, "", labelFalse);
    
    if (node->thenBlock) node->thenBlock->accept(*this);
    
    if (node->elseBlock) {
        emit(TACOp::JUMP, "", "", labelEnd);
        emit(TACOp::LABEL, "", "", labelFalse);
        node->elseBlock->accept(*this);
    }
    
    emit(TACOp::LABEL, "", "", labelEnd);
}

void TACGenerator::visit(WhileStmt* node) {
    string labelStart = newLabel();
    string labelEnd = newLabel();
    
    emit(TACOp::LABEL, "", "", labelStart);
    if (node->condition) node->condition->accept(*this);
    emit(TACOp::JUMPF, node->condition->tmpVar, "", labelEnd);
    
    if (node->body) node->body->accept(*this);
    
    emit(TACOp::JUMP, "", "", labelStart);
    emit(TACOp::LABEL, "", "", labelEnd);
}

void TACGenerator::visit(PokeStmt* node) {
    if (node->address) node->address->accept(*this);
    if (node->value) node->value->accept(*this);
    emit(TACOp::POKE, node->address->tmpVar, node->value->tmpVar, "");
}

void TACGenerator::visit(ReturnStmt* node) {
    if (node->value) {
        node->value->accept(*this);
        emit(TACOp::RETURN, node->value->tmpVar, "", "");
    } else {
        emit(TACOp::RETURN, "", "", "");
    }
}
