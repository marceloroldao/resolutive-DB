#include "bdr/database.hpp"

#include <csignal>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>

namespace fs = std::filesystem;

int main() {
    const fs::path root = "v74-writer-io-failure-db";
    fs::remove_all(root);

    bdr::Options opt;
    opt.reserve_bytes = 0; // RLIMIT_FSIZE test must not fail at preallocation.
    opt.wal_batch = 1;
    opt.partition_count = 8;
    opt.partition_max_load = 0.78;

    auto db = bdr::Database::open(root, opt);

    struct rlimit old_limit{};
    if (::getrlimit(RLIMIT_FSIZE, &old_limit) != 0) {
        std::cerr << "getrlimit failed\n";
        return 2;
    }
    ::signal(SIGXFSZ, SIG_IGN);

    struct rlimit limited = old_limit;
    limited.rlim_cur = 256; // WAL header fits; the large frame does not.
    if (::setrlimit(RLIMIT_FSIZE, &limited) != 0) {
        std::cerr << "setrlimit failed\n";
        return 3;
    }

    auto ticket = db->put("large-key", std::string(4096, 'x'));

    bool wait_failed = false;
    try {
        db->wait(ticket);
    } catch (const std::exception& e) {
        wait_failed = true;
        std::cout << "wait_error=" << e.what() << "\n";
    }
    if (!wait_failed) {
        std::cerr << "wait unexpectedly succeeded after forced I/O failure\n";
        return 4;
    }

    bool submit_rejected = false;
    try {
        (void)db->put("after-failure", "must-not-be-accepted");
    } catch (const std::exception& e) {
        submit_rejected = true;
        std::cout << "submit_error=" << e.what() << "\n";
    }
    if (!submit_rejected) {
        std::cerr << "submit unexpectedly accepted after writer failure\n";
        return 5;
    }

    bool close_reported = false;
    try {
        db->close();
    } catch (const std::exception& e) {
        close_reported = true;
        std::cout << "close_error=" << e.what() << "\n";
    }
    if (!close_reported) {
        std::cerr << "close did not report outstanding writer failure\n";
        return 6;
    }

    // Restore the file-size limit before reopening and continuing normally.
    if (::setrlimit(RLIMIT_FSIZE, &old_limit) != 0) {
        std::cerr << "failed to restore RLIMIT_FSIZE\n";
        return 7;
    }

    auto reopened = bdr::Database::open(root, opt);
    if (reopened->last_sequence() != reopened->durable_sequence()) {
        std::cerr << "recovery frontier mismatch\n";
        return 8;
    }
    if (reopened->contains("large-key")) {
        std::cerr << "failed/non-durable record survived recovery\n";
        return 9;
    }

    reopened->put_sync("healthy", "ok");
    if (reopened->get("healthy") != std::optional<std::string>("ok")) {
        std::cerr << "post-recovery write failed\n";
        return 10;
    }
    reopened->checkpoint();
    reopened->close();

    auto final_db = bdr::Database::open(root, opt);
    if (final_db->get("healthy") != std::optional<std::string>("ok") || final_db->size() != 1) {
        std::cerr << "final reopen mismatch\n";
        return 11;
    }
    final_db->close();
    fs::remove_all(root);

    std::cout << "wait_failed,submit_rejected,close_reported,recovery_ok,post_recovery_write,pass\n";
    std::cout << "1,1,1,1,1,1\n";
    return 0;
}
