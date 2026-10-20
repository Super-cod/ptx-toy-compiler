#include "StackVM.hpp"
#include <stdexcept>

namespace ptxcc {

StackVM::StackVM(size_t max_instructions) : max_instructions_(max_instructions) {}

int StackVM::resolve_val(const Operand& op) {
    if (op.is_imm()) {
        return op.imm_val;
    }
    if (op.is_reg()) {
        auto it = regs_.find(op.name);
        if (it != regs_.end()) {
            return it->second;
        }
        return 0; // Uninitialized registers default to 0
    }
    throw std::runtime_error("Cannot resolve operand as integer value: " + op.to_string());
}

bool StackVM::resolve_pred(const Operand& op) {
    if (op.is_pred()) {
        auto it = preds_.find(op.name);
        if (it != preds_.end()) {
            return it->second;
        }
        return false;
    }
    return resolve_val(op) != 0;
}

int StackVM::execute(const IRKernel& kernel, const std::vector<int>& args) {
    regs_.clear();
    preds_.clear();

    // Map labels to instruction indices
    std::unordered_map<std::string, size_t> label_map;
    for (size_t i = 0; i < kernel.instructions.size(); ++i) {
        if (kernel.instructions[i].opcode == IROpCode::LABEL) {
            label_map[kernel.instructions[i].dst.name] = i;
        }
    }

    // Bind parameters
    size_t param_idx = 0;
    for (const auto& inst : kernel.instructions) {
        if (inst.opcode == IROpCode::PARAM) {
            int val = (param_idx < args.size()) ? args[param_idx] : 0;
            regs_[inst.dst.name] = val;
            param_idx++;
        }
    }

    size_t pc = 0;
    size_t step_count = 0;
    int return_val = 0;

    while (pc < kernel.instructions.size()) {
        if (++step_count > max_instructions_) {
            throw std::runtime_error("StackVM Execution Timeout: Maximum instruction limit exceeded (infinite loop)");
        }

        const auto& inst = kernel.instructions[pc];
        switch (inst.opcode) {
            case IROpCode::LABEL:
            case IROpCode::PARAM:
                pc++;
                break;
            case IROpCode::MOV:
                regs_[inst.dst.name] = resolve_val(inst.src1);
                pc++;
                break;
            case IROpCode::ADD:
                regs_[inst.dst.name] = resolve_val(inst.src1) + resolve_val(inst.src2);
                pc++;
                break;
            case IROpCode::SUB:
                regs_[inst.dst.name] = resolve_val(inst.src1) - resolve_val(inst.src2);
                pc++;
                break;
            case IROpCode::MUL:
                regs_[inst.dst.name] = resolve_val(inst.src1) * resolve_val(inst.src2);
                pc++;
                break;
            case IROpCode::DIV: {
                int denom = resolve_val(inst.src2);
                if (denom == 0) throw std::runtime_error("StackVM Error: Division by zero");
                regs_[inst.dst.name] = resolve_val(inst.src1) / denom;
                pc++;
                break;
            }
            case IROpCode::MOD: {
                int denom = resolve_val(inst.src2);
                if (denom == 0) throw std::runtime_error("StackVM Error: Modulo by zero");
                regs_[inst.dst.name] = resolve_val(inst.src1) % denom;
                pc++;
                break;
            }
            case IROpCode::NEG:
                regs_[inst.dst.name] = -resolve_val(inst.src1);
                pc++;
                break;
            case IROpCode::SETP_EQ:
                preds_[inst.dst.name] = (resolve_val(inst.src1) == resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::SETP_NEQ:
                preds_[inst.dst.name] = (resolve_val(inst.src1) != resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::SETP_LT:
                preds_[inst.dst.name] = (resolve_val(inst.src1) < resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::SETP_LTE:
                preds_[inst.dst.name] = (resolve_val(inst.src1) <= resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::SETP_GT:
                preds_[inst.dst.name] = (resolve_val(inst.src1) > resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::SETP_GTE:
                preds_[inst.dst.name] = (resolve_val(inst.src1) >= resolve_val(inst.src2));
                pc++;
                break;
            case IROpCode::BRA: {
                auto it = label_map.find(inst.dst.name);
                if (it == label_map.end()) throw std::runtime_error("Undefined jump label: " + inst.dst.name);
                pc = it->second;
                break;
            }
            case IROpCode::BRA_COND: {
                if (resolve_pred(inst.src1)) {
                    auto it = label_map.find(inst.dst.name);
                    if (it == label_map.end()) throw std::runtime_error("Undefined jump label: " + inst.dst.name);
                    pc = it->second;
                } else {
                    pc++;
                }
                break;
            }
            case IROpCode::BRA_COND_NOT: {
                if (!resolve_pred(inst.src1)) {
                    auto it = label_map.find(inst.dst.name);
                    if (it == label_map.end()) throw std::runtime_error("Undefined jump label: " + inst.dst.name);
                    pc = it->second;
                } else {
                    pc++;
                }
                break;
            }
            case IROpCode::RET:
                if (!inst.dst.is_none()) {
                    return_val = resolve_val(inst.dst);
                }
                return return_val;
        }
    }

    return return_val;
}

} // namespace ptxcc
