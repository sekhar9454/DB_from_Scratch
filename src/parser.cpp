#include "dbms/parser.h"
#include <algorithm>
#include <stdexcept>

namespace dbms {

static std::string to_upper(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

Parser::Parser(std::vector<Token> tok_list) : tokens(std::move(tok_list)), pos(0) {}

std::shared_ptr<SQLStmt> Parser::parse_sql(const std::string& sql) {
    Lexer lexer(sql);
    auto tok_list = lexer.tokenize();
    Parser parser(tok_list);
    return parser.parse_statement();
}

const Token& Parser::peek() const {
    if (pos >= tokens.size()) return tokens.back();
    return tokens[pos];
}

const Token& Parser::advance() {
    if (pos >= tokens.size()) return tokens.back();
    return tokens[pos++];
}

bool Parser::match_keyword(const std::string& kw) {
    if (peek().type == TokenType::KEYWORD && to_upper(peek().text) == to_upper(kw)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match_symbol(const std::string& sym) {
    if (peek().type == TokenType::SYMBOL && peek().text == sym) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match_operator(const std::string& op) {
    if (peek().type == TokenType::OPERATOR && peek().text == op) {
        advance();
        return true;
    }
    return false;
}

void Parser::expect_symbol(const std::string& sym) {
    if (!match_symbol(sym)) {
        throw std::runtime_error("Expected symbol: '" + sym + "' but found '" + peek().text + "'");
    }
}

std::string Parser::expect_identifier() {
    if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::KEYWORD) {
        return advance().text;
    }
    throw std::runtime_error("Expected identifier but found '" + peek().text + "'");
}

std::shared_ptr<SQLStmt> Parser::parse_statement() {
    if (peek().type == TokenType::KEYWORD) {
        std::string kw = to_upper(peek().text);
        if (kw == "CREATE") {
            advance();
            if (match_keyword("TABLE")) {
                return parse_create_table();
            } else if (match_keyword("INDEX")) {
                return parse_create_index();
            }
        } else if (kw == "INSERT") {
            advance();
            return parse_insert();
        } else if (kw == "SELECT") {
            advance();
            return parse_select();
        } else if (kw == "UPDATE") {
            advance();
            return parse_update();
        } else if (kw == "DELETE") {
            advance();
            return parse_delete();
        }
    }
    throw std::runtime_error("Unexpected statement starting with '" + peek().text + "'");
}

std::shared_ptr<CreateTableStmt> Parser::parse_create_table() {
    auto stmt = std::make_shared<CreateTableStmt>();
    stmt->table_name = expect_identifier();

    expect_symbol("(");
    while (true) {
        if (peek().type == TokenType::KEYWORD && to_upper(peek().text) == "PRIMARY") {
            advance();
            if (match_keyword("KEY")) {
                expect_symbol("(");
                std::vector<std::string> pk_cols;
                while (true) {
                    pk_cols.push_back(expect_identifier());
                    if (match_symbol(",")) continue;
                    break;
                }
                expect_symbol(")");
                stmt->pkeys = pk_cols.size();
                if (match_symbol(",")) continue;
                break;
            }
        }

        std::string col_name = expect_identifier();
        std::string col_type_str = to_upper(expect_identifier());

        ValueType vtype = ValueType::INT64;
        if (col_type_str == "TEXT" || col_type_str == "VARCHAR" || col_type_str == "BYTES" || col_type_str == "STRING") {
            vtype = ValueType::BYTES;
        }

        if (peek().type == TokenType::KEYWORD && to_upper(peek().text) == "PRIMARY") {
            advance();
            if (match_keyword("KEY")) {
                stmt->pkeys = 1;
            }
        }

        stmt->cols.push_back(col_name);
        stmt->types.push_back(vtype);

        if (match_symbol(",")) {
            continue;
        }
        break;
    }
    expect_symbol(")");
    match_symbol(";");
    return stmt;
}

std::shared_ptr<CreateIndexStmt> Parser::parse_create_index() {
    auto stmt = std::make_shared<CreateIndexStmt>();
    stmt->index_name = expect_identifier();
    if (match_keyword("ON")) {
        stmt->table_name = expect_identifier();
    }
    expect_symbol("(");
    while (true) {
        stmt->cols.push_back(expect_identifier());
        if (match_symbol(",")) continue;
        break;
    }
    expect_symbol(")");
    match_symbol(";");
    return stmt;
}

std::shared_ptr<InsertStmt> Parser::parse_insert() {
    auto stmt = std::make_shared<InsertStmt>();
    if (match_keyword("INTO")) {
        stmt->table_name = expect_identifier();
    }

    if (match_symbol("(")) {
        while (true) {
            stmt->cols.push_back(expect_identifier());
            if (match_symbol(",")) continue;
            break;
        }
        expect_symbol(")");
    }

    if (match_keyword("VALUES")) {
        expect_symbol("(");
        while (true) {
            if (peek().type == TokenType::NUMBER) {
                int64_t num = std::stoll(advance().text);
                stmt->values.emplace_back(num);
            } else if (peek().type == TokenType::STRING_LITERAL) {
                stmt->values.emplace_back(advance().text);
            } else {
                throw std::runtime_error("Invalid value in INSERT statement: " + peek().text);
            }
            if (match_symbol(",")) continue;
            break;
        }
        expect_symbol(")");
    }

    match_symbol(";");
    return stmt;
}

std::shared_ptr<SelectStmt> Parser::parse_select() {
    auto stmt = std::make_shared<SelectStmt>();

    if (match_symbol("*")) {
        // select all
    } else {
        while (true) {
            stmt->select_cols.push_back(expect_identifier());
            if (match_symbol(",")) continue;
            break;
        }
    }

    if (match_keyword("FROM")) {
        stmt->table_name = expect_identifier();
    }

    if (match_keyword("WHERE")) {
        stmt->where_clause = parse_expr();
    }

    match_symbol(";");
    return stmt;
}

std::shared_ptr<UpdateStmt> Parser::parse_update() {
    auto stmt = std::make_shared<UpdateStmt>();
    stmt->table_name = expect_identifier();

    if (match_keyword("SET")) {
        while (true) {
            std::string col = expect_identifier();
            if (!match_symbol("=") && !match_operator("=")) {
                throw std::runtime_error("Expected '=' after column name in UPDATE statement");
            }
            Value val;
            if (peek().type == TokenType::NUMBER) {
                val = Value(std::stoll(advance().text));
            } else if (peek().type == TokenType::STRING_LITERAL) {
                val = Value(advance().text);
            }
            stmt->updates.push_back({col, val});
            if (match_symbol(",")) continue;
            break;
        }
    }

    if (match_keyword("WHERE")) {
        stmt->where_clause = parse_expr();
    }

    match_symbol(";");
    return stmt;
}

std::shared_ptr<DeleteStmt> Parser::parse_delete() {
    auto stmt = std::make_shared<DeleteStmt>();
    if (match_keyword("FROM")) {
        stmt->table_name = expect_identifier();
    }

    if (match_keyword("WHERE")) {
        stmt->where_clause = parse_expr();
    }

    match_symbol(";");
    return stmt;
}

std::shared_ptr<ExprAST> Parser::parse_expr() {
    return parse_logical_or();
}

std::shared_ptr<ExprAST> Parser::parse_logical_or() {
    auto left = parse_logical_and();
    while (match_keyword("OR")) {
        auto right = parse_logical_and();
        left = ExprAST::make_binary("OR", left, right);
    }
    return left;
}

std::shared_ptr<ExprAST> Parser::parse_logical_and() {
    auto left = parse_equality();
    while (match_keyword("AND")) {
        auto right = parse_equality();
        left = ExprAST::make_binary("AND", left, right);
    }
    return left;
}

std::shared_ptr<ExprAST> Parser::parse_equality() {
    auto left = parse_relational();
    while ((peek().type == TokenType::OPERATOR || peek().type == TokenType::SYMBOL) && (peek().text == "=" || peek().text == "!=")) {
        std::string op = advance().text;
        auto right = parse_relational();
        left = ExprAST::make_binary(op, left, right);
    }
    return left;
}

std::shared_ptr<ExprAST> Parser::parse_relational() {
    auto left = parse_primary_expr();
    while ((peek().type == TokenType::OPERATOR || peek().type == TokenType::SYMBOL) && (peek().text == "<" || peek().text == "<=" || peek().text == ">" || peek().text == ">=")) {
        std::string op = advance().text;
        auto right = parse_primary_expr();
        left = ExprAST::make_binary(op, left, right);
    }
    return left;
}

std::shared_ptr<ExprAST> Parser::parse_primary_expr() {
    if (peek().type == TokenType::NUMBER) {
        int64_t val = std::stoll(advance().text);
        return ExprAST::make_literal(Value(val));
    } else if (peek().type == TokenType::STRING_LITERAL) {
        std::string s = advance().text;
        return ExprAST::make_literal(Value(s));
    } else if (peek().type == TokenType::IDENTIFIER || peek().type == TokenType::KEYWORD) {
        std::string name = advance().text;
        return ExprAST::make_sym(name);
    } else if (match_symbol("(")) {
        auto expr = parse_expr();
        expect_symbol(")");
        return expr;
    }
    throw std::runtime_error("Unexpected token in expression: " + peek().text);
}

} // namespace dbms
