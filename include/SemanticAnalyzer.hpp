#pragma once

#include "AST.hpp"
#include "SymbolTable.hpp"
#include <string>
#include <vector>

namespace ptxcc {

class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer() = default;

    void analyze(Program& program);

    const std::vector<std::string>& errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }

    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(LiteralExpr& node) override;
    void visit(VariableExpr& node) override;
    void visit(BlockStmt& node) override;
    void visit(VarDeclStmt& node) override;
    void visit(AssignStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(KernelDecl& node) override;
    void visit(Program& node) override;

private:
    void error(SourceLocation loc, const std::string& message);

    SymbolTable symbol_table_;
    std::vector<std::string> errors_;
    bool inside_kernel_{false};
};

} // namespace ptxcc
