#include "dbms/table.h"
#include <sstream>

namespace dbms {

DB::DB() {}
DB::~DB() { close(); }

Status DB::open(const std::string& filepath) {
    Status st = kv.open(filepath);
    if (!st.ok()) return st;
    return load_tables();
}

void DB::close() {
    kv.close();
}

Status DB::load_tables() {
    Key meta_prefix = encode_key(0, {Value("table_catalog")});
    ValueData meta_val;
    if (kv.get(meta_prefix, meta_val).ok()) {
        std::string meta_str(meta_val.begin(), meta_val.end());
        std::stringstream ss(meta_str);
        uint32_t num_tables = 0;
        ss >> num_tables >> next_prefix_id;

        for (uint32_t i = 0; i < num_tables; ++i) {
            TableDef tdef;
            uint32_t num_cols, pkeys, num_idx;
            ss >> tdef.name >> tdef.prefix_id >> num_cols >> pkeys >> num_idx;
            tdef.pkeys = pkeys;

            for (uint32_t c = 0; c < num_cols; ++c) {
                std::string col_name;
                uint32_t col_type;
                ss >> col_name >> col_type;
                tdef.cols.push_back(col_name);
                tdef.types.push_back(static_cast<ValueType>(col_type));
            }

            for (uint32_t idx = 0; idx < num_idx; ++idx) {
                uint32_t idx_prefix, idx_col_count;
                ss >> idx_prefix >> idx_col_count;
                tdef.index_prefixes.push_back(idx_prefix);
                std::vector<std::string> idx_cols;
                for (uint32_t ic = 0; ic < idx_col_count; ++ic) {
                    std::string ic_name;
                    ss >> ic_name;
                    idx_cols.push_back(ic_name);
                }
                tdef.indexes.push_back(idx_cols);
            }

            tables[tdef.name] = tdef;
        }
    }
    return Status::OK();
}

Status DB::save_table_def(const TableDef& tdef) {
    tables[tdef.name] = tdef;

    std::stringstream ss;
    ss << tables.size() << " " << next_prefix_id << "\n";

    for (const auto& [name, td] : tables) {
        ss << td.name << " " << td.prefix_id << " " << td.cols.size() << " " << td.pkeys << " " << td.indexes.size() << "\n";
        for (size_t c = 0; c < td.cols.size(); ++c) {
            ss << td.cols[c] << " " << static_cast<uint32_t>(td.types[c]) << " ";
        }
        ss << "\n";
        for (size_t idx = 0; idx < td.indexes.size(); ++idx) {
            ss << td.index_prefixes[idx] << " " << td.indexes[idx].size() << " ";
            for (const auto& ic : td.indexes[idx]) {
                ss << ic << " ";
            }
            ss << "\n";
        }
    }

    Key meta_prefix = encode_key(0, {Value("table_catalog")});
    std::string meta_str = ss.str();
    ValueData meta_val(meta_str.begin(), meta_str.end());
    kv.set(meta_prefix, meta_val);
    return kv.commit();
}

Status DB::create_table(const TableDef& tdef_in) {
    if (tables.find(tdef_in.name) != tables.end()) {
        return Status::TableExists("Table already exists: " + tdef_in.name);
    }

    TableDef tdef = tdef_in;
    tdef.prefix_id = next_prefix_id++;

    for (size_t i = 0; i < tdef.indexes.size(); ++i) {
        tdef.index_prefixes.push_back(next_prefix_id++);
    }

    return save_table_def(tdef);
}

const TableDef* DB::get_table_def(const std::string& name) const {
    auto it = tables.find(name);
    if (it != tables.end()) return &it->second;
    return nullptr;
}

Key DB::get_primary_key(const TableDef& tdef, const Record& rec) const {
    std::vector<Value> pk_vals;
    for (size_t i = 0; i < tdef.pkeys; ++i) {
        const Value* v = rec.get(tdef.cols[i]);
        pk_vals.push_back(v ? *v : Value());
    }
    return encode_key(tdef.prefix_id, pk_vals);
}

std::vector<Key> DB::get_secondary_keys(const TableDef& tdef, const Record& rec) const {
    std::vector<Key> keys;

    std::vector<Value> pk_vals;
    for (size_t i = 0; i < tdef.pkeys; ++i) {
        const Value* v = rec.get(tdef.cols[i]);
        pk_vals.push_back(v ? *v : Value());
    }

    for (size_t idx = 0; idx < tdef.indexes.size(); ++idx) {
        uint32_t prefix = tdef.index_prefixes[idx];
        std::vector<Value> idx_vals;

        for (const auto& col_name : tdef.indexes[idx]) {
            const Value* v = rec.get(col_name);
            idx_vals.push_back(v ? *v : Value());
        }
        idx_vals.insert(idx_vals.end(), pk_vals.begin(), pk_vals.end());
        keys.push_back(encode_key(prefix, idx_vals));
    }

    return keys;
}

Status DB::get(const std::string& table_name, Record& rec) {
    const TableDef* tdef = get_table_def(table_name);
    if (!tdef) return Status::TableNotFound("Table not found: " + table_name);

    Key pk = get_primary_key(*tdef, rec);
    ValueData payload;
    Status st = kv.get(pk, payload);
    if (!st.ok()) return st;

    return decode_row_payload(payload, *tdef, rec);
}

Status DB::insert(const std::string& table_name, const Record& rec) {
    const TableDef* tdef = get_table_def(table_name);
    if (!tdef) return Status::TableNotFound("Table not found: " + table_name);

    Key pk = get_primary_key(*tdef, rec);
    ValueData existing_val;
    if (kv.get(pk, existing_val).ok()) {
        return Status::KeyExists("Primary key violation in table: " + table_name);
    }

    ValueData payload = encode_row_payload(*tdef, rec);
    kv.set(pk, payload);

    std::vector<Key> sec_keys = get_secondary_keys(*tdef, rec);
    for (const auto& sk : sec_keys) {
        kv.set(sk, ValueData());
    }

    return kv.commit();
}

Status DB::update(const std::string& table_name, const Record& rec) {
    const TableDef* tdef = get_table_def(table_name);
    if (!tdef) return Status::TableNotFound("Table not found: " + table_name);

    Record old_rec;
    for (size_t i = 0; i < tdef->pkeys; ++i) {
        const Value* v = rec.get(tdef->cols[i]);
        if (!v) return Status::InvalidArgument("Primary key missing: " + tdef->cols[i]);
        old_rec.set(tdef->cols[i], *v);
    }

    Status st = get(table_name, old_rec);
    if (!st.ok()) return Status::NotFound("Record to update not found");

    std::vector<Key> old_sec_keys = get_secondary_keys(*tdef, old_rec);
    for (const auto& sk : old_sec_keys) {
        kv.del(sk);
    }

    for (size_t i = 0; i < rec.cols.size(); ++i) {
        old_rec.set(rec.cols[i], rec.vals[i]);
    }

    Key pk = get_primary_key(*tdef, old_rec);
    ValueData payload = encode_row_payload(*tdef, old_rec);
    kv.set(pk, payload);

    std::vector<Key> new_sec_keys = get_secondary_keys(*tdef, old_rec);
    for (const auto& sk : new_sec_keys) {
        kv.set(sk, ValueData());
    }

    return kv.commit();
}

Status DB::upsert(const std::string& table_name, const Record& rec) {
    Record tmp = rec;
    if (get(table_name, tmp).ok()) {
        return update(table_name, rec);
    }
    return insert(table_name, rec);
}

Status DB::delete_record(const std::string& table_name, const Record& rec) {
    const TableDef* tdef = get_table_def(table_name);
    if (!tdef) return Status::TableNotFound("Table not found: " + table_name);

    Record old_rec;
    for (size_t i = 0; i < tdef->pkeys; ++i) {
        const Value* v = rec.get(tdef->cols[i]);
        if (!v) return Status::InvalidArgument("Primary key missing: " + tdef->cols[i]);
        old_rec.set(tdef->cols[i], *v);
    }

    Status st = get(table_name, old_rec);
    if (!st.ok()) return Status::NotFound("Record to delete not found");

    Key pk = get_primary_key(*tdef, old_rec);
    kv.del(pk);

    std::vector<Key> sec_keys = get_secondary_keys(*tdef, old_rec);
    for (const auto& sk : sec_keys) {
        kv.del(sk);
    }

    return kv.commit();
}

} // namespace dbms
