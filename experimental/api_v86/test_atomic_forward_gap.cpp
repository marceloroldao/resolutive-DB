#include "../api_v101/atomic_wal.hpp"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    using bdr::v101::OpType;
    using bdr::v101::Operation;

    const auto first = bdr::v101::encode_batch(
        1, {Operation{OpType::Put, "a", "1"}});
    const auto third = bdr::v101::encode_batch(
        3, {Operation{OpType::Put, "b", "2"}});

    std::vector<std::uint8_t> bytes;
    bytes.reserve(first.size() + third.size());
    bytes.insert(bytes.end(), first.begin(), first.end());
    bytes.insert(bytes.end(), third.begin(), third.end());

    std::map<std::string, std::string> state;
    try {
        (void)bdr::v101::replay(bytes, state, 0);
    } catch (const std::runtime_error& exc) {
        if (std::string(exc.what()) == "BDW4 sequence gap") {
            std::cout << "BDW4 forward gap rejection PASS\n";
            return 0;
        }
        std::cerr << "unexpected recovery error: " << exc.what() << "\n";
        return 1;
    }

    std::cerr << "forward sequence gap was incorrectly accepted\n";
    return 1;
}
