cmake_minimum_required(VERSION 3.14)
project(DBMS_Cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(SOURCES
    src/bnode.cpp
    src/btree.cpp
    src/freelist.cpp
    src/kv.cpp
    src/schema.cpp
    src/encoding.cpp
    src/table.cpp
    src/tx.cpp
    src/lexer.cpp
    src/parser.cpp
    src/executor.cpp
)

add_library(dbms_lib STATIC ${SOURCES})

add_executable(dbms_cli src/main.cpp)
target_link_libraries(dbms_cli PRIVATE dbms_lib)

add_executable(dbms_tests tests/test_main.cpp)
target_link_libraries(dbms_tests PRIVATE dbms_lib)
