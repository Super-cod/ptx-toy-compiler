#include "Lexer.hpp"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace ptxcc {

std::string_view Token::token_type_name(TokenType t) {
    switch (t) {
        case TokenType::KW_KERNEL:    return "KW_KERNEL";
        case TokenType::KW_INT:       return "KW_INT";
        case TokenType::KW_RETURN:    return "KW_RETURN";
        case TokenType::KW_IF:        return "KW_IF";
        case TokenType::KW_ELSE:      return "KW_ELSE";
        case TokenType::KW_WHILE:     return "KW_WHILE";
        case TokenType::IDENTIFIER:   return "IDENTIFIER";
        case TokenType::INT_LITERAL:  return "INT_LITERAL";
        case TokenType::PLUS:         return "PLUS";
        case TokenType::MINUS:        return "MINUS";
        case TokenType::STAR:         return "STAR";
        case TokenType::SLASH:        return "SLASH";
        case TokenType::PERCENT:      return "PERCENT";
        case TokenType::ASSIGN:       return "ASSIGN";
        case TokenType::EQ_EQ:        return "EQ_EQ";
        case TokenType::BANG_EQ:      return "BANG_EQ";
        case TokenType::LESS:         return "LESS";
        case TokenType::LESS_EQ:      return "LESS_EQ";
        case TokenType::GREATER:      return "GREATER";
        case TokenType::GREATER_EQ:   return "GREATER_EQ";
        case TokenType::LPAREN:       return "LPAREN";
        case TokenType::RPAREN:       return "RPAREN";
        case TokenType::LBRACE:       return "LBRACE";
        case TokenType::RBRACE:       return "RBRACE";
        case TokenType::COMMA:        return "COMMA";
        case TokenType::SEMICOLON:    return "SEMICOLON";
        case TokenType::END_OF_FILE:  return "END_OF_FILE";
        case TokenType::UNKNOWN:      return "UNKNOWN";
    }
    return "UNKNOWN";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::is_at_end() const {
    return current_ >= source_.size();
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.size()) return '\0';
    return source_[current_ + 1];
}

char Lexer::advance() {
    char c = source_[current_++];
    column_++;
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (source_[current_] != expected) return false;
    current_++;
    column_++;
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
        } else if (c == '\n') {
            advance();
            line_++;
            column_ = 1;
        } else if (c == '/' && peek_next() == '/') {
            // Single line comment
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::make_token(TokenType type, std::string lexeme) {
    return Token(type, std::move(lexeme), SourceLocation{line_, token_col_start_});
}

Token Lexer::scan_number() {
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }
    std::string lexeme = source_.substr(start_, current_ - start_);
    int val = 0;
    try {
        val = std::stoi(lexeme);
    } catch (...) {
        throw std::runtime_error("Integer literal out of range: " + lexeme +
                                 " at line " + std::to_string(line_) + ", col " + std::to_string(token_col_start_));
    }
    Token tok(TokenType::INT_LITERAL, lexeme, SourceLocation{line_, token_col_start_}, val);
    return tok;
}

Token Lexer::scan_identifier_or_keyword() {
    while (!is_at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        advance();
    }
    std::string lexeme = source_.substr(start_, current_ - start_);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"kernel", TokenType::KW_KERNEL},
        {"int",    TokenType::KW_INT},
        {"return", TokenType::KW_RETURN},
        {"if",     TokenType::KW_IF},
        {"else",   TokenType::KW_ELSE},
        {"while",  TokenType::KW_WHILE}
    };

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        return make_token(it->second, lexeme);
    }
    return make_token(TokenType::IDENTIFIER, lexeme);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;

        start_ = current_;
        token_col_start_ = column_;
        char c = advance();

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(scan_number());
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(scan_identifier_or_keyword());
            continue;
        }

        switch (c) {
            case '+': tokens.push_back(make_token(TokenType::PLUS, "+")); break;
            case '-': tokens.push_back(make_token(TokenType::MINUS, "-")); break;
            case '*': tokens.push_back(make_token(TokenType::STAR, "*")); break;
            case '/': tokens.push_back(make_token(TokenType::SLASH, "/")); break;
            case '%': tokens.push_back(make_token(TokenType::PERCENT, "%")); break;
            case '(': tokens.push_back(make_token(TokenType::LPAREN, "(")); break;
            case ')': tokens.push_back(make_token(TokenType::RPAREN, ")")); break;
            case '{': tokens.push_back(make_token(TokenType::LBRACE, "{")); break;
            case '}': tokens.push_back(make_token(TokenType::RBRACE, "}")); break;
            case ',': tokens.push_back(make_token(TokenType::COMMA, ",")); break;
            case ';': tokens.push_back(make_token(TokenType::SEMICOLON, ";")); break;

            case '=':
                if (match('=')) {
                    tokens.push_back(make_token(TokenType::EQ_EQ, "=="));
                } else {
                    tokens.push_back(make_token(TokenType::ASSIGN, "="));
                }
                break;
            case '!':
                if (match('=')) {
                    tokens.push_back(make_token(TokenType::BANG_EQ, "!="));
                } else {
                    throw std::runtime_error("Unexpected character '!' (did you mean '!='?) at line " +
                                             std::to_string(line_) + ", col " + std::to_string(token_col_start_));
                }
                break;
            case '<':
                if (match('=')) {
                    tokens.push_back(make_token(TokenType::LESS_EQ, "<="));
                } else {
                    tokens.push_back(make_token(TokenType::LESS, "<"));
                }
                break;
            case '>':
                if (match('=')) {
                    tokens.push_back(make_token(TokenType::GREATER_EQ, ">="));
                } else {
                    tokens.push_back(make_token(TokenType::GREATER, ">"));
                }
                break;
            default:
                throw std::runtime_error(std::string("Unrecognized token '") + c + "' at line " +
                                         std::to_string(line_) + ", col " + std::to_string(token_col_start_));
        }
    }

    tokens.push_back(Token(TokenType::END_OF_FILE, "", SourceLocation{line_, column_}));
    return tokens;
}

} // namespace ptxcc
