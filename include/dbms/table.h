#ifndef DBMS_TABLE_H
#define DBMS_TABLE_H

#include "kv.h"
#include "schema.h"
#include "encoding.h"
#include <unordered_map>
#include <memory>

namespace dbms {

class DB {
public:
    KV kv;
    std::unordered_map<std::string, TableDef> tables;
    uint32_t next_prefix_id{1};

    DB();
    ~DB();

    Status open(const std::string& filepath);
    void close();

    Status create_table(const TableDef& tdef);
    const TableDef* get_table_def(const std::string& name) const;

    Status get(const std::string& table, Record& rec);
    Status insert(const std::string& table, const Record& rec);
    Status update(const std::string& table, const Record& rec);
    Status upsert(const std::string& table, const Record& rec);
    Status delete_record(const std::string& table, const Record& rec);

    // Helper to generate index keys
    Key get_primary_key(const TableDef& tdef, const Record& rec) const;
    std::vector<Key> get_secondary_keys(const TableDef& tdef, const Record& rec) const;

private:
    Status load_tables();
    Status save_table_def(const TableDef& tdef);
};

} // namespace dbms

#endif // DBMS_TABLE_H
