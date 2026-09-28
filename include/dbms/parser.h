#ifndef DBMS_PARSER_H
#define DBMS_PARSER_H

#include "sql_ast.h"
#include "lexer.h"
#include <memory>

namespace dbms {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    static std::shared_ptr<SQLStmt> parse_sql(const std::string& sql);

    std::shared_ptr<SQLStmt> parse_statement();

private:
    std::vector<Token> tokens;
    size_t pos{0};

    const Token& peek() const;
    const Token& advance();
    bool match_keyword(const std::string& kw);
    bool match_symbol(const std::string& sym);
    bool match_operator(const std::string& op);
    void expect_symbol(const std::string& sym);
    std::string expect_identifier();

    std::shared_ptr<CreateTableStmt> parse_create_table();
    std::shared_ptr<CreateIndexStmt> parse_create_index();
    std::shared_ptr<InsertStmt> parse_insert();
    std::shared_ptr<SelectStmt> parse_select();
    std::shared_ptr<UpdateStmt> parse_update();
    std::shared_ptr<DeleteStmt> parse_delete();

    std::shared_ptr<ExprAST> parse_expr();
    std::shared_ptr<ExprAST> parse_logical_or();
    std::shared_ptr<ExprAST> parse_logical_and();
    std::shared_ptr<ExprAST> parse_equality();
    std::shared_ptr<ExprAST> parse_relational();
    std::shared_ptr<ExprAST> parse_primary_expr();
};

} // namespace dbms

#endif // DBMS_PARSER_H
