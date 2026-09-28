#ifndef DBMS_SQL_AST_H
#define DBMS_SQL_AST_H

#include "schema.h"
#include <string>
#include <vector>
#include <memory>

namespace dbms {

enum class ExprType {
    SYM = 1,
    LITERAL = 2,
    BINARY_OP = 3
};

struct ExprAST {
    ExprType type{ExprType::LITERAL};
    Value val; // Literal value (int64 or string)
    std::string sym_name; // Column name if SYM
    std::string op; // "=", "<", "<=", ">", ">=", "!=", "AND", "OR"
    std::shared_ptr<ExprAST> left;
    std::shared_ptr<ExprAST> right;

    static std::shared_ptr<ExprAST> make_literal(Value v) {
        auto e = std::make_shared<ExprAST>();
        e->type = ExprType::LITERAL;
        e->val = std::move(v);
        return e;
    }

    static std::shared_ptr<ExprAST> make_sym(std::string name) {
        auto e = std::make_shared<ExprAST>();
        e->type = ExprType::SYM;
        e->sym_name = std::move(name);
        return e;
    }

    static std::shared_ptr<ExprAST> make_binary(std::string op, std::shared_ptr<ExprAST> l, std::shared_ptr<ExprAST> r) {
        auto e = std::make_shared<ExprAST>();
        e->type = ExprType::BINARY_OP;
        e->op = std::move(op);
        e->left = std::move(l);
        e->right = std::move(r);
        return e;
    }
};

enum class StmtType {
    CREATE_TABLE,
    CREATE_INDEX,
    INSERT,
    SELECT,
    UPDATE,
    DELETE
};

struct SQLStmt {
    virtual ~SQLStmt() = default;
    virtual StmtType type() const = 0;
};

struct CreateTableStmt : public SQLStmt {
    std::string table_name;
    std::vector<std::string> cols;
    std::vector<ValueType> types;
    size_t pkeys{1};

    StmtType type() const override { return StmtType::CREATE_TABLE; }
};

struct CreateIndexStmt : public SQLStmt {
    std::string index_name;
    std::string table_name;
    std::vector<std::string> cols;

    StmtType type() const override { return StmtType::CREATE_INDEX; }
};

struct InsertStmt : public SQLStmt {
    std::string table_name;
    std::vector<std::string> cols;
    std::vector<Value> values;

    StmtType type() const override { return StmtType::INSERT; }
};

struct SelectStmt : public SQLStmt {
    std::string table_name;
    std::vector<std::string> select_cols; // empty means '*'
    std::shared_ptr<ExprAST> where_clause;

    StmtType type() const override { return StmtType::SELECT; }
};

struct UpdateStmt : public SQLStmt {
    std::string table_name;
    std::vector<std::pair<std::string, Value>> updates;
    std::shared_ptr<ExprAST> where_clause;

    StmtType type() const override { return StmtType::UPDATE; }
};

struct DeleteStmt : public SQLStmt {
    std::string table_name;
    std::shared_ptr<ExprAST> where_clause;

    StmtType type() const override { return StmtType::DELETE; }
};

} // namespace dbms

#endif // DBMS_SQL_AST_H
