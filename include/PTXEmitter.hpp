#pragma once

#include "IR.hpp"
#include <string>

namespace ptxcc {

class PTXEmitter {
public:
    PTXEmitter() = default;

    std::string emit(const IRProgram& program);
    std::string emit_kernel(const IRKernel& kernel);

private:
    std::string format_operand(const Operand& op);
};

} // namespace ptxcc
