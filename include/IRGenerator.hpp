#pragma once

#include "AST.hpp"
#include "IR.hpp"
#include <unordered_map>
#include <string>
#include <vector>

namespace ptxcc {

class IRGenerator : public ASTVisitor {
public:
    IRGenerator() = default;

    IRProgram generate(Program& program);

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
    Operand new_reg();
    Operand new_pred();
    Operand new_label();

    void enter_scope();
    void exit_scope();
    void set_var_reg(const std::string& name, const Operand& reg);
    Operand get_var_reg(const std::string& name);

    void emit(IROpCode op, Operand dst = Operand::make_none(),
              Operand src1 = Operand::make_none(), Operand src2 = Operand::make_none());

    size_t reg_counter_{0};
    size_t pred_counter_{0};
    size_t label_counter_{0};

    IRProgram ir_program_;
    IRKernel current_kernel_;
    Operand last_expr_result_;

    std::vector<std::unordered_map<std::string, Operand>> scope_var_map_;
};

} // namespace ptxcc
