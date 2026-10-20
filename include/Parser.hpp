#pragma once

#include "AST.hpp"
#include "Token.hpp"
#include <string>
#include <vector>

namespace ptxcc {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parse();

private:
    const Token& peek() const;
    const Token& previous() const;
    bool is_at_end() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);

    // Parsing methods
    std::unique_ptr<KernelDecl> parse_kernel();
    std::vector<ParamDecl> parse_param_list();
    std::unique_ptr<BlockStmt> parse_block();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_var_decl();
    std::unique_ptr<Stmt> parse_assignment();
    std::unique_ptr<Stmt> parse_if();
    std::unique_ptr<Stmt> parse_while();
    std::unique_ptr<Stmt> parse_return();

    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_primary();

    void synchronize();

    std::vector<Token> tokens_;
    size_t current_{0};
};

} // namespace ptxcc
