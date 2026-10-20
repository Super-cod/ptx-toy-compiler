#include "Parser.hpp"
#include <stdexcept>
#include <sstream>

namespace ptxcc {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::END_OF_FILE;
}

const Token& Parser::advance() {
    if (!is_at_end()) current_++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    std::ostringstream oss;
    oss << "Syntax Error at line " << peek().location.line << ", col " << peek().location.column
        << ": " << message << " (got '" << peek().lexeme << "')";
    throw std::runtime_error(oss.str());
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    while (!is_at_end()) {
        program->kernels.push_back(parse_kernel());
    }
    return program;
}

std::unique_ptr<KernelDecl> Parser::parse_kernel() {
    SourceLocation loc = peek().location;
    consume(TokenType::KW_KERNEL, "Expected 'kernel' keyword");
    const Token& name_tok = consume(TokenType::IDENTIFIER, "Expected kernel name identifier");
    consume(TokenType::LPAREN, "Expected '(' after kernel name");

    std::vector<ParamDecl> params;
    if (!check(TokenType::RPAREN)) {
        params = parse_param_list();
    }
    consume(TokenType::RPAREN, "Expected ')' after parameter list");

    auto body = parse_block();
    return std::make_unique<KernelDecl>(name_tok.lexeme, std::move(params), std::move(body), loc);
}

std::vector<ParamDecl> Parser::parse_param_list() {
    std::vector<ParamDecl> params;
    do {
        SourceLocation loc = peek().location;
        consume(TokenType::KW_INT, "Expected 'int' parameter type");
        const Token& name = consume(TokenType::IDENTIFIER, "Expected parameter name");
        params.push_back(ParamDecl{"int", name.lexeme, loc});
    } while (match(TokenType::COMMA));
    return params;
}

std::unique_ptr<BlockStmt> Parser::parse_block() {
    SourceLocation loc = peek().location;
    consume(TokenType::LBRACE, "Expected '{' to begin block");
    auto block = std::make_unique<BlockStmt>(loc);
    while (!check(TokenType::RBRACE) && !is_at_end()) {
        block->statements.push_back(parse_statement());
    }
    consume(TokenType::RBRACE, "Expected '}' to close block");
    return block;
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    if (check(TokenType::KW_INT)) {
        return parse_var_decl();
    }
    if (check(TokenType::KW_IF)) {
        return parse_if();
    }
    if (check(TokenType::KW_WHILE)) {
        return parse_while();
    }
    if (check(TokenType::KW_RETURN)) {
        return parse_return();
    }
    if (check(TokenType::LBRACE)) {
        return parse_block();
    }
    return parse_assignment();
}

std::unique_ptr<Stmt> Parser::parse_var_decl() {
    SourceLocation loc = peek().location;
    consume(TokenType::KW_INT, "Expected 'int'");
    const Token& name_tok = consume(TokenType::IDENTIFIER, "Expected variable name in declaration");

    std::unique_ptr<Expr> init = nullptr;
    if (match(TokenType::ASSIGN)) {
        init = parse_expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return std::make_unique<VarDeclStmt>("int", name_tok.lexeme, std::move(init), loc);
}

std::unique_ptr<Stmt> Parser::parse_assignment() {
    SourceLocation loc = peek().location;
    const Token& name_tok = consume(TokenType::IDENTIFIER, "Expected variable name in assignment");
    consume(TokenType::ASSIGN, "Expected '=' in assignment");
    auto val = parse_expression();
    consume(TokenType::SEMICOLON, "Expected ';' after assignment");
    return std::make_unique<AssignStmt>(name_tok.lexeme, std::move(val), loc);
}

std::unique_ptr<Stmt> Parser::parse_if() {
    SourceLocation loc = peek().location;
    consume(TokenType::KW_IF, "Expected 'if'");
    consume(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after if condition");

    auto then_branch = parse_statement();
    std::unique_ptr<Stmt> else_branch = nullptr;
    if (match(TokenType::KW_ELSE)) {
        else_branch = parse_statement();
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(then_branch), std::move(else_branch), loc);
}

std::unique_ptr<Stmt> Parser::parse_while() {
    SourceLocation loc = peek().location;
    consume(TokenType::KW_WHILE, "Expected 'while'");
    consume(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parse_expression();
    consume(TokenType::RPAREN, "Expected ')' after while condition");

    auto body = parse_statement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::parse_return() {
    SourceLocation loc = peek().location;
    consume(TokenType::KW_RETURN, "Expected 'return'");
    std::unique_ptr<Expr> val = nullptr;
    if (!check(TokenType::SEMICOLON)) {
        val = parse_expression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after return");
    return std::make_unique<ReturnStmt>(std::move(val), loc);
}

std::unique_ptr<Expr> Parser::parse_expression() {
    return parse_equality();
}

std::unique_ptr<Expr> Parser::parse_equality() {
    auto expr = parse_comparison();
    while (match(TokenType::EQ_EQ) || match(TokenType::BANG_EQ)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op = (op_type == TokenType::EQ_EQ) ? BinaryOp::EQ : BinaryOp::NEQ;
        auto rhs = parse_comparison();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
    auto expr = parse_term();
    while (match(TokenType::LESS) || match(TokenType::LESS_EQ) ||
           match(TokenType::GREATER) || match(TokenType::GREATER_EQ)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op;
        switch (op_type) {
            case TokenType::LESS: op = BinaryOp::LT; break;
            case TokenType::LESS_EQ: op = BinaryOp::LTE; break;
            case TokenType::GREATER: op = BinaryOp::GT; break;
            case TokenType::GREATER_EQ: op = BinaryOp::GTE; break;
            default: op = BinaryOp::LT; break;
        }
        auto rhs = parse_term();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_term() {
    auto expr = parse_factor();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op = (op_type == TokenType::PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
        auto rhs = parse_factor();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_factor() {
    auto expr = parse_unary();
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op;
        switch (op_type) {
            case TokenType::STAR: op = BinaryOp::MUL; break;
            case TokenType::SLASH: op = BinaryOp::DIV; break;
            case TokenType::PERCENT: op = BinaryOp::MOD; break;
            default: op = BinaryOp::MUL; break;
        }
        auto rhs = parse_unary();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(rhs), loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (match(TokenType::MINUS)) {
        SourceLocation loc = previous().location;
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(UnaryOp::NEG, std::move(operand), loc);
    }
    return parse_primary();
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (match(TokenType::INT_LITERAL)) {
        return std::make_unique<LiteralExpr>(previous().int_value, previous().location);
    }
    if (match(TokenType::IDENTIFIER)) {
        return std::make_unique<VariableExpr>(previous().lexeme, previous().location);
    }
    if (match(TokenType::LPAREN)) {
        auto expr = parse_expression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    std::ostringstream oss;
    oss << "Syntax Error at line " << peek().location.line << ", col " << peek().location.column
        << ": Unexpected token '" << peek().lexeme << "'";
    throw std::runtime_error(oss.str());
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::KW_KERNEL:
            case TokenType::KW_INT:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_RETURN:
                return;
            default:
                advance();
        }
    }
}

} // namespace ptxcc
