#include "dbms/bnode.h"
#include <algorithm>

namespace dbms {

uint16_t BNode::btype() const {
    return decode_u16(data);
}

uint16_t BNode::nkeys() const {
    return decode_u16(data + 2);
}

void BNode::set_header(uint16_t btype, uint16_t nkeys) {
    encode_u16(data, btype);
    encode_u16(data + 2, nkeys);
}

PageID BNode::get_ptr(uint16_t idx) const {
    assert(btype() == BNODE_NODE);
    assert(idx < nkeys());
    return decode_u64(data + 4 + idx * 8);
}

void BNode::set_ptr(uint16_t idx, PageID val) {
    assert(btype() == BNODE_NODE);
    assert(idx < nkeys());
    encode_u64(data + 4 + idx * 8, val);
}

static size_t offset_array_start(uint16_t btype, uint16_t nkeys) {
    return 4 + (btype == BNODE_NODE ? nkeys * 8 : 0);
}

uint16_t BNode::get_offset(uint16_t idx) const {
    if (idx == 0) return 0;
    size_t pos = offset_array_start(btype(), nkeys()) + (idx - 1) * 2;
    return decode_u16(data + pos);
}

void BNode::set_offset(uint16_t idx, uint16_t val) {
    if (idx == 0) return;
    size_t pos = offset_array_start(btype(), nkeys()) + (idx - 1) * 2;
    encode_u16(data + pos, val);
}

size_t BNode::kv_pos(uint16_t idx) const {
    uint16_t bt = btype();
    uint16_t nk = nkeys();
    size_t kv_header_size = 4 + (bt == BNODE_NODE ? nk * 8 : 0) + nk * 2;
    return kv_header_size + get_offset(idx);
}

Key BNode::get_key(uint16_t idx) const {
    assert(idx < nkeys());
    size_t pos = kv_pos(idx);
    uint16_t klen = decode_u16(data + pos);
    return Key(data + pos + 4, data + pos + 4 + klen);
}

ValueData BNode::get_val(uint16_t idx) const {
    assert(idx < nkeys());
    size_t pos = kv_pos(idx);
    uint16_t klen = decode_u16(data + pos);
    uint16_t vlen = decode_u16(data + pos + 2);
    return ValueData(data + pos + 4 + klen, data + pos + 4 + klen + vlen);
}

uint16_t BNode::nbytes() const {
    return static_cast<uint16_t>(kv_pos(nkeys()));
}

uint16_t BNode::node_lookup_le(const Key& key) const {
    uint16_t nk = nkeys();
    uint16_t found = 0;
    for (uint16_t i = 0; i < nk; ++i) {
        Key k = get_key(i);
        if (k <= key) {
            found = i;
        } else {
            break;
        }
    }
    return found;
}

void node_append_kv(BNode new_node, uint16_t idx, PageID ptr, const Key& key, const ValueData& val) {
    if (new_node.btype() == BNODE_NODE) {
        new_node.set_ptr(idx, ptr);
    }

    uint16_t prev_off = new_node.get_offset(idx);
    uint16_t klen = static_cast<uint16_t>(key.size());
    uint16_t vlen = static_cast<uint16_t>(val.size());
    uint16_t kv_len = 4 + klen + vlen;
    new_node.set_offset(idx + 1, prev_off + kv_len);

    size_t pos = new_node.kv_pos(idx);
    encode_u16(new_node.data + pos, klen);
    encode_u16(new_node.data + pos + 2, vlen);
    if (klen > 0) {
        std::memcpy(new_node.data + pos + 4, key.data(), klen);
    }
    if (vlen > 0) {
        std::memcpy(new_node.data + pos + 4 + klen, val.data(), vlen);
    }
}

void node_append_range(BNode new_node, BNode old_node, uint16_t dst_new, uint16_t src_old, uint16_t n) {
    assert(src_old + n <= old_node.nkeys());
    assert(dst_new + n <= new_node.nkeys());

    for (uint16_t i = 0; i < n; ++i) {
        uint16_t src_idx = src_old + i;
        uint16_t dst_idx = dst_new + i;
        PageID ptr = (old_node.btype() == BNODE_NODE) ? old_node.get_ptr(src_idx) : 0;
        node_append_kv(new_node, dst_idx, ptr, old_node.get_key(src_idx), old_node.get_val(src_idx));
    }
}

void BNode::leaf_insert(BNode new_node, BNode old_node, uint16_t idx, const Key& key, const ValueData& val) {
    new_node.set_header(BNODE_LEAF, old_node.nkeys() + 1);
    node_append_range(new_node, old_node, 0, 0, idx);
    node_append_kv(new_node, idx, 0, key, val);
    node_append_range(new_node, old_node, idx + 1, idx, old_node.nkeys() - idx);
}

void BNode::leaf_update(BNode new_node, BNode old_node, uint16_t idx, const Key& key, const ValueData& val) {
    new_node.set_header(BNODE_LEAF, old_node.nkeys());
    node_append_range(new_node, old_node, 0, 0, idx);
    node_append_kv(new_node, idx, 0, key, val);
    node_append_range(new_node, old_node, idx + 1, idx + 1, old_node.nkeys() - idx - 1);
}

void BNode::leaf_delete(BNode new_node, BNode old_node, uint16_t idx) {
    assert(old_node.nkeys() > 0);
    new_node.set_header(BNODE_LEAF, old_node.nkeys() - 1);
    node_append_range(new_node, old_node, 0, 0, idx);
    node_append_range(new_node, old_node, idx, idx + 1, old_node.nkeys() - idx - 1);
}

void BNode::node_insert(BNode new_node, BNode old_node, uint16_t idx, const Key& key, PageID ptr) {
    new_node.set_header(BNODE_NODE, old_node.nkeys() + 1);
    node_append_range(new_node, old_node, 0, 0, idx);
    node_append_kv(new_node, idx, ptr, key, ValueData());
    node_append_range(new_node, old_node, idx + 1, idx, old_node.nkeys() - idx);
}

void BNode::node_replace2(BNode new_node, BNode old_node, uint16_t idx, const Key& key, PageID ptr) {
    new_node.set_header(BNODE_NODE, old_node.nkeys());
    node_append_range(new_node, old_node, 0, 0, idx);
    node_append_kv(new_node, idx, ptr, key, ValueData());
    node_append_range(new_node, old_node, idx + 1, idx + 1, old_node.nkeys() - idx - 1);
}

void BNode::node_split3(BNode left, BNode right, BNode old_node) {
    uint16_t n = old_node.nkeys();
    uint16_t nleft = n / 2;
    uint16_t nright = n - nleft;

    left.set_header(old_node.btype(), nleft);
    right.set_header(old_node.btype(), nright);

    node_append_range(left, old_node, 0, 0, nleft);
    node_append_range(right, old_node, 0, nleft, nright);
}

void BNode::node_merge(BNode new_node, BNode left, BNode right) {
    assert(left.btype() == right.btype());
    uint16_t total = left.nkeys() + right.nkeys();
    new_node.set_header(left.btype(), total);
    node_append_range(new_node, left, 0, 0, left.nkeys());
    node_append_range(new_node, right, left.nkeys(), 0, right.nkeys());
}

} // namespace dbms
