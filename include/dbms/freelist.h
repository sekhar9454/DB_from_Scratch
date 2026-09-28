#ifndef DBMS_FREELIST_H
#define DBMS_FREELIST_H

#include "types.h"
#include "btree.h"
#include <functional>
#include <vector>

namespace dbms {

constexpr size_t FREE_LIST_HEADER = 8;
constexpr size_t FREE_LIST_CAP = (BTREE_PAGE_SIZE - FREE_LIST_HEADER) / 8; // 511 pointers per page

class FreeList {
public:
    uint64_t head_page;
    uint64_t head_seq;
    uint64_t tail_page;
    uint64_t tail_seq;
    uint64_t max_seq;

    GetPageFn get_page;
    AllocPageFn alloc_page;
    std::function<void(PageID, const std::vector<uint8_t>&)> set_page;

    FreeList();
    FreeList(GetPageFn get_fn, AllocPageFn alloc_fn, std::function<void(PageID, const std::vector<uint8_t>&)> set_fn);

    PageID pop_head();
    void push_tail(PageID ptr);
};

} // namespace dbms

#endif // DBMS_FREELIST_H
