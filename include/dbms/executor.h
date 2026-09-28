#ifndef DBMS_EXECUTOR_H
#define DBMS_EXECUTOR_H

#include "sql_ast.h"
#include "tx.h"
#include <string>
#include <vector>
#include <memory>

namespace dbms {

struct QueryResult {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    size_t rows_affected{0};
    std::string message;

    void print(std::ostream& os) const;
};

class QLEvalContext {
public:
    static Value eval(const Record& env, const ExprAST* expr);
    static bool eval_condition(const Record& env, const ExprAST* expr);
};

class Executor {
public:
    explicit Executor(DB* db_ptr);

    QueryResult execute(const std::string& sql);
    QueryResult execute_stmt(const SQLStmt* stmt);

private:
    DB* db;

    QueryResult exec_create_table(const CreateTableStmt* stmt);
    QueryResult exec_create_index(const CreateIndexStmt* stmt);
    QueryResult exec_insert(const InsertStmt* stmt);
    QueryResult exec_select(const SelectStmt* stmt);
    QueryResult exec_update(const UpdateStmt* stmt);
    QueryResult exec_delete(const DeleteStmt* stmt);
};

} // namespace dbms

#endif // DBMS_EXECUTOR_H
