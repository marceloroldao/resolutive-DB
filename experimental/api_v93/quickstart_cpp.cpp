#include "bdr/database.hpp"
#include <iostream>

int main() {
    auto db = bdr::Database::open("./v93_cpp_db");

    auto ticket = db->put("hello", "world");
    db->wait(ticket);

    auto value = db->get("hello");
    if (!value || *value != "world") {
        std::cerr << "unexpected value\n";
        return 1;
    }

    db->checkpoint();
    db->close();

    auto reopened = bdr::Database::open("./v93_cpp_db");
    auto value2 = reopened->get("hello");
    if (!value2 || *value2 != "world") return 2;
    reopened->close();

    std::cout << "V93 C++ quickstart PASS\n";
    return 0;
}
