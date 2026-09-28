#include "dbms/lexer.h"
#include <cctype>
#include <algorithm>

namespace dbms {

static bool is_keyword(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    return (upper == "CREATE" || upper == "TABLE" || upper == "INDEX" ||
            upper == "PRIMARY" || upper == "KEY" || upper == "INSERT" ||
            upper == "INTO" || upper == "VALUES" || upper == "SELECT" ||
            upper == "FROM" || upper == "WHERE" || upper == "UPDATE" ||
            upper == "SET" || upper == "DELETE" || upper == "INT" ||
            upper == "INTEGER" || upper == "TEXT" || upper == "VARCHAR" ||
            upper == "BYTES" || upper == "AND" || upper == "OR");
}

Lexer::Lexer(std::string sql) : input(std::move(sql)), pos(0) {}

char Lexer::peek() const {
    if (pos >= input.size()) return '\0';
    return input[pos];
}

char Lexer::advance() {
    if (pos >= input.size()) return '\0';
    return input[pos++];
}

void Lexer::skip_whitespace() {
    while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
        pos++;
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < input.size()) {
        skip_whitespace();
        if (pos >= input.size()) break;

        char c = peek();

        // String literals ('string')
        if (c == '\'' || c == '"') {
            char quote = advance();
            std::string s;
            while (pos < input.size() && peek() != quote) {
                s.push_back(advance());
            }
            if (peek() == quote) advance(); // consume closing quote
            tokens.push_back({TokenType::STRING_LITERAL, s});
        }
        // Numbers
        else if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && pos + 1 < input.size() && std::isdigit(static_cast<unsigned char>(input[pos + 1])))) {
            std::string num;
            if (c == '-') num.push_back(advance());
            while (pos < input.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                num.push_back(advance());
            }
            tokens.push_back({TokenType::NUMBER, num});
        }
        // Identifiers and Keywords
        else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string ident;
            while (pos < input.size() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                ident.push_back(advance());
            }
            if (is_keyword(ident)) {
                tokens.push_back({TokenType::KEYWORD, ident});
            } else {
                tokens.push_back({TokenType::IDENTIFIER, ident});
            }
        }
        // Multi-char operators: <=, >=, !=, ==
        else if ((c == '<' || c == '>' || c == '!') && pos + 1 < input.size() && input[pos + 1] == '=') {
            std::string op;
            op.push_back(advance());
            op.push_back(advance());
            tokens.push_back({TokenType::OPERATOR, op});
        }
        // Single-char operators: =, <, >
        else if (c == '=' || c == '<' || c == '>') {
            std::string op;
            op.push_back(advance());
            tokens.push_back({TokenType::OPERATOR, op});
        }
        // Symbols: (, ), ,, ;, *
        else if (c == '(' || c == ')' || c == ',' || c == ';' || c == '*') {
            std::string sym;
            sym.push_back(advance());
            tokens.push_back({TokenType::SYMBOL, sym});
        }
        else {
            advance(); // Skip unknown char
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, ""});
    return tokens;
}

} // namespace dbms
