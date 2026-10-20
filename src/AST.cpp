#include "AST.hpp"

namespace ptxcc {

void BinaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void LiteralExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }

void BlockStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarDeclStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AssignStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }

void KernelDecl::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void Program::accept(ASTVisitor& visitor) { visitor.visit(*this); }

} // namespace ptxcc
