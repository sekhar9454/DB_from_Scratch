#include "dbms/btree.h"
#include <cassert>
#include <algorithm>

namespace dbms {

BTree::BTree() : root(0) {}

BTree::BTree(PageID root_id, GetPageFn get_fn, AllocPageFn alloc_fn, DelPageFn del_fn)
    : root(root_id), get_page(std::move(get_fn)), alloc_page(std::move(alloc_fn)), del_page(std::move(del_fn)) {}

bool BTree::get(const Key& key, ValueData& val_out) {
    if (root == 0) return false;

    PageID current_id = root;
    while (current_id != 0) {
        std::vector<uint8_t> page_data = get_page(current_id);
        BNode node(page_data);
        uint16_t idx = node.node_lookup_le(key);

        if (node.btype() == BNODE_LEAF) {
            if (node.nkeys() > 0 && node.get_key(idx) == key) {
                val_out = node.get_val(idx);
                return true;
            }
            return false;
        } else {
            current_id = node.get_ptr(idx);
        }
    }
    return false;
}

void BTree::insert(const Key& key, const ValueData& val) {
    if (root == 0) {
        std::vector<uint8_t> page(BTREE_PAGE_SIZE, 0);
        BNode node(page);
        node.set_header(BNODE_LEAF, 1);
        node_append_kv(node, 0, 0, key, val);
        root = alloc_page(page);
        return;
    }

    std::vector<uint8_t> root_data = get_page(root);
    BNode root_node(root_data);
    PageID new_root_id = tree_insert(root_node, key, val);

    std::vector<uint8_t> new_root_data = get_page(new_root_id);
    BNode new_root_node(new_root_data);

    if (new_root_node.nbytes() > BTREE_PAGE_SIZE) {
        // Root split needed
        std::vector<uint8_t> left_page(BTREE_PAGE_SIZE, 0);
        std::vector<uint8_t> right_page(BTREE_PAGE_SIZE, 0);
        BNode left(left_page);
        BNode right(right_page);

        BNode::node_split3(left, right, new_root_node);

        PageID left_id = alloc_page(left_page);
        PageID right_id = alloc_page(right_page);

        std::vector<uint8_t> split_root_page(BTREE_PAGE_SIZE, 0);
        BNode split_root(split_root_page);
        split_root.set_header(BNODE_NODE, 2);
        node_append_kv(split_root, 0, left_id, left.get_key(0), ValueData());
        node_append_kv(split_root, 1, right_id, right.get_key(0), ValueData());

        root = alloc_page(split_root_page);
    } else {
        root = new_root_id;
    }
}

PageID BTree::tree_insert(BNode node, const Key& key, const ValueData& val) {
    uint16_t idx = node.node_lookup_le(key);

    if (node.btype() == BNODE_LEAF) {
        if (node.nkeys() > 0 && node.get_key(idx) == key) {
            return do_leaf_update(node, idx, key, val);
        } else {
            uint16_t insert_idx = (node.nkeys() > 0 && key > node.get_key(idx)) ? idx + 1 : idx;
            return do_leaf_insert(node, insert_idx, key, val);
        }
    } else {
        return insert_internal(node, idx, key, val);
    }
}

PageID BTree::do_leaf_insert(BNode node, uint16_t idx, const Key& key, const ValueData& val) {
    std::vector<uint8_t> new_page(BTREE_PAGE_SIZE * 2, 0);
    BNode new_node(new_page);
    BNode::leaf_insert(new_node, node, idx, key, val);
    return alloc_page(new_page);
}

PageID BTree::do_leaf_update(BNode node, uint16_t idx, const Key& key, const ValueData& val) {
    std::vector<uint8_t> new_page(BTREE_PAGE_SIZE * 2, 0);
    BNode new_node(new_page);
    BNode::leaf_update(new_node, node, idx, key, val);
    return alloc_page(new_page);
}

PageID BTree::insert_internal(BNode node, uint16_t idx, const Key& key, const ValueData& val) {
    PageID child_id = node.get_ptr(idx);
    std::vector<uint8_t> child_data = get_page(child_id);
    BNode child_node(child_data);

    PageID new_child_id = tree_insert(child_node, key, val);
    std::vector<uint8_t> new_child_data = get_page(new_child_id);
    BNode new_child_node(new_child_data);

    if (new_child_node.nbytes() > BTREE_PAGE_SIZE) {
        // Split child into two nodes
        std::vector<uint8_t> left_page(BTREE_PAGE_SIZE, 0);
        std::vector<uint8_t> right_page(BTREE_PAGE_SIZE, 0);
        BNode left(left_page);
        BNode right(right_page);

        BNode::node_split3(left, right, new_child_node);

        PageID left_id = alloc_page(left_page);
        PageID right_id = alloc_page(right_page);

        // Replace child entry with left and right in parent
        std::vector<uint8_t> new_parent_page(BTREE_PAGE_SIZE * 2, 0);
        BNode new_parent(new_parent_page);
        
        // Prepare temporary node with left replace
        BNode::node_replace2(new_parent, node, idx, left.get_key(0), left_id);
        
        // Insert right child
        std::vector<uint8_t> final_parent_page(BTREE_PAGE_SIZE * 2, 0);
        BNode final_parent(final_parent_page);
        BNode::node_insert(final_parent, new_parent, idx + 1, right.get_key(0), right_id);

        return alloc_page(final_parent_page);
    } else {
        // Update child pointer and child key in parent node
        std::vector<uint8_t> new_parent_page(BTREE_PAGE_SIZE * 2, 0);
        BNode new_parent(new_parent_page);
        BNode::node_replace2(new_parent, node, idx, new_child_node.get_key(0), new_child_id);
        return alloc_page(new_parent_page);
    }
}

bool BTree::delete_key(const Key& key) {
    if (root == 0) return false;

    std::vector<uint8_t> root_data = get_page(root);
    BNode root_node(root_data);

    bool deleted = false;
    PageID new_root_id = tree_delete(root_node, key, deleted);
    if (!deleted) return false;

    if (new_root_id != 0) {
        std::vector<uint8_t> new_root_data = get_page(new_root_id);
        BNode new_root_node(new_root_data);
        if (new_root_node.btype() == BNODE_NODE && new_root_node.nkeys() == 1) {
            root = new_root_node.get_ptr(0);
        } else if (new_root_node.btype() == BNODE_LEAF && new_root_node.nkeys() == 0) {
            root = 0;
        } else {
            root = new_root_id;
        }
    } else {
        root = 0;
    }
    return true;
}

PageID BTree::tree_delete(BNode node, const Key& key, bool& deleted_out) {
    uint16_t idx = node.node_lookup_le(key);

    if (node.btype() == BNODE_LEAF) {
        if (node.nkeys() > 0 && node.get_key(idx) == key) {
            deleted_out = true;
            return do_leaf_delete(node, idx, key, deleted_out);
        }
        deleted_out = false;
        return 0;
    } else {
        return delete_internal(node, idx, key, deleted_out);
    }
}

PageID BTree::do_leaf_delete(BNode node, uint16_t idx, const Key& key, bool& deleted_out) {
    if (node.nkeys() == 1) {
        deleted_out = true;
        return 0;
    }
    std::vector<uint8_t> new_page(BTREE_PAGE_SIZE, 0);
    BNode new_node(new_page);
    BNode::leaf_delete(new_node, node, idx);
    return alloc_page(new_page);
}

PageID BTree::delete_internal(BNode node, uint16_t idx, const Key& key, bool& deleted_out) {
    PageID child_id = node.get_ptr(idx);
    std::vector<uint8_t> child_data = get_page(child_id);
    BNode child_node(child_data);

    PageID new_child_id = tree_delete(child_node, key, deleted_out);
    if (!deleted_out) return 0;

    std::vector<uint8_t> new_parent_page(BTREE_PAGE_SIZE, 0);
    BNode new_parent(new_parent_page);

    if (new_child_id == 0) {
        // Child became empty, remove child pointer from parent
        if (node.nkeys() == 1) {
            return 0;
        }
        std::vector<uint8_t> page(BTREE_PAGE_SIZE, 0);
        BNode nnode(page);
        BNode::leaf_delete(nnode, node, idx); // remove idx entry
        nnode.set_header(BNODE_NODE, node.nkeys() - 1);
        return alloc_page(page);
    }

    std::vector<uint8_t> new_child_data = get_page(new_child_id);
    BNode new_child_node(new_child_data);

    // Merge check: if child node merged/underflows, rebalance if needed
    BNode::node_replace2(new_parent, node, idx, new_child_node.get_key(0), new_child_id);
    return alloc_page(new_parent_page);
}

// --- Iterator Implementation ---

BIter::BIter() : tree(nullptr) {}

BIter::BIter(BTree* tree_ptr, std::vector<PageID> path_nodes, std::vector<uint16_t> pos_indices)
    : tree(tree_ptr), path(std::move(path_nodes)), pos(std::move(pos_indices)) {}

bool BIter::valid() const {
    if (!tree || path.empty() || pos.empty()) return false;
    std::vector<uint8_t> leaf_data = tree->get_page(path.back());
    BNode leaf(leaf_data);
    return pos.back() < leaf.nkeys();
}

void BIter::next() {
    if (!valid()) return;

    pos.back()++;
    std::vector<uint8_t> leaf_data = tree->get_page(path.back());
    BNode leaf(leaf_data);

    if (pos.back() < leaf.nkeys()) {
        return; // still inside current leaf
    }

    // Step up to parent to find next leaf node
    while (path.size() > 1) {
        path.pop_back();
        pos.pop_back();

        std::vector<uint8_t> parent_data = tree->get_page(path.back());
        BNode parent(parent_data);
        pos.back()++;

        if (pos.back() < parent.nkeys()) {
            // Descend into leftmost child of next subtree
            PageID curr_id = parent.get_ptr(pos.back());
            while (true) {
                path.push_back(curr_id);
                pos.push_back(0);

                std::vector<uint8_t> node_data = tree->get_page(curr_id);
                BNode node(node_data);
                if (node.btype() == BNODE_LEAF) {
                    break;
                }
                curr_id = node.get_ptr(0);
            }
            return;
        }
    }

    // Iteration complete
    path.clear();
    pos.clear();
}

void BIter::prev() {
    if (path.empty()) return;

    if (pos.back() > 0) {
        pos.back()--;
        return;
    }

    while (path.size() > 1) {
        path.pop_back();
        pos.pop_back();

        if (pos.back() > 0) {
            pos.back()--;
            std::vector<uint8_t> parent_data = tree->get_page(path.back());
            BNode parent(parent_data);

            PageID curr_id = parent.get_ptr(pos.back());
            while (true) {
                path.push_back(curr_id);

                std::vector<uint8_t> node_data = tree->get_page(curr_id);
                BNode node(node_data);
                pos.push_back(node.nkeys() > 0 ? node.nkeys() - 1 : 0);

                if (node.btype() == BNODE_LEAF) {
                    break;
                }
                curr_id = node.get_ptr(node.nkeys() > 0 ? node.nkeys() - 1 : 0);
            }
            return;
        }
    }

    path.clear();
    pos.clear();
}

bool BIter::deref(Key& key_out, ValueData& val_out) {
    if (!valid()) return false;
    std::vector<uint8_t> leaf_data = tree->get_page(path.back());
    BNode leaf(leaf_data);
    key_out = leaf.get_key(pos.back());
    val_out = leaf.get_val(pos.back());
    return true;
}

BIter BTree::seek_le(const Key& key) {
    if (root == 0) return BIter();

    std::vector<PageID> path_nodes;
    std::vector<uint16_t> pos_indices;

    PageID curr_id = root;
    while (curr_id != 0) {
        path_nodes.push_back(curr_id);
        std::vector<uint8_t> page_data = get_page(curr_id);
        BNode node(page_data);

        uint16_t idx = node.node_lookup_le(key);
        pos_indices.push_back(idx);

        if (node.btype() == BNODE_LEAF) {
            break;
        }
        curr_id = node.get_ptr(idx);
    }

    return BIter(this, path_nodes, pos_indices);
}

BIter BTree::seek_ge(const Key& key) {
    BIter iter = seek_le(key);
    if (!iter.valid()) {
        return seek_first();
    }
    Key k; ValueData v;
    if (iter.deref(k, v) && k < key) {
        iter.next();
    }
    return iter;
}

BIter BTree::seek_first() {
    if (root == 0) return BIter();

    std::vector<PageID> path_nodes;
    std::vector<uint16_t> pos_indices;

    PageID curr_id = root;
    while (curr_id != 0) {
        path_nodes.push_back(curr_id);
        pos_indices.push_back(0);

        std::vector<uint8_t> page_data = get_page(curr_id);
        BNode node(page_data);
        if (node.btype() == BNODE_LEAF) {
            break;
        }
        curr_id = node.get_ptr(0);
    }

    return BIter(this, path_nodes, pos_indices);
}

} // namespace dbms
