#include "dbms/kv.h"
#include <iostream>
#include <cstring>
#include <filesystem>

namespace dbms {

KV::KV() : fd(-1) {}

KV::~KV() {
    close();
}

Status KV::open(const std::string& filepath) {
    path = filepath;

    bool exists = std::filesystem::exists(filepath);
    std::ios_base::openmode mode = std::ios::in | std::ios::out | std::ios::binary;
    if (!exists) {
        std::ofstream create_file(filepath, std::ios::binary);
        create_file.close();
    }

    file.open(filepath, mode);
    if (!file.is_open()) {
        return Status::IOError("Failed to open database file: " + filepath);
    }

    tree.get_page = [this](PageID pid) { return read_page(pid); };
    tree.alloc_page = [this](const std::vector<uint8_t>& data) { return alloc_page(data); };
    tree.del_page = [this](PageID pid) { free_page(pid); };

    freelist.get_page = tree.get_page;
    freelist.alloc_page = tree.alloc_page;
    freelist.set_page = [this](PageID pid, const std::vector<uint8_t>& data) { write_page(pid, data); };

    if (!exists || std::filesystem::file_size(filepath) == 0) {
        meta = MetaPage();
        meta.total_pages = 2;
        meta.root_id = 0;
        meta.commit_seq = 0;

        Status st = write_meta();
        if (!st.ok()) return st;
        
        // Immediately flush initial meta to disk
        for (const auto& [pid, data] : page_cache) {
            file.seekp(static_cast<std::streamoff>(pid * BTREE_PAGE_SIZE), std::ios::beg);
            file.write(reinterpret_cast<const char*>(data.data()), BTREE_PAGE_SIZE);
        }
        page_cache.clear();
        file.flush();
    } else {
        Status st = read_meta();
        if (!st.ok()) return st;
    }

    tree.root = meta.root_id;
    freelist.head_page = meta.free_head_page;
    freelist.head_seq = meta.free_head_seq;
    freelist.tail_page = meta.free_tail_page;
    freelist.tail_seq = meta.free_tail_seq;

    return Status::OK();
}

void KV::close() {
    if (file.is_open()) {
        file.flush();
        file.close();
    }
}

Status KV::read_meta() {
    auto p0 = read_page(0);
    auto p1 = read_page(1);

    auto parse_meta = [](const std::vector<uint8_t>& p, MetaPage& m) -> bool {
        if (std::memcmp(p.data(), KV_MAGIC, 14) != 0) return false;
        m.root_id = decode_u64(p.data() + 16);
        m.total_pages = decode_u64(p.data() + 24);
        m.free_head_page = decode_u64(p.data() + 32);
        m.free_head_seq = decode_u64(p.data() + 40);
        m.free_tail_page = decode_u64(p.data() + 48);
        m.free_tail_seq = decode_u64(p.data() + 56);
        m.commit_seq = decode_u64(p.data() + 64);
        return true;
    };

    MetaPage m0, m1;
    bool v0 = parse_meta(p0, m0);
    bool v1 = parse_meta(p1, m1);

    if (!v0 && !v1) {
        return Status::Corrupted("Invalid database header in both meta pages");
    }

    if (v0 && v1) {
        meta = (m0.commit_seq >= m1.commit_seq) ? m0 : m1;
    } else if (v0) {
        meta = m0;
    } else {
        meta = m1;
    }

    return Status::OK();
}

Status KV::write_meta() {
    std::vector<uint8_t> p(BTREE_PAGE_SIZE, 0);
    std::memcpy(p.data(), KV_MAGIC, 14);
    encode_u64(p.data() + 16, meta.root_id);
    encode_u64(p.data() + 24, meta.total_pages);
    encode_u64(p.data() + 32, meta.free_head_page);
    encode_u64(p.data() + 40, meta.free_head_seq);
    encode_u64(p.data() + 48, meta.free_tail_page);
    encode_u64(p.data() + 56, meta.free_tail_seq);
    encode_u64(p.data() + 64, meta.commit_seq);

    PageID target_meta_page = (meta.commit_seq % 2 == 0) ? 0 : 1;
    write_page(target_meta_page, p);
    return Status::OK();
}

std::vector<uint8_t> KV::read_page(PageID page_id) {
    auto it = page_cache.find(page_id);
    if (it != page_cache.end()) {
        return it->second;
    }

    std::vector<uint8_t> buf(BTREE_PAGE_SIZE, 0);
    file.seekg(static_cast<std::streamoff>(page_id * BTREE_PAGE_SIZE), std::ios::beg);
    file.read(reinterpret_cast<char*>(buf.data()), BTREE_PAGE_SIZE);
    return buf;
}

void KV::write_page(PageID page_id, const std::vector<uint8_t>& data) {
    page_cache[page_id] = data;
}

PageID KV::alloc_page(const std::vector<uint8_t>& data) {
    PageID reused_id = freelist.pop_head();
    PageID new_id = reused_id;

    if (new_id == 0) {
        new_id = meta.total_pages++;
    }

    write_page(new_id, data);
    return new_id;
}

void KV::free_page(PageID page_id) {
    if (page_id >= 2) {
        pending_freed_pages.push_back(page_id);
    }
}

Status KV::get(const Key& key, ValueData& val_out) {
    std::lock_guard<std::mutex> lock(kv_mutex);
    if (tree.get(key, val_out)) {
        return Status::OK();
    }
    return Status::NotFound();
}

Status KV::set(const Key& key, const ValueData& val) {
    std::lock_guard<std::mutex> lock(kv_mutex);
    tree.insert(key, val);
    return Status::OK();
}

Status KV::del(const Key& key) {
    std::lock_guard<std::mutex> lock(kv_mutex);
    if (tree.delete_key(key)) {
        return Status::OK();
    }
    return Status::NotFound();
}

Status KV::commit() {
    std::lock_guard<std::mutex> lock(kv_mutex);

    for (PageID pid : pending_freed_pages) {
        freelist.push_tail(pid);
    }
    pending_freed_pages.clear();

    meta.root_id = tree.root;
    meta.free_head_page = freelist.head_page;
    meta.free_head_seq = freelist.head_seq;
    meta.free_tail_page = freelist.tail_page;
    meta.free_tail_seq = freelist.tail_seq;
    meta.commit_seq++;

    write_meta();

    for (const auto& [pid, data] : page_cache) {
        file.seekp(static_cast<std::streamoff>(pid * BTREE_PAGE_SIZE), std::ios::beg);
        file.write(reinterpret_cast<const char*>(data.data()), BTREE_PAGE_SIZE);
    }
    page_cache.clear();
    file.flush();

    return Status::OK();
}

} // namespace dbms
