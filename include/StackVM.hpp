#pragma once

#include "IR.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace ptxcc {

class StackVM {
public:
    explicit StackVM(size_t max_instructions = 100000);

    int execute(const IRKernel& kernel, const std::vector<int>& args = {});

private:
    int resolve_val(const Operand& op);
    bool resolve_pred(const Operand& op);

    size_t max_instructions_;
    std::unordered_map<std::string, int> regs_;
    std::unordered_map<std::string, bool> preds_;
};

} // namespace ptxcc
