# Validation record

- Main suite: 282 rows; supplemental depth/threshold/native-control suite: 99 rows; dedicated prefix recheck: 18 rows.
- 399 distinct (suite,N,workload,engine,seed) process configurations, exit status 0, no timeouts.
- Each row checks 20,000 seeded hits and 20,000 seeded misses. All sampled results match exact generated key identity. This is not exhaustive verification of every large table.
- Three seeds per group, 133 groups; summary uses median across runs.
- Full-configuration 1,000-record smoke also passed (temporary smoke output excluded from git).
- ASan+UBSan smoke at 5,000 records passed for original native index, current CompactIndex, ordered atomic-state type, fingerprint pressure, total-routing collapse, fixed3 and adaptive refinement.
- LeakSanitizer's default exit check failed because this container cannot inspect /proc task namespace under tracing. The address/undefined checks were rerun with `ASAN_OPTIONS=detect_leaks=0` and passed. No leak-check success is claimed.
- Figure rendered and inspected; summary/raw row uniqueness and complete expected counts asserted; git diff whitespace check passed.
- Production files, build configuration, APIs, C ABI, WAL, snapshots, recovery, tags and releases untouched. No stable test or CI result is invented; unchanged native index headers/sources compiled in the standalone harness.
- Hardware performance counters unavailable (perf executable absent); no measured cache-miss/cycle claim.
- Original prefix evidence retained; separate sequential recheck avoids the short initial overlap with compilation/smoke work.
