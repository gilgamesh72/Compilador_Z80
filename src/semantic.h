#pragma once
#include "ast.h"
#include "visitor.h"

using namespace std;

class SemanticAnalyzer : public ASTVisitor {
private:
    SymbolTable* currentEnv;
    bool hasError;

public:
    SemanticAnalyzer();
    ~SemanticAnalyzer();

    void analyze(ASTNode* root);
    bool hasErrors() const { return hasError; }

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
