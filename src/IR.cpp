#include "IR.hpp"
#include <sstream>

namespace ptxcc {

std::string Operand::to_string() const {
    switch (kind) {
        case OperandKind::REG:
        case OperandKind::PRED:
        case OperandKind::LABEL:
            return name;
        case OperandKind::IMM:
            return std::to_string(imm_val);
        case OperandKind::NONE:
            return "";
    }
    return "";
}

std::string IRInstruction::to_string() const {
    std::ostringstream oss;
    switch (opcode) {
        case IROpCode::LABEL:
            oss << dst.to_string() << ":";
            return oss.str();
        case IROpCode::MOV:
            oss << "  mov " << dst.to_string() << ", " << src1.to_string();
            break;
        case IROpCode::ADD:
            oss << "  add " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SUB:
            oss << "  sub " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::MUL:
            oss << "  mul " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::DIV:
            oss << "  div " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::MOD:
            oss << "  rem " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::NEG:
            oss << "  neg " << dst.to_string() << ", " << src1.to_string();
            break;
        case IROpCode::SETP_EQ:
            oss << "  setp.eq " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SETP_NEQ:
            oss << "  setp.ne " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SETP_LT:
            oss << "  setp.lt " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SETP_LTE:
            oss << "  setp.le " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SETP_GT:
            oss << "  setp.gt " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::SETP_GTE:
            oss << "  setp.ge " << dst.to_string() << ", " << src1.to_string() << ", " << src2.to_string();
            break;
        case IROpCode::BRA:
            oss << "  bra " << dst.to_string();
            break;
        case IROpCode::BRA_COND:
            oss << "  @ " << src1.to_string() << " bra " << dst.to_string();
            break;
        case IROpCode::BRA_COND_NOT:
            oss << "  @!" << src1.to_string() << " bra " << dst.to_string();
            break;
        case IROpCode::PARAM:
            oss << "  param " << dst.to_string();
            break;
        case IROpCode::RET:
            if (!dst.is_none()) {
                oss << "  ret " << dst.to_string();
            } else {
                oss << "  ret";
            }
            break;
    }
    return oss.str();
}

std::string IRKernel::to_string() const {
    std::ostringstream oss;
    oss << "kernel " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << params[i];
    }
    oss << ") {\n";
    for (const auto& inst : instructions) {
        oss << inst.to_string() << "\n";
    }
    oss << "}\n";
    return oss.str();
}

std::string IRProgram::to_string() const {
    std::ostringstream oss;
    for (const auto& k : kernels) {
        oss << k.to_string() << "\n";
    }
    return oss.str();
}

} // namespace ptxcc
