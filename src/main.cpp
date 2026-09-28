#include "dbms/table.h"
#include "dbms/executor.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string db_file = "mydb.db";
    if (argc > 1) {
        db_file = argv[1];
    }

    dbms::DB db;
    dbms::Status st = db.open(db_file);
    if (!st.ok()) {
        std::cerr << "Error opening database file: " << st.message << std::endl;
        return 1;
    }

    std::cout << "========================================================\n";
    std::cout << "         Modular C++ Relational DBMS (C++17)            \n";
    std::cout << "   Copy-On-Write B+Tree Engine & SQL Parser Engine      \n";
    std::cout << "   Opened database file: " << db_file << "\n";
    std::cout << "   Type 'exit;' or 'quit;' to exit the shell.           \n";
    std::cout << "========================================================\n\n";

    dbms::Executor executor(&db);

    std::string input;
    while (true) {
        std::cout << "dbms> ";
        if (!std::getline(std::cin, input)) break;

        // Strip leading/trailing whitespace
        size_t first = input.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) continue;
        input = input.substr(first);

        if (input == "exit;" || input == "quit;" || input == "exit" || input == "quit") {
            std::cout << "Bye!\n";
            break;
        }

        dbms::QueryResult res = executor.execute(input);
        res.print(std::cout);
    }

    db.close();
    return 0;
}
