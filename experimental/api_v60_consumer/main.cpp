#include <bdr/database.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = "v60-consumer-db";
    fs::remove_all(dir);

    auto db = bdr::Database::open(dir);
    auto ticket = db->put("alpha", "A");
    db->wait(ticket);
    db->put_sync("beta", "B");

    if (db->get("alpha").value_or("") != "A") throw std::runtime_error("alpha lookup failed");
    if (db->get("beta").value_or("") != "B") throw std::runtime_error("beta lookup failed");

    db->erase_sync("alpha");
    if (db->get("alpha").has_value()) throw std::runtime_error("alpha delete failed");

    db->checkpoint();
    db->close();

    db = bdr::Database::open(dir);
    if (db->get("alpha").has_value()) throw std::runtime_error("deleted alpha resurrected");
    if (db->get("beta").value_or("") != "B") throw std::runtime_error("beta missing after reopen");
    db->close();

    std::cout << "install,find_package,put,wait,get,delete,checkpoint,reopen,pass\n"
              << "1,1,1,1,1,1,1,1,1\n";
    return 0;
}
