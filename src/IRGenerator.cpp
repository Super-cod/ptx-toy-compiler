#include "IRGenerator.hpp"
#include <stdexcept>

namespace ptxcc {

Operand IRGenerator::new_reg() {
    std::string name = "%r" + std::to_string(reg_counter_++);
    return Operand::make_reg(name);
}

Operand IRGenerator::new_pred() {
    std::string name = "%p" + std::to_string(pred_counter_++);
    return Operand::make_pred(name);
}

Operand IRGenerator::new_label() {
    std::string name = "L" + std::to_string(label_counter_++);
    return Operand::make_label(name);
}

void IRGenerator::enter_scope() {
    scope_var_map_.emplace_back();
}

void IRGenerator::exit_scope() {
    if (!scope_var_map_.empty()) {
        scope_var_map_.pop_back();
    }
}

void IRGenerator::set_var_reg(const std::string& name, const Operand& reg) {
    if (!scope_var_map_.empty()) {
        scope_var_map_.back()[name] = reg;
    }
}

Operand IRGenerator::get_var_reg(const std::string& name) {
    for (auto it = scope_var_map_.rbegin(); it != scope_var_map_.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) {
            return f->second;
        }
    }
    throw std::runtime_error("Variable '" + name + "' not found in IR register map");
}

void IRGenerator::emit(IROpCode op, Operand dst, Operand src1, Operand src2) {
    current_kernel_.instructions.push_back(IRInstruction{op, std::move(dst), std::move(src1), std::move(src2)});
}

IRProgram IRGenerator::generate(Program& program) {
    ir_program_.kernels.clear();
    program.accept(*this);
    return ir_program_;
}

void IRGenerator::visit(Program& node) {
    for (auto& kernel : node.kernels) {
        kernel->accept(*this);
    }
}

void IRGenerator::visit(KernelDecl& node) {
    reg_counter_ = 0;
    pred_counter_ = 0;
    label_counter_ = 0;

    current_kernel_ = IRKernel{};
    current_kernel_.name = node.name;

    enter_scope();

    for (const auto& param : node.params) {
        current_kernel_.params.push_back(param.name);
        Operand p_reg = new_reg();
        set_var_reg(param.name, p_reg);
        emit(IROpCode::PARAM, p_reg);
    }

    if (node.body) {
        for (auto& stmt : node.body->statements) {
            stmt->accept(*this);
        }
    }

    exit_scope();

    current_kernel_.num_regs = reg_counter_;
    current_kernel_.num_preds = pred_counter_;
    ir_program_.kernels.push_back(std::move(current_kernel_));
}

void IRGenerator::visit(BlockStmt& node) {
    enter_scope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    exit_scope();
}

void IRGenerator::visit(VarDeclStmt& node) {
    Operand var_reg = new_reg();
    set_var_reg(node.var_name, var_reg);

    if (node.initializer) {
        node.initializer->accept(*this);
        emit(IROpCode::MOV, var_reg, last_expr_result_);
    } else {
        // default initialize to 0
        emit(IROpCode::MOV, var_reg, Operand::make_imm(0));
    }
}

void IRGenerator::visit(AssignStmt& node) {
    Operand var_reg = get_var_reg(node.var_name);
    node.value->accept(*this);
    emit(IROpCode::MOV, var_reg, last_expr_result_);
}

void IRGenerator::visit(LiteralExpr& node) {
    last_expr_result_ = Operand::make_imm(node.value);
}

void IRGenerator::visit(VariableExpr& node) {
    last_expr_result_ = get_var_reg(node.name);
}

void IRGenerator::visit(UnaryExpr& node) {
    node.operand->accept(*this);
    Operand src = last_expr_result_;
    Operand dst = new_reg();
    emit(IROpCode::NEG, dst, src);
    last_expr_result_ = dst;
}

void IRGenerator::visit(BinaryExpr& node) {
    node.lhs->accept(*this);
    Operand lhs = last_expr_result_;

    node.rhs->accept(*this);
    Operand rhs = last_expr_result_;

    switch (node.op) {
        case BinaryOp::ADD: {
            Operand dst = new_reg();
            emit(IROpCode::ADD, dst, lhs, rhs);
            last_expr_result_ = dst;
            break;
        }
        case BinaryOp::SUB: {
            Operand dst = new_reg();
            emit(IROpCode::SUB, dst, lhs, rhs);
            last_expr_result_ = dst;
            break;
        }
        case BinaryOp::MUL: {
            Operand dst = new_reg();
            emit(IROpCode::MUL, dst, lhs, rhs);
            last_expr_result_ = dst;
            break;
        }
        case BinaryOp::DIV: {
            Operand dst = new_reg();
            emit(IROpCode::DIV, dst, lhs, rhs);
            last_expr_result_ = dst;
            break;
        }
        case BinaryOp::MOD: {
            Operand dst = new_reg();
            emit(IROpCode::MOD, dst, lhs, rhs);
            last_expr_result_ = dst;
            break;
        }
        case BinaryOp::EQ: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_EQ, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
        case BinaryOp::NEQ: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_NEQ, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
        case BinaryOp::LT: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_LT, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
        case BinaryOp::LTE: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_LTE, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
        case BinaryOp::GT: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_GT, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
        case BinaryOp::GTE: {
            Operand pred = new_pred();
            emit(IROpCode::SETP_GTE, pred, lhs, rhs);
            last_expr_result_ = pred;
            break;
        }
    }
}

void IRGenerator::visit(IfStmt& node) {
    node.condition->accept(*this);
    Operand cond = last_expr_result_;

    // If cond is not a predicate register (e.g. while (x)), compare with 0
    if (!cond.is_pred()) {
        Operand p = new_pred();
        emit(IROpCode::SETP_NEQ, p, cond, Operand::make_imm(0));
        cond = p;
    }

    Operand else_label = new_label();
    Operand end_label = new_label();

    if (node.else_branch) {
        emit(IROpCode::BRA_COND_NOT, else_label, cond);
        node.then_branch->accept(*this);
        emit(IROpCode::BRA, end_label);
        emit(IROpCode::LABEL, else_label);
        node.else_branch->accept(*this);
        emit(IROpCode::LABEL, end_label);
    } else {
        emit(IROpCode::BRA_COND_NOT, end_label, cond);
        node.then_branch->accept(*this);
        emit(IROpCode::LABEL, end_label);
    }
}

void IRGenerator::visit(WhileStmt& node) {
    Operand start_label = new_label();
    Operand end_label = new_label();

    emit(IROpCode::LABEL, start_label);

    node.condition->accept(*this);
    Operand cond = last_expr_result_;

    if (!cond.is_pred()) {
        Operand p = new_pred();
        emit(IROpCode::SETP_NEQ, p, cond, Operand::make_imm(0));
        cond = p;
    }

    emit(IROpCode::BRA_COND_NOT, end_label, cond);

    if (node.body) {
        node.body->accept(*this);
    }

    emit(IROpCode::BRA, start_label);
    emit(IROpCode::LABEL, end_label);
}

void IRGenerator::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);
        emit(IROpCode::RET, last_expr_result_);
    } else {
        emit(IROpCode::RET);
    }
}

} // namespace ptxcc
