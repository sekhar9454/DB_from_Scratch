# Modular C++ Relational DBMS Engine

A complete, production-grade, modular Relational Database Management System (DBMS) written from scratch in standard C++17. 

The architecture is built following a clean layered design:
- **Paged File Storage Engine**: Manages 4KB pages on disk with double-buffered meta/master pages for atomic crash safety.
- **Copy-on-Write (CoW) B+Tree**: Immutable B+Tree supporting page splits, rebalancing node merges, leaf range iterators (`seek_le`, `seek_ge`), point lookups, and range scans.
- **FreeList Allocator**: On-disk linked list manager (`LNode`) that recycles freed pages across commits to prevent disk space leaks.
- **Relational Row Encoding & Indexing**: Order-preserving lexicographical binary serialization for signed integers (`INT64`) and variable-length strings (`VARCHAR`/`TEXT`), primary key index management, and secondary index maintenance (`[IndexPrefix][IndexCols...][PrimaryKey...]`).
- **ACID Transactions & Concurrency**: Multi-key atomic transactions (`DBTX`/`KVTX`) with full `commit` / `abort` semantics and readers-writer (`std::shared_mutex`) concurrency control.
- **SQL Lexer, Parser & Query Engine**: Tokenizer, recursive-descent AST parser for DDL & DML, expression evaluator (`QLEvalContext`), range query scanner, and formatted ASCII table output.

---

## 🚀 Quick Start & Building

### Prerequisites
- C++17 compiler (`g++` 8+ or `clang++` 7+)
- CMake 3.14+
- Make or Ninja

### Build Instructions

```bash
# 1. Create build directory
mkdir -p build && cd build

# 2. Configure project with CMake
cmake ..

# 3. Compile static library, CLI shell, and test suite
make -j$(nproc)
```

This generates two main binaries in the `build/` directory:
- `dbms_cli`: The interactive SQL REPL shell.
- `dbms_tests`: The automated unit test suite.

---

## 🧪 Running Unit Tests

To verify that all storage, B+Tree, FreeList, transaction, and SQL engine components are working properly:

```bash
./build/dbms_tests
```

**Expected Output:**
```
========================================================
            Running DBMS System Test Suite              
========================================================
[TEST] BNode binary layout & leaf insertion... PASSED
[TEST] In-memory Copy-on-Write B+Tree operations... PASSED
[TEST] Persistent KV Store & FreeList recycling... PASSED
[TEST] SQL Parser, Table Definition & Query Execution... PASSED
========================================================
            ALL UNIT TESTS PASSED SUCCESSFULLY!         
========================================================
```

---

## 💻 Using the Interactive SQL Shell (`dbms_cli`)

Launch the shell with a database filename (defaults to `mydb.db` if omitted):

```bash
./build/dbms_cli my_company.db
```

### Interactive Session Examples

#### 1. Creating Tables (`CREATE TABLE`)
```sql
dbms> CREATE TABLE employees (id INT, name TEXT, salary INT, PRIMARY KEY (id));
Table 'employees' created successfully.
```

#### 2. Inserting Records (`INSERT INTO`)
```sql
dbms> INSERT INTO employees VALUES (1, 'Alice', 95000);
1 row inserted successfully.

dbms> INSERT INTO employees VALUES (2, 'Bob', 85000);
1 row inserted successfully.

dbms> INSERT INTO employees VALUES (3, 'Charlie', 110000);
1 row inserted successfully.
```

#### 3. Querying Data (`SELECT`)
Scan all rows:
```sql
dbms> SELECT * FROM employees;
+----+---------+--------+
| id | name    | salary |
+----+---------+--------+
| 1  | Alice   | 95000  |
| 2  | Bob     | 85000  |
| 3  | Charlie | 110000 |
+----+---------+--------+
(3 rows)
```

Query specific columns with `WHERE` filtering:
```sql
dbms> SELECT name, salary FROM employees WHERE salary > 90000;
+---------+--------+
| name    | salary |
+---------+--------+
| Alice   | 95000  |
| Charlie | 110000 |
+---------+--------+
(2 rows)
```

Combine logical conditions (`AND` / `OR` / `=` / `!=` / `<` / `>`):
```sql
dbms> SELECT * FROM employees WHERE salary >= 90000 AND name = 'Charlie';
+----+---------+--------+
| id | name    | salary |
+----+---------+--------+
| 3  | Charlie | 110000 |
+----+---------+--------+
(1 rows)
```

#### 4. Creating Secondary Indexes (`CREATE INDEX`)
Speed up queries on non-primary key columns:
```sql
dbms> CREATE INDEX idx_salary ON employees (salary);
Index created on employees
```

#### 5. Updating Records (`UPDATE`)
```sql
dbms> UPDATE employees SET salary = 100000 WHERE id = 1;
1 rows updated.

dbms> SELECT * FROM employees WHERE id = 1;
+----+-------+--------+
| id | name  | salary |
+----+-------+--------+
| 1  | Alice | 100000 |
+----+-------+--------+
(1 rows)
```

#### 6. Deleting Records (`DELETE`)
```sql
dbms> DELETE FROM employees WHERE id = 2;
1 rows deleted.

dbms> SELECT * FROM employees;
+----+---------+--------+
| id | name    | salary |
+----+---------+--------+
| 1  | Alice   | 100000 |
| 3  | Charlie | 110000 |
+----+---------+--------+
(2 rows)
```

#### 7. Exiting the Shell
```sql
dbms> exit;
Bye!
```

---

## 🛠️ Programmatic C++ API Usage

You can also link against `libdbms_lib.a` in your own C++ applications:

```cpp
#include "dbms/table.h"
#include "dbms/executor.h"
#include <iostream>

int main() {
    dbms::DB db;
    if (!db.open("app.db").ok()) {
        std::cerr << "Failed to open database\n";
        return 1;
    }

    dbms::Executor exec(&db);

    // Execute DDL statement
    exec.execute("CREATE TABLE products (id INT, title TEXT, price INT, PRIMARY KEY (id));");

    // Execute DML statement
    exec.execute("INSERT INTO products VALUES (101, 'Laptop', 1200);");

    // Execute SELECT query and print ASCII table to stdout
    dbms::QueryResult res = exec.execute("SELECT * FROM products WHERE price > 1000;");
    res.print(std::cout);

    db.close();
    return 0;
}
```

---

## 📁 Source Code Structure

```
DBMS_Cpp/
├── include/dbms/
│   ├── types.h        # Base types, endian helpers, Status codes
│   ├── bnode.h        # B+Tree node byte layout & CoW functions
│   ├── btree.h        # Copy-on-Write B+Tree engine & range iterators
│   ├── freelist.h     # On-disk FreeList page allocator
│   ├── kv.h           # Transactional KV store file manager
│   ├── schema.h       # Value, Record, and TableDef definitions
│   ├── encoding.h     # Order-preserving key & payload binary serialization
│   ├── table.h        # Relational table engine & secondary index manager
│   ├── tx.h           # Multi-key ACID transaction context & RWLock
│   ├── lexer.h        # SQL tokenizer
│   ├── sql_ast.h      # Abstract Syntax Tree nodes for DDL/DML & expressions
│   ├── parser.h       # Recursive-descent SQL parser
│   └── executor.h     # Query executor, expression evaluator & output formatter
├── src/               # Implementation files (.cpp)
├── tests/
│   └── test_main.cpp  # System unit tests
├── CMakeLists.txt     # CMake build script
└── README.md          # Project documentation & walkthrough
```
