#ifndef DBMS_SCHEMA_H
#define DBMS_SCHEMA_H

#include "types.h"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <memory>

namespace dbms {

enum class ValueType : uint32_t {
    INT64 = 1,
    BYTES = 2
};

struct Value {
    ValueType type{ValueType::INT64};
    int64_t i64{0};
    std::string str;

    Value() = default;
    explicit Value(int64_t v) : type(ValueType::INT64), i64(v) {}
    explicit Value(std::string s) : type(ValueType::BYTES), str(std::move(s)) {}

    bool operator==(const Value& o) const {
        if (type != o.type) return false;
        if (type == ValueType::INT64) return i64 == o.i64;
        return str == o.str;
    }

    bool operator<(const Value& o) const {
        if (type != o.type) return static_cast<uint32_t>(type) < static_cast<uint32_t>(o.type);
        if (type == ValueType::INT64) return i64 < o.i64;
        return str < o.str;
    }
};

class Record {
public:
    std::vector<std::string> cols;
    std::vector<Value> vals;

    void add_int64(const std::string& col, int64_t val);
    void add_str(const std::string& col, const std::string& val);
    const Value* get(const std::string& col) const;
    void set(const std::string& col, const Value& val);
};

struct TableDef {
    std::string name;
    std::vector<ValueType> types;
    std::vector<std::string> cols;
    size_t pkeys{1}; // First pkeys columns in cols form the primary key
    std::vector<std::vector<std::string>> indexes; // Secondary index column lists
    
    uint32_t prefix_id{0}; // B+Tree key prefix for primary table
    std::vector<uint32_t> index_prefixes; // B+Tree key prefixes for secondary indexes

    int col_idx(const std::string& col_name) const;
};

} // namespace dbms

#endif // DBMS_SCHEMA_H
