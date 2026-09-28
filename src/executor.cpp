#include "dbms/executor.h"
#include "dbms/parser.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace dbms {

void QueryResult::print(std::ostream& os) const {
    if (!message.empty()) {
        os << message << "\n";
    }

    if (headers.empty()) return;

    std::vector<size_t> widths(headers.size(), 0);
    for (size_t i = 0; i < headers.size(); ++i) {
        widths[i] = std::max(widths[i], headers[i].size());
    }

    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }

    os << "+";
    for (size_t w : widths) os << std::string(w + 2, '-') << "+";
    os << "\n";

    os << "|";
    for (size_t i = 0; i < headers.size(); ++i) {
        os << " " << std::left << std::setw(widths[i]) << headers[i] << " |";
    }
    os << "\n";

    os << "+";
    for (size_t w : widths) os << std::string(w + 2, '-') << "+";
    os << "\n";

    for (const auto& row : rows) {
        os << "|";
        for (size_t i = 0; i < row.size(); ++i) {
            os << " " << std::left << std::setw(widths[i]) << row[i] << " |";
        }
        os << "\n";
    }

    os << "+";
    for (size_t w : widths) os << std::string(w + 2, '-') << "+";
    os << "\n";
    os << "(" << rows.size() << " rows)\n";
}

Value QLEvalContext::eval(const Record& env, const ExprAST* expr) {
    if (!expr) return Value(0);

    if (expr->type == ExprType::LITERAL) {
        return expr->val;
    } else if (expr->type == ExprType::SYM) {
        const Value* v = env.get(expr->sym_name);
        return v ? *v : Value(0);
    } else if (expr->type == ExprType::BINARY_OP) {
        Value left_v = eval(env, expr->left.get());
        Value right_v = eval(env, expr->right.get());

        if (expr->op == "=") return Value(left_v == right_v ? 1 : 0);
        if (expr->op == "!=") return Value(!(left_v == right_v) ? 1 : 0);
        if (expr->op == "<") return Value(left_v < right_v ? 1 : 0);
        if (expr->op == "<=") return Value((left_v < right_v || left_v == right_v) ? 1 : 0);
        if (expr->op == ">") return Value((!(left_v < right_v) && !(left_v == right_v)) ? 1 : 0);
        if (expr->op == ">=") return Value(!(left_v < right_v) ? 1 : 0);
        if (expr->op == "AND") return Value((left_v.i64 != 0 && right_v.i64 != 0) ? 1 : 0);
        if (expr->op == "OR") return Value((left_v.i64 != 0 || right_v.i64 != 0) ? 1 : 0);
    }

    return Value(0);
}

bool QLEvalContext::eval_condition(const Record& env, const ExprAST* expr) {
    if (!expr) return true;
    Value res = eval(env, expr);
    return (res.type == ValueType::INT64 && res.i64 != 0);
}

Executor::Executor(DB* db_ptr) : db(db_ptr) {}

QueryResult Executor::execute(const std::string& sql) {
    try {
        auto stmt = Parser::parse_sql(sql);
        if (!stmt) return QueryResult{ {}, {}, 0, "Empty statement" };
        return execute_stmt(stmt.get());
    } catch (const std::exception& ex) {
        return QueryResult{ {}, {}, 0, std::string("Execution error: ") + ex.what() };
    }
}

QueryResult Executor::execute_stmt(const SQLStmt* stmt) {
    switch (stmt->type()) {
        case StmtType::CREATE_TABLE:
            return exec_create_table(static_cast<const CreateTableStmt*>(stmt));
        case StmtType::CREATE_INDEX:
            return exec_create_index(static_cast<const CreateIndexStmt*>(stmt));
        case StmtType::INSERT:
            return exec_insert(static_cast<const InsertStmt*>(stmt));
        case StmtType::SELECT:
            return exec_select(static_cast<const SelectStmt*>(stmt));
        case StmtType::UPDATE:
            return exec_update(static_cast<const UpdateStmt*>(stmt));
        case StmtType::DELETE:
            return exec_delete(static_cast<const DeleteStmt*>(stmt));
    }
    return QueryResult{ {}, {}, 0, "Unknown statement type" };
}

QueryResult Executor::exec_create_table(const CreateTableStmt* stmt) {
    TableDef tdef;
    tdef.name = stmt->table_name;
    tdef.cols = stmt->cols;
    tdef.types = stmt->types;
    tdef.pkeys = stmt->pkeys;

    Status st = db->create_table(tdef);
    if (!st.ok()) {
        return QueryResult{ {}, {}, 0, st.message };
    }

    return QueryResult{ {}, {}, 0, "Table '" + stmt->table_name + "' created successfully." };
}

QueryResult Executor::exec_create_index(const CreateIndexStmt* stmt) {
    const TableDef* tdef = db->get_table_def(stmt->table_name);
    if (!tdef) {
        return QueryResult{ {}, {}, 0, "Table not found: " + stmt->table_name };
    }

    TableDef new_tdef = *tdef;
    new_tdef.indexes.push_back(stmt->cols);
    new_tdef.index_prefixes.push_back(db->next_prefix_id++);

    db->tables[new_tdef.name] = new_tdef;
    return QueryResult{ {}, {}, 0, "Index created on " + stmt->table_name };
}

