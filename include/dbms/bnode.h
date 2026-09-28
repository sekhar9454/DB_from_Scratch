#ifndef DBMS_BNODE_H
#define DBMS_BNODE_H

#include "types.h"

namespace dbms {

class BNode {
public:
    uint8_t* data;

    explicit BNode(uint8_t* ptr = nullptr) : data(ptr) {}
    explicit BNode(std::vector<uint8_t>& vec) : data(vec.data()) {}

    bool is_null() const { return data == nullptr; }

    uint16_t btype() const;
    uint16_t nkeys() const;
    void set_header(uint16_t btype, uint16_t nkeys);

    PageID get_ptr(uint16_t idx) const;
    void set_ptr(uint16_t idx, PageID val);

    uint16_t get_offset(uint16_t idx) const;
    void set_offset(uint16_t idx, uint16_t val);

    size_t kv_pos(uint16_t idx) const;
    Key get_key(uint16_t idx) const;
    ValueData get_val(uint16_t idx) const;

    uint16_t nbytes() const;

    uint16_t node_lookup_le(const Key& key) const;

    static void leaf_insert(BNode new_node, BNode old_node, uint16_t idx, const Key& key, const ValueData& val);
    static void leaf_update(BNode new_node, BNode old_node, uint16_t idx, const Key& key, const ValueData& val);
    static void leaf_delete(BNode new_node, BNode old_node, uint16_t idx);

    static void node_insert(BNode new_node, BNode old_node, uint16_t idx, const Key& key, PageID ptr);
    static void node_replace2(BNode new_node, BNode old_node, uint16_t idx, const Key& key, PageID ptr);
    static void node_split3(BNode left, BNode right, BNode old_node);
    static void node_merge(BNode new_node, BNode left, BNode right);
};

void node_append_range(BNode new_node, BNode old_node, uint16_t dst_new, uint16_t src_old, uint16_t n);
void node_append_kv(BNode new_node, uint16_t idx, PageID ptr, const Key& key, const ValueData& val);

} // namespace dbms

#endif // DBMS_BNODE_H
