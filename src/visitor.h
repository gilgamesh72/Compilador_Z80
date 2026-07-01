#pragma once

using namespace std;

// Declaraciones previas de los nodos del AST
class NumExpr;
class IdExpr;
class PeekExpr;
class BinaryExpr;
class Block;
class VarDecl;
class Assign;
class IfStmt;
class WhileStmt;
class PokeStmt;
class ReturnStmt;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(NumExpr* node) = 0;
    virtual void visit(IdExpr* node) = 0;
    virtual void visit(PeekExpr* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    
    virtual void visit(Block* node) = 0;
    virtual void visit(VarDecl* node) = 0;
    virtual void visit(Assign* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(WhileStmt* node) = 0;
    virtual void visit(PokeStmt* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
};
