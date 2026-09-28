#ifndef DBMS_BTREE_H
#define DBMS_BTREE_H

#include "bnode.h"
#include <functional>
#include <vector>
#include <memory>

namespace dbms {

using GetPageFn = std::function<std::vector<uint8_t>(PageID)>;
using AllocPageFn = std::function<PageID(const std::vector<uint8_t>&)>;
using DelPageFn = std::function<void(PageID)>;

class BIter;

class BTree {
public:
    PageID root;
    GetPageFn get_page;
    AllocPageFn alloc_page;
    DelPageFn del_page;

    BTree();
    BTree(PageID root_id, GetPageFn get_fn, AllocPageFn alloc_fn, DelPageFn del_fn);

    // --- Core Operations ---
    bool get(const Key& key, ValueData& val_out);
    void insert(const Key& key, const ValueData& val);
    bool delete_key(const Key& key);

    // --- Iterators for Range Queries ---
    BIter seek_le(const Key& key);
    BIter seek_ge(const Key& key);
    BIter seek_first();

private:
    PageID tree_insert(BNode node, const Key& key, const ValueData& val);
    PageID tree_delete(BNode node, const Key& key, bool& deleted_out);

    PageID do_leaf_insert(BNode node, uint16_t idx, const Key& key, const ValueData& val);
    PageID do_leaf_update(BNode node, uint16_t idx, const Key& key, const ValueData& val);
    PageID insert_internal(BNode node, uint16_t idx, const Key& key, const ValueData& val);

    PageID do_leaf_delete(BNode node, uint16_t idx, const Key& key, bool& deleted_out);
    PageID delete_internal(BNode node, uint16_t idx, const Key& key, bool& deleted_out);
};

// Stateful Iterator over B+Tree leaves
class BIter {
public:
    BTree* tree;
    std::vector<PageID> path;
    std::vector<uint16_t> pos;

    BIter();
    BIter(BTree* tree_ptr, std::vector<PageID> path_nodes, std::vector<uint16_t> pos_indices);

    bool valid() const;
    void next();
    void prev();
    bool deref(Key& key_out, ValueData& val_out);
};

} // namespace dbms

#endif // DBMS_BTREE_H
