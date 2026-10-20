#include "Optimizer.hpp"
#include <algorithm>

namespace ptxcc {

IRProgram Optimizer::optimize(const IRProgram& program) {
    IRProgram opt_prog;
    for (const auto& k : program.kernels) {
        opt_prog.kernels.push_back(optimize_kernel(k));
    }
    return opt_prog;
}

IRKernel Optimizer::optimize_kernel(const IRKernel& kernel) {
    IRKernel current = kernel;
    bool changed = true;
    size_t iterations = 0;
    const size_t max_iterations = 20;

    while (changed && iterations < max_iterations) {
        changed = false;
        iterations++;

        if (run_constant_folding(current)) {
            changed = true;
        }
        if (run_algebraic_simplification(current)) {
            changed = true;
        }
        if (run_dead_code_elimination(current)) {
            changed = true;
        }
    }

    return current;
}

bool Optimizer::run_constant_folding(IRKernel& kernel) {
    bool changed = false;
    std::unordered_map<std::string, int> const_map; // maps %r -> constant value

    for (auto& inst : kernel.instructions) {
        // Clear constant map across control flow boundaries (labels or branches)
        if (inst.opcode == IROpCode::LABEL || inst.opcode == IROpCode::BRA ||
            inst.opcode == IROpCode::BRA_COND || inst.opcode == IROpCode::BRA_COND_NOT) {
            const_map.clear();
            continue;
        }

        // Constant propagation: replace register with immediate if known
        if (inst.src1.is_reg()) {
            auto it = const_map.find(inst.src1.name);
            if (it != const_map.end()) {
                inst.src1 = Operand::make_imm(it->second);
                changed = true;
            }
        }
        if (inst.src2.is_reg()) {
            auto it = const_map.find(inst.src2.name);
            if (it != const_map.end()) {
                inst.src2 = Operand::make_imm(it->second);
                changed = true;
            }
        }

        // If both src1 and src2 are immediates, fold!
        if (inst.src1.is_imm() && inst.src2.is_imm()) {
            int v1 = inst.src1.imm_val;
            int v2 = inst.src2.imm_val;
            int res = 0;
            bool folded = false;

            switch (inst.opcode) {
                case IROpCode::ADD: res = v1 + v2; folded = true; break;
                case IROpCode::SUB: res = v1 - v2; folded = true; break;
                case IROpCode::MUL: res = v1 * v2; folded = true; break;
                case IROpCode::DIV:
                    if (v2 != 0) { res = v1 / v2; folded = true; }
                    break;
                case IROpCode::MOD:
                    if (v2 != 0) { res = v1 % v2; folded = true; }
                    break;
                default:
                    break;
            }

            if (folded) {
                inst.opcode = IROpCode::MOV;
                inst.src1 = Operand::make_imm(res);
                inst.src2 = Operand::make_none();
                stats_.constant_folds++;
                changed = true;
            }
        } else if (inst.opcode == IROpCode::NEG && inst.src1.is_imm()) {
            inst.opcode = IROpCode::MOV;
            inst.src1 = Operand::make_imm(-inst.src1.imm_val);
            stats_.constant_folds++;
            changed = true;
        }

        // Record or invalidate in const_map
        if (inst.opcode == IROpCode::MOV && inst.dst.is_reg() && inst.src1.is_imm()) {
            const_map[inst.dst.name] = inst.src1.imm_val;
        } else if (inst.dst.is_reg()) {
            const_map.erase(inst.dst.name);
        }
    }

    return changed;
}

bool Optimizer::run_algebraic_simplification(IRKernel& kernel) {
    bool changed = false;

    for (auto& inst : kernel.instructions) {
        // x + 0 -> x
        if (inst.opcode == IROpCode::ADD) {
            if (inst.src2.is_imm() && inst.src2.imm_val == 0) {
                inst.opcode = IROpCode::MOV;
                // dst = src1
                inst.src2 = Operand::make_none();
                stats_.algebraic_simplifications++;
                changed = true;
            } else if (inst.src1.is_imm() && inst.src1.imm_val == 0) {
                inst.opcode = IROpCode::MOV;
                inst.src1 = inst.src2;
                inst.src2 = Operand::make_none();
                stats_.algebraic_simplifications++;
                changed = true;
            }
        }
        // x - 0 -> x
        else if (inst.opcode == IROpCode::SUB) {
            if (inst.src2.is_imm() && inst.src2.imm_val == 0) {
                inst.opcode = IROpCode::MOV;
                inst.src2 = Operand::make_none();
                stats_.algebraic_simplifications++;
                changed = true;
            }
        }
        // x * 1 -> x, x * 0 -> 0
        else if (inst.opcode == IROpCode::MUL) {
            if (inst.src2.is_imm()) {
                if (inst.src2.imm_val == 1) {
                    inst.opcode = IROpCode::MOV;
                    inst.src2 = Operand::make_none();
                    stats_.algebraic_simplifications++;
                    changed = true;
                } else if (inst.src2.imm_val == 0) {
                    inst.opcode = IROpCode::MOV;
                    inst.src1 = Operand::make_imm(0);
                    inst.src2 = Operand::make_none();
                    stats_.algebraic_simplifications++;
                    changed = true;
                }
            } else if (inst.src1.is_imm()) {
                if (inst.src1.imm_val == 1) {
                    inst.opcode = IROpCode::MOV;
                    inst.src1 = inst.src2;
                    inst.src2 = Operand::make_none();
                    stats_.algebraic_simplifications++;
                    changed = true;
                } else if (inst.src1.imm_val == 0) {
                    inst.opcode = IROpCode::MOV;
                    inst.src1 = Operand::make_imm(0);
                    inst.src2 = Operand::make_none();
                    stats_.algebraic_simplifications++;
                    changed = true;
                }
            }
        }
        // x / 1 -> x
        else if (inst.opcode == IROpCode::DIV) {
            if (inst.src2.is_imm() && inst.src2.imm_val == 1) {
                inst.opcode = IROpCode::MOV;
                inst.src2 = Operand::make_none();
                stats_.algebraic_simplifications++;
                changed = true;
            }
        }
    }

    return changed;
}

bool Optimizer::run_dead_code_elimination(IRKernel& kernel) {
    bool changed = false;

    // Pass 1: Eliminate unreachable instructions following unconditional BRA or RET before the next LABEL
    std::vector<IRInstruction> reachable;
    bool unreachable = false;

    for (const auto& inst : kernel.instructions) {
        if (inst.opcode == IROpCode::LABEL) {
            unreachable = false;
        }

        if (!unreachable) {
            reachable.push_back(inst);
        } else {
            stats_.dead_instructions_eliminated++;
            changed = true;
        }

        if (inst.opcode == IROpCode::BRA || inst.opcode == IROpCode::RET) {
            unreachable = true;
        }
    }
    kernel.instructions = std::move(reachable);

    // Pass 2: Eliminate unused register definitions (registers that are written but never read anywhere)
    std::unordered_set<std::string> used_registers;
    for (const auto& inst : kernel.instructions) {
        if (inst.src1.is_reg()) used_registers.insert(inst.src1.name);
        if (inst.src1.is_pred()) used_registers.insert(inst.src1.name);
        if (inst.src2.is_reg()) used_registers.insert(inst.src2.name);
        if (inst.src2.is_pred()) used_registers.insert(inst.src2.name);

        // RET or branch destinations can use registers/predicates
        if (inst.opcode == IROpCode::RET && inst.dst.is_reg()) {
            used_registers.insert(inst.dst.name);
        }
    }

    std::vector<IRInstruction> active_instructions;
    for (const auto& inst : kernel.instructions) {
        // If it writes to a register or predicate that is never read anywhere, and has no side effects, eliminate
        bool is_pure_def = (inst.opcode == IROpCode::MOV || inst.opcode == IROpCode::ADD ||
                            inst.opcode == IROpCode::SUB || inst.opcode == IROpCode::MUL ||
                            inst.opcode == IROpCode::DIV || inst.opcode == IROpCode::MOD ||
                            inst.opcode == IROpCode::NEG || inst.opcode == IROpCode::SETP_EQ ||
                            inst.opcode == IROpCode::SETP_NEQ || inst.opcode == IROpCode::SETP_LT ||
                            inst.opcode == IROpCode::SETP_LTE || inst.opcode == IROpCode::SETP_GT ||
                            inst.opcode == IROpCode::SETP_GTE);

        if (is_pure_def && (inst.dst.is_reg() || inst.dst.is_pred())) {
            if (used_registers.find(inst.dst.name) == used_registers.end()) {
                // Not used anywhere!
                stats_.dead_instructions_eliminated++;
                changed = true;
                continue;
            }
        }
        active_instructions.push_back(inst);
    }
    kernel.instructions = std::move(active_instructions);

    return changed;
}

} // namespace ptxcc
