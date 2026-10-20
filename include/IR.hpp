#pragma once

#include <string>
#include <vector>
#include <variant>
#include <optional>

namespace ptxcc {

enum class IROpCode {
    MOV,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    SETP_EQ,
    SETP_NEQ,
    SETP_LT,
    SETP_LTE,
    SETP_GT,
    SETP_GTE,
    BRA,
    BRA_COND,
    BRA_COND_NOT,
    LABEL,
    PARAM,
    RET
};

enum class OperandKind {
    REG,
    PRED,
    IMM,
    LABEL,
    NONE
};

struct Operand {
    OperandKind kind{OperandKind::NONE};
    std::string name; // register name (e.g., "%r0", "%p0") or label name ("L1")
    int imm_val{0};

    static Operand make_reg(std::string r) {
        Operand op;
        op.kind = OperandKind::REG;
        op.name = std::move(r);
        return op;
    }

    static Operand make_pred(std::string p) {
        Operand op;
        op.kind = OperandKind::PRED;
        op.name = std::move(p);
        return op;
    }

    static Operand make_imm(int val) {
        Operand op;
        op.kind = OperandKind::IMM;
        op.imm_val = val;
        op.name = std::to_string(val);
        return op;
    }

    static Operand make_label(std::string l) {
        Operand op;
        op.kind = OperandKind::LABEL;
        op.name = std::move(l);
        return op;
    }

    static Operand make_none() {
        return Operand{};
    }

    bool is_imm() const { return kind == OperandKind::IMM; }
    bool is_reg() const { return kind == OperandKind::REG; }
    bool is_pred() const { return kind == OperandKind::PRED; }
    bool is_label() const { return kind == OperandKind::LABEL; }
    bool is_none() const { return kind == OperandKind::NONE; }

    std::string to_string() const;
    bool operator==(const Operand& other) const = default;
};

struct IRInstruction {
    IROpCode opcode;
    Operand dst;
    Operand src1;
    Operand src2;

    std::string to_string() const;
};

struct IRKernel {
    std::string name;
    std::vector<std::string> params;
    std::vector<IRInstruction> instructions;
    size_t num_regs{0};
    size_t num_preds{0};

    std::string to_string() const;
};

struct IRProgram {
    std::vector<IRKernel> kernels;

    std::string to_string() const;
};

} // namespace ptxcc
