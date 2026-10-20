#pragma once

#include "IR.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace ptxcc {

class Optimizer {
public:
    Optimizer() = default;

    IRProgram optimize(const IRProgram& program);
    IRKernel optimize_kernel(const IRKernel& kernel);

    struct OptStats {
        size_t constant_folds{0};
        size_t algebraic_simplifications{0};
        size_t dead_instructions_eliminated{0};
    };

    const OptStats& stats() const { return stats_; }

private:
    bool run_constant_folding(IRKernel& kernel);
    bool run_algebraic_simplification(IRKernel& kernel);
    bool run_dead_code_elimination(IRKernel& kernel);

    OptStats stats_;
};

} // namespace ptxcc
