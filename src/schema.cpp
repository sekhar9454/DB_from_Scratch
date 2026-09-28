#include "dbms/schema.h"

namespace dbms {

void Record::add_int64(const std::string& col, int64_t val) {
    cols.push_back(col);
    vals.push_back(Value(val));
}

void Record::add_str(const std::string& col, const std::string& val) {
    cols.push_back(col);
    vals.push_back(Value(val));
}

const Value* Record::get(const std::string& col) const {
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i] == col) {
            return &vals[i];
        }
    }
    return nullptr;
}

void Record::set(const std::string& col, const Value& val) {
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i] == col) {
            vals[i] = val;
            return;
        }
    }
    cols.push_back(col);
    vals.push_back(val);
}

int TableDef::col_idx(const std::string& col_name) const {
    for (size_t i = 0; i < cols.size(); ++i) {
        if (cols[i] == col_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace dbms
