#include "dbms/tx.h"

namespace dbms {

std::shared_mutex TransactionManager::rw_lock;

KVTX::KVTX(KV* kv_ptr, bool write_mode)
    : kv(kv_ptr), is_write(write_mode), active(true) {
    if (is_write) {
        TransactionManager::rw_lock.lock();
    } else {
        TransactionManager::rw_lock.lock_shared();
    }
    root_snapshot = kv->tree.root;
}

KVTX::~KVTX() {
    if (active) {
        abort();
    }
}

Status KVTX::get(const Key& key, ValueData& val_out) {
    if (!active) return Status::TxAborted("Transaction inactive");
    return kv->get(key, val_out);
}

Status KVTX::set(const Key& key, const ValueData& val) {
    if (!active || !is_write) return Status::TxAborted("Transaction read-only or inactive");
    return kv->set(key, val);
}

Status KVTX::del(const Key& key) {
    if (!active || !is_write) return Status::TxAborted("Transaction read-only or inactive");
    return kv->del(key);
}

Status KVTX::commit() {
    if (!active) return Status::TxAborted("Transaction inactive");
    Status st = Status::OK();
    if (is_write) {
        st = kv->commit();
        TransactionManager::rw_lock.unlock();
    } else {
        TransactionManager::rw_lock.unlock_shared();
    }
    active = false;
    return st;
}

void KVTX::abort() {
    if (!active) return;
    kv->page_cache.clear();
    kv->pending_freed_pages.clear();
    kv->tree.root = root_snapshot;

    if (is_write) {
        TransactionManager::rw_lock.unlock();
    } else {
        TransactionManager::rw_lock.unlock_shared();
    }
    active = false;
}

DBTX::DBTX(DB* db_ptr, bool write_mode) : db(db_ptr), active(true) {
    kv_tx = std::make_unique<KVTX>(&db->kv, write_mode);
}

DBTX::~DBTX() {
    if (active) {
        abort();
    }
}

Status DBTX::get(const std::string& table, Record& rec) {
    if (!active) return Status::TxAborted("DBTX inactive");
    return db->get(table, rec);
}

Status DBTX::insert(const std::string& table, const Record& rec) {
    if (!active) return Status::TxAborted("DBTX inactive");
    return db->insert(table, rec);
}

Status DBTX::update(const std::string& table, const Record& rec) {
    if (!active) return Status::TxAborted("DBTX inactive");
    return db->update(table, rec);
}

Status DBTX::delete_record(const std::string& table, const Record& rec) {
    if (!active) return Status::TxAborted("DBTX inactive");
    return db->delete_record(table, rec);
}

Status DBTX::commit() {
    if (!active) return Status::TxAborted("DBTX inactive");
    Status st = kv_tx->commit();
    active = false;
    return st;
}

void DBTX::abort() {
    if (!active) return;
    kv_tx->abort();
    active = false;
}

} // namespace dbms
