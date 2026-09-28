#ifndef DBMS_KV_H
#define DBMS_KV_H

#include "types.h"
#include "btree.h"
#include "freelist.h"
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace dbms {

constexpr char KV_MAGIC[16] = "DBMS_KV_COW_V2";

struct MetaPage {
    PageID root_id{0};
    uint64_t total_pages{2}; // 0 & 1 are reserved meta pages
    PageID free_head_page{0};
    uint64_t free_head_seq{0};
    PageID free_tail_page{0};
    uint64_t free_tail_seq{0};
    uint64_t commit_seq{0};
};

class KV {
public:
    std::string path;
    int fd;
    std::fstream file;
    MetaPage meta;
    BTree tree;
    FreeList freelist;
    std::mutex kv_mutex;

    std::unordered_map<PageID, std::vector<uint8_t>> page_cache;
    std::vector<PageID> pending_freed_pages;

    KV();
    ~KV();

    Status open(const std::string& filepath);
    void close();

    Status get(const Key& key, ValueData& val_out);
    Status set(const Key& key, const ValueData& val);
    Status del(const Key& key);
    Status commit();

    std::vector<uint8_t> read_page(PageID page_id);
    void write_page(PageID page_id, const std::vector<uint8_t>& data);
    PageID alloc_page(const std::vector<uint8_t>& data);
    void free_page(PageID page_id);

private:
    Status read_meta();
    Status write_meta();
};

} // namespace dbms

#endif // DBMS_KV_H
