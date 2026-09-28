#include "dbms/types.h"
#include "dbms/bnode.h"
#include "dbms/btree.h"
#include "dbms/freelist.h"
#include "dbms/kv.h"
#include "dbms/schema.h"
#include "dbms/encoding.h"
#include "dbms/table.h"
#include "dbms/tx.h"
#include "dbms/parser.h"
#include "dbms/executor.h"

#include <iostream>
#include <cassert>
#include <filesystem>

void test_bnode() {
    std::cout << "[TEST] BNode binary layout & leaf insertion... ";
    std::vector<uint8_t> page(dbms::BTREE_PAGE_SIZE, 0);
    dbms::BNode node(page);

    std::vector<uint8_t> new_page(dbms::BTREE_PAGE_SIZE, 0);
    dbms::BNode new_node(new_page);

    dbms::Key k1 = { 'k', 'e', 'y', '1' };
    dbms::ValueData v1 = { 'v', 'a', 'l', '1' };
    dbms::BNode::leaf_insert(new_node, node, 0, k1, v1);

    assert(new_node.btype() == dbms::BNODE_LEAF);
    assert(new_node.nkeys() == 1);
    assert(new_node.get_key(0) == k1);
    assert(new_node.get_val(0) == v1);
    std::cout << "PASSED\n";
}

void test_btree() {
    std::cout << "[TEST] In-memory Copy-on-Write B+Tree operations... ";
    std::unordered_map<dbms::PageID, std::vector<uint8_t>> pages;
    dbms::PageID next_id = 1;

    dbms::BTree tree(0,
        [&](dbms::PageID id) { return pages[id]; },
        [&](const std::vector<uint8_t>& data) {
            dbms::PageID id = next_id++;
            pages[id] = data;
            return id;
        },
        [&](dbms::PageID id) { pages.erase(id); }
    );

    for (int i = 1; i <= 50; ++i) {
        dbms::Key k = { static_cast<uint8_t>(i) };
        dbms::ValueData v = { static_cast<uint8_t>(i * 2) };
        tree.insert(k, v);
    }

    for (int i = 1; i <= 50; ++i) {
        dbms::Key k = { static_cast<uint8_t>(i) };
        dbms::ValueData v;
        bool found = tree.get(k, v);
        assert(found);
        assert(v[0] == static_cast<uint8_t>(i * 2));
    }

    std::cout << "PASSED\n";
}

void test_kv_store() {
    std::cout << "[TEST] Persistent KV Store & FreeList recycling... ";
    std::string test_db = "test_kv.db";
    std::filesystem::remove(test_db);

    {
        dbms::KV kv;
        assert(kv.open(test_db).ok());

        for (int i = 1; i <= 100; ++i) {
            dbms::Key k = { static_cast<uint8_t>(i) };
            dbms::ValueData v = { static_cast<uint8_t>(i + 10) };
            assert(kv.set(k, v).ok());
        }
        assert(kv.commit().ok());
        kv.close();
    }

    {
        dbms::KV kv;
        assert(kv.open(test_db).ok());
        for (int i = 1; i <= 100; ++i) {
            dbms::Key k = { static_cast<uint8_t>(i) };
            dbms::ValueData v;
            assert(kv.get(k, v).ok());
            assert(v[0] == static_cast<uint8_t>(i + 10));
        }
        kv.close();
    }

    std::filesystem::remove(test_db);
    std::cout << "PASSED\n";
}

void test_sql_engine() {
    std::cout << "[TEST] SQL Parser, Table Definition & Query Execution... ";
    std::string test_db = "test_sql.db";
    std::filesystem::remove(test_db);

    {
        dbms::DB db;
        assert(db.open(test_db).ok());
        dbms::Executor exec(&db);

        // CREATE TABLE
        auto r1 = exec.execute("CREATE TABLE users (id INT, name TEXT, PRIMARY KEY (id));");
        assert(r1.message.find("created successfully") != std::string::npos);

        // INSERT
        auto r2 = exec.execute("INSERT INTO users VALUES (1, 'Alice');");
        assert(r2.rows_affected == 1);

        auto r3 = exec.execute("INSERT INTO users VALUES (2, 'Bob');");
        assert(r3.rows_affected == 1);

        // SELECT ALL
        auto r4 = exec.execute("SELECT * FROM users;");
        assert(r4.rows.size() == 2);

        // SELECT WHERE
        auto r5 = exec.execute("SELECT name FROM users WHERE id = 2;");
        assert(r5.rows.size() == 1);
        assert(r5.rows[0][0] == "Bob");

        // UPDATE
        auto r6 = exec.execute("UPDATE users SET name = 'Robert' WHERE id = 2;");
        assert(r6.rows_affected == 1);

        // DELETE
        auto r8 = exec.execute("DELETE FROM users WHERE id = 1;");
        assert(r8.rows_affected == 1);

        auto r9 = exec.execute("SELECT * FROM users;");
        assert(r9.rows.size() == 1);

        db.close();
    }

    std::filesystem::remove(test_db);
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "            Running DBMS System Test Suite              \n";
    std::cout << "========================================================\n";

    test_bnode();
    test_btree();
    test_kv_store();
    test_sql_engine();

    std::cout << "========================================================\n";
    std::cout << "            ALL UNIT TESTS PASSED SUCCESSFULLY!         \n";
    std::cout << "========================================================\n";
    return 0;
}