QueryResult Executor::exec_insert(const InsertStmt* stmt) {
    const TableDef* tdef = db->get_table_def(stmt->table_name);
    if (!tdef) return QueryResult{ {}, {}, 0, "Table not found: " + stmt->table_name };

    Record rec;
    if (stmt->cols.empty()) {
        rec.cols = tdef->cols;
        rec.vals = stmt->values;
    } else {
        rec.cols = stmt->cols;
        rec.vals = stmt->values;
    }

    DBTX tx(db, true);
    Status st = tx.insert(stmt->table_name, rec);
    if (!st.ok()) {
        tx.abort();
        return QueryResult{ {}, {}, 0, st.message };
    }
    tx.commit();

    return QueryResult{ {}, {}, 1, "1 row inserted successfully." };
}

QueryResult Executor::exec_select(const SelectStmt* stmt) {
    const TableDef* tdef = db->get_table_def(stmt->table_name);
    if (!tdef) return QueryResult{ {}, {}, 0, "Table not found: " + stmt->table_name };

    QueryResult res;
    if (stmt->select_cols.empty()) {
        res.headers = tdef->cols;
    } else {
        res.headers = stmt->select_cols;
    }

    DBTX tx(db, false);
    Key prefix_start = encode_key(tdef->prefix_id, {});
    BIter iter = db->kv.tree.seek_ge(prefix_start);

    while (iter.valid()) {
        Key k; ValueData payload;
        iter.deref(k, payload);

        if (k.size() < 4 || decode_u32_be(k.data()) != tdef->prefix_id) {
            break;
        }

        Record rec;
        const uint8_t* ptr = k.data() + 4;
        size_t len = k.size() - 4;
        size_t off = 0;
        for (size_t i = 0; i < tdef->pkeys; ++i) {
            Value v;
            off += decode_value_order_preserving(ptr + off, len - off, tdef->types[i], v);
            rec.cols.push_back(tdef->cols[i]);
            rec.vals.push_back(v);
        }

        decode_row_payload(payload, *tdef, rec);

        if (QLEvalContext::eval_condition(rec, stmt->where_clause.get())) {
            std::vector<std::string> row;
            for (const auto& col_name : res.headers) {
                const Value* v = rec.get(col_name);
                if (v) {
                    if (v->type == ValueType::INT64) {
                        row.push_back(std::to_string(v->i64));
                    } else {
                        row.push_back(v->str);
                    }
                } else {
                    row.push_back("NULL");
                }
            }
            res.rows.push_back(row);
        }

        iter.next();
    }

    tx.commit();
    return res;
}

QueryResult Executor::exec_update(const UpdateStmt* stmt) {
    const TableDef* tdef = db->get_table_def(stmt->table_name);
    if (!tdef) return QueryResult{ {}, {}, 0, "Table not found: " + stmt->table_name };

    DBTX tx(db, true);
    Key prefix_start = encode_key(tdef->prefix_id, {});
    BIter iter = db->kv.tree.seek_ge(prefix_start);

    std::vector<Record> records_to_update;

    while (iter.valid()) {
        Key k; ValueData payload;
        iter.deref(k, payload);
        if (k.size() < 4 || decode_u32_be(k.data()) != tdef->prefix_id) break;

        Record rec;
        const uint8_t* ptr = k.data() + 4;
        size_t len = k.size() - 4;
        size_t off = 0;
        for (size_t i = 0; i < tdef->pkeys; ++i) {
            Value v;
            off += decode_value_order_preserving(ptr + off, len - off, tdef->types[i], v);
            rec.cols.push_back(tdef->cols[i]);
            rec.vals.push_back(v);
        }
        decode_row_payload(payload, *tdef, rec);

        if (QLEvalContext::eval_condition(rec, stmt->where_clause.get())) {
            for (const auto& [col, val] : stmt->updates) {
                rec.set(col, val);
            }
            records_to_update.push_back(rec);
        }
        iter.next();
    }

    size_t count = 0;
    for (const auto& rec : records_to_update) {
        if (db->update(stmt->table_name, rec).ok()) count++;
    }

    tx.commit();
    return QueryResult{ {}, {}, count, std::to_string(count) + " rows updated." };
}

QueryResult Executor::exec_delete(const DeleteStmt* stmt) {
    const TableDef* tdef = db->get_table_def(stmt->table_name);
    if (!tdef) return QueryResult{ {}, {}, 0, "Table not found: " + stmt->table_name };

    DBTX tx(db, true);
    Key prefix_start = encode_key(tdef->prefix_id, {});
    BIter iter = db->kv.tree.seek_ge(prefix_start);

    std::vector<Record> records_to_del;

    while (iter.valid()) {
        Key k; ValueData payload;
        iter.deref(k, payload);
        if (k.size() < 4 || decode_u32_be(k.data()) != tdef->prefix_id) break;

        Record rec;
        const uint8_t* ptr = k.data() + 4;
        size_t len = k.size() - 4;
        size_t off = 0;
        for (size_t i = 0; i < tdef->pkeys; ++i) {
            Value v;
            off += decode_value_order_preserving(ptr + off, len - off, tdef->types[i], v);
            rec.cols.push_back(tdef->cols[i]);
            rec.vals.push_back(v);
        }
        decode_row_payload(payload, *tdef, rec);

        if (QLEvalContext::eval_condition(rec, stmt->where_clause.get())) {
            records_to_del.push_back(rec);
        }
        iter.next();
    }

    size_t count = 0;
    for (const auto& rec : records_to_del) {
        if (db->delete_record(stmt->table_name, rec).ok()) count++;
    }

    tx.commit();
    return QueryResult{ {}, {}, count, std::to_string(count) + " rows deleted." };
}

} // namespace dbms
