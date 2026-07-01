#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "visitor.h"

using namespace std;

enum class TACOp {
    ADD, SUB, LT, GT, EQ, AND, OR,
    ASSIGN,
    POKE, PEEK,
    JUMP, JUMPF,
    LABEL,
    RETURN,
    PARAM, CALL // Por si el lenguaje crece
};

struct TACInstr {
    TACOp op;
    string arg1;
    string arg2;
    string result;

    void print() const;
};

class TACGenerator : public ASTVisitor {
private:
    int tempCount;
    int labelCount;
    vector<TACInstr> instructions;

    string newTemp();
    string newLabel();
    void emit(TACOp op, const string& arg1, const string& arg2, const string& result);

public:
    TACGenerator();
    
    void generate(ASTNode* root);
    const vector<TACInstr>& getInstructions() const { return instructions; }
    void printAll() const;

    void visit(NumExpr* node) override;
    void visit(IdExpr* node) override;
    void visit(PeekExpr* node) override;
    void visit(BinaryExpr* node) override;
    
    void visit(Block* node) override;
    void visit(VarDecl* node) override;
    void visit(Assign* node) override;
    void visit(IfStmt* node) override;
    void visit(WhileStmt* node) override;
    void visit(PokeStmt* node) override;
    void visit(ReturnStmt* node) override;
};
