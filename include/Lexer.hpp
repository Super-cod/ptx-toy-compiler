#pragma once

#include "Token.hpp"
#include <string>
#include <vector>

namespace ptxcc {

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> tokenize();

private:
    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;

    void skip_whitespace_and_comments();
    Token scan_number();
    Token scan_identifier_or_keyword();
    Token make_token(TokenType type, std::string lexeme);

    std::string source_;
    size_t start_{0};
    size_t current_{0};
    size_t line_{1};
    size_t column_{1};
    size_t token_col_start_{1};
};

} // namespace ptxcc
