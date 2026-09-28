#ifndef DBMS_LEXER_H
#define DBMS_LEXER_H

#include <string>
#include <vector>

namespace dbms {

enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    NUMBER,
    STRING_LITERAL,
    OPERATOR,
    SYMBOL,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string text;
};

class Lexer {
public:
    explicit Lexer(std::string sql);
    std::vector<Token> tokenize();

private:
    std::string input;
    size_t pos{0};

    char peek() const;
    char advance();
    void skip_whitespace();
};

} // namespace dbms

#endif // DBMS_LEXER_H
