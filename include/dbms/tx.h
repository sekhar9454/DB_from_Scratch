#ifndef DBMS_TX_H
#define DBMS_TX_H

#include "table.h"
#include <shared_mutex>
#include <memory>

namespace dbms {

class KVTX {
public:
    KV* kv;
    bool is_write;
    PageID root_snapshot{0};
    bool active{false};

    KVTX(KV* kv_ptr, bool write_mode);
    ~KVTX();

    Status get(const Key& key, ValueData& val_out);
    Status set(const Key& key, const ValueData& val);
    Status del(const Key& key);

    Status commit();
    void abort();
};

class DBTX {
public:
    DB* db;
    std::unique_ptr<KVTX> kv_tx;
    bool active{false};

    DBTX(DB* db_ptr, bool write_mode);
    ~DBTX();

    Status get(const std::string& table, Record& rec);
    Status insert(const std::string& table, const Record& rec);
    Status update(const std::string& table, const Record& rec);
    Status delete_record(const std::string& table, const Record& rec);

    Status commit();
    void abort();
};

class TransactionManager {
public:
    static std::shared_mutex rw_lock;
};

} // namespace dbms

#endif // DBMS_TX_H
