#pragma once

#include "Token.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ptxcc {

class ASTVisitor;

struct ASTNode {
    SourceLocation loc;
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

// Expressions
struct Expr : public ASTNode {};

enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD,
    EQ, NEQ, LT, LTE, GT, GTE
};

struct BinaryExpr : public Expr {
    BinaryOp op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;

    BinaryExpr(BinaryOp op, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r, SourceLocation loc)
        : op(op), lhs(std::move(l)), rhs(std::move(r)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

enum class UnaryOp {
    NEG
};

struct UnaryExpr : public Expr {
    UnaryOp op;
    std::unique_ptr<Expr> operand;

    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> operand, SourceLocation loc)
        : op(op), operand(std::move(operand)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct LiteralExpr : public Expr {
    int value;

    LiteralExpr(int val, SourceLocation loc) : value(val) { this->loc = loc; }
    void accept(ASTVisitor& visitor) override;
};

struct VariableExpr : public Expr {
    std::string name;

    VariableExpr(std::string name, SourceLocation loc) : name(std::move(name)) { this->loc = loc; }
    void accept(ASTVisitor& visitor) override;
};

// Statements
struct Stmt : public ASTNode {};

struct BlockStmt : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;

    explicit BlockStmt(SourceLocation loc) { this->loc = loc; }
    void accept(ASTVisitor& visitor) override;
};

struct VarDeclStmt : public Stmt {
    std::string type_name;
    std::string var_name;
    std::unique_ptr<Expr> initializer; // optional, may be null

    VarDeclStmt(std::string type, std::string name, std::unique_ptr<Expr> init, SourceLocation loc)
        : type_name(std::move(type)), var_name(std::move(name)), initializer(std::move(init)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct AssignStmt : public Stmt {
    std::string var_name;
    std::unique_ptr<Expr> value;

    AssignStmt(std::string name, std::unique_ptr<Expr> val, SourceLocation loc)
        : var_name(std::move(name)), value(std::move(val)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch; // optional

    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> then_b, std::unique_ptr<Stmt> else_b, SourceLocation loc)
        : condition(std::move(cond)), then_branch(std::move(then_b)), else_branch(std::move(else_b)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;

    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body, SourceLocation loc)
        : condition(std::move(cond)), body(std::move(body)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct ReturnStmt : public Stmt {
    std::unique_ptr<Expr> value; // optional

    ReturnStmt(std::unique_ptr<Expr> val, SourceLocation loc)
        : value(std::move(val)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct ParamDecl {
    std::string type;
    std::string name;
    SourceLocation loc;
};

struct KernelDecl : public ASTNode {
    std::string name;
    std::vector<ParamDecl> params;
    std::unique_ptr<BlockStmt> body;

    KernelDecl(std::string name, std::vector<ParamDecl> params, std::unique_ptr<BlockStmt> body, SourceLocation loc)
        : name(std::move(name)), params(std::move(params)), body(std::move(body)) { this->loc = loc; }

    void accept(ASTVisitor& visitor) override;
};

struct Program : public ASTNode {
    std::vector<std::unique_ptr<KernelDecl>> kernels;

    Program() = default;
    void accept(ASTVisitor& visitor) override;
};

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0;
    virtual void visit(LiteralExpr& node) = 0;
    virtual void visit(VariableExpr& node) = 0;
    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(AssignStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(KernelDecl& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace ptxcc
