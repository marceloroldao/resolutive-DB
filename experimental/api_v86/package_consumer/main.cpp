#include <bdr/database.hpp>
#include <filesystem>
#include <iostream>

int main() {
    const auto dir = std::filesystem::temp_directory_path() / "bdr-v1-package-consumer";
    std::filesystem::remove_all(dir);

    auto db = bdr::Database::open(dir);
    db->put_sync("package", "ok");
    db->checkpoint();
    db->close();

    auto reopened = bdr::Database::open(dir);
    auto value = reopened->get("package");
    if (!value || *value != "ok") return 2;
    reopened->close();
    std::filesystem::remove_all(dir);

    std::cout << "V1_PACKAGE_CONSUMER PASS\n";
    return 0;
}
