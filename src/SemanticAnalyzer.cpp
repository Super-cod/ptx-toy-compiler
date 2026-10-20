#include "SemanticAnalyzer.hpp"
#include <sstream>

namespace ptxcc {

void SemanticAnalyzer::error(SourceLocation loc, const std::string& message) {
    std::ostringstream oss;
    oss << "Semantic Error at line " << loc.line << ", col " << loc.column << ": " << message;
    errors_.push_back(oss.str());
}

void SemanticAnalyzer::analyze(Program& program) {
    errors_.clear();
    program.accept(*this);
}

void SemanticAnalyzer::visit(Program& node) {
    for (auto& kernel : node.kernels) {
        kernel->accept(*this);
    }
}

void SemanticAnalyzer::visit(KernelDecl& node) {
    if (!symbol_table_.define(node.name, SymbolType::KERNEL)) {
        error(node.loc, "Redefinition of kernel '" + node.name + "'");
    }

    inside_kernel_ = true;
    symbol_table_.enter_scope();

    for (const auto& param : node.params) {
        if (!symbol_table_.define(param.name, SymbolType::INT)) {
            error(param.loc, "Duplicate parameter name '" + param.name + "' in kernel '" + node.name + "'");
        }
    }

    if (node.body) {
        // We visit the body statements directly to share the parameter scope
        for (auto& stmt : node.body->statements) {
            stmt->accept(*this);
        }
    }

    symbol_table_.exit_scope();
    inside_kernel_ = false;
}

void SemanticAnalyzer::visit(BlockStmt& node) {
    symbol_table_.enter_scope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    symbol_table_.exit_scope();
}

void SemanticAnalyzer::visit(VarDeclStmt& node) {
    if (!symbol_table_.define(node.var_name, SymbolType::INT)) {
        error(node.loc, "Variable '" + node.var_name + "' is already defined in this scope");
    }
    if (node.initializer) {
        node.initializer->accept(*this);
    }
}

void SemanticAnalyzer::visit(AssignStmt& node) {
    auto sym = symbol_table_.lookup(node.var_name);
    if (!sym.has_value()) {
        error(node.loc, "Assignment to undeclared variable '" + node.var_name + "'");
    }
    if (node.value) {
        node.value->accept(*this);
    }
}

void SemanticAnalyzer::visit(VariableExpr& node) {
    auto sym = symbol_table_.lookup(node.name);
    if (!sym.has_value()) {
        error(node.loc, "Use of undeclared variable '" + node.name + "'");
    }
}

void SemanticAnalyzer::visit(LiteralExpr& /*node*/) {
    // Literal values are always semantically valid
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    if (node.operand) {
        node.operand->accept(*this);
    }
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    if (node.lhs) node.lhs->accept(*this);
    if (node.rhs) node.rhs->accept(*this);
}

void SemanticAnalyzer::visit(IfStmt& node) {
    if (node.condition) node.condition->accept(*this);
    if (node.then_branch) node.then_branch->accept(*this);
    if (node.else_branch) node.else_branch->accept(*this);
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    if (node.condition) node.condition->accept(*this);
    if (node.body) node.body->accept(*this);
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    if (!inside_kernel_) {
        error(node.loc, "Return statement outside of kernel definition");
    }
    if (node.value) {
        node.value->accept(*this);
    }
}

} // namespace ptxcc
