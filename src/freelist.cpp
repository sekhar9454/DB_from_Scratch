#include "dbms/freelist.h"
#include <cassert>

namespace dbms {

FreeList::FreeList()
    : head_page(0), head_seq(0), tail_page(0), tail_seq(0), max_seq(0) {}

FreeList::FreeList(GetPageFn get_fn, AllocPageFn alloc_fn, std::function<void(PageID, const std::vector<uint8_t>&)> set_fn)
    : head_page(0), head_seq(0), tail_page(0), tail_seq(0), max_seq(0),
      get_page(std::move(get_fn)), alloc_page(std::move(alloc_fn)), set_page(std::move(set_fn)) {}

PageID FreeList::pop_head() {
    if (head_seq >= tail_seq) {
        return 0; // Free list is empty
    }

    std::vector<uint8_t> page_data = get_page(head_page);
    size_t idx = head_seq % FREE_LIST_CAP;
    PageID ptr = decode_u64(page_data.data() + FREE_LIST_HEADER + idx * 8);

    head_seq++;
    if (head_seq % FREE_LIST_CAP == 0) {
        head_page = decode_u64(page_data.data()); // advance to next LNode page
    }

    return ptr;
}

void FreeList::push_tail(PageID ptr) {
    size_t idx = tail_seq % FREE_LIST_CAP;

    if (idx == 0) {
        std::vector<uint8_t> new_lnode_page(BTREE_PAGE_SIZE, 0);
        PageID new_page_id = alloc_page(new_lnode_page);

        if (tail_page != 0) {
            std::vector<uint8_t> old_tail_data = get_page(tail_page);
            encode_u64(old_tail_data.data(), new_page_id); // set next pointer
            set_page(tail_page, old_tail_data);
        }

        tail_page = new_page_id;
        if (head_page == 0) {
            head_page = new_page_id;
        }
    }

    std::vector<uint8_t> tail_data = get_page(tail_page);
    encode_u64(tail_data.data() + FREE_LIST_HEADER + idx * 8, ptr);
    set_page(tail_page, tail_data);

    tail_seq++;
}

} // namespace dbms
