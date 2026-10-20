#pragma once

#include <string>
#include <string_view>

namespace ptxcc {

enum class TokenType {
    // Keywords
    KW_KERNEL,
    KW_INT,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_WHILE,

    // Identifiers & Literals
    IDENTIFIER,
    INT_LITERAL,

    // Operators
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    PERCENT,    // %
    ASSIGN,     // =
    EQ_EQ,      // ==
    BANG_EQ,    // !=
    LESS,       // <
    LESS_EQ,    // <=
    GREATER,    // >
    GREATER_EQ, // >=

    // Delimiters
    LPAREN,     // (
    RPAREN,     // )
    LBRACE,     // {
    RBRACE,     // }
    COMMA,      // ,
    SEMICOLON,  // ;

    // Special
    END_OF_FILE,
    UNKNOWN
};

struct SourceLocation {
    size_t line{1};
    size_t column{1};
};

struct Token {
    TokenType type{TokenType::UNKNOWN};
    std::string lexeme;
    int int_value{0};
    SourceLocation location;

    Token() = default;
    Token(TokenType t, std::string lex, SourceLocation loc, int val = 0)
        : type(t), lexeme(std::move(lex)), int_value(val), location(loc) {}

    static std::string_view token_type_name(TokenType t);
};

} // namespace ptxcc
