# Audit before implementation — 2026-10-01

Repository source of truth: `marceloroldao/resolutive-DB`.

- Main HEAD: `9ab47205cbc09553ecd00a86fd809a7e24d63182`, confirmed via GitHub commits API and `git ls-remote`.
- Latest stable GitHub release: `v1.1.0` (latest-release API).
- Latest candidate: `v1.2.0-rc4`, `317882a00f041fc1568ff986af8016b09453f21a`.
- Research branch: `experiment/address-scaling-2026-10-01`.
- Repository-wide text search covered complexity, hierarchy, collisions, fingerprints, adversarial tests and mature flat-map dependencies; inventory is recorded with raw evidence. No AGENTS.md was present. No C++ SwissTable/Abseil/ankerl/tsl mature flat-map dependency was found. Python dict controls and native project Robin Hood controls are present.

## Seven answers, before implementation

1. **Closest to approximately constant latency:** V2 fixed-M and, more fairly, V2 vs same-SHA flat hash. Same-SHA GET hit: 2.156 us at 10K and 2.041 us at 2M; intervening values 2.270, 2.367, 2.332, 2.436 us. The flat comparator also remained approximately flat and was faster. V4 is the closest compiled Robin Hood precursor, with competitive insertion and hit latency, but only 10K/100K/300K in its original report.
2. **Demonstrated:** direct rho routing plus expected/amortized local hashing avoids proportional scanning of a growing rho bucket on the measured synthetic keys. No asymptotic advantage over conventional hashing follows. V4's 300K hit median was 0.299 us versus 0.298 us for unordered_map, with p99 0.706 vs 0.763 us in that run.
3. **Not demonstrated:** strict worst-case O(1), general bounded latency, realistic adversarial guarantees, exact collision-safe identity in every historical benchmark, cache-miss benefit, or a general hierarchy advantage. V1 scans O(k); V5 binary-searches O(log k). V3 found overwrites under reduced phase/signature width even while latency stayed flat. V4's original fixed local capacity can overflow/nonterminate on pathological partition concentration.
4. **Why V18 was not recommended for incorporation:** hierarchical refinement improved a weak scan leaf under concentration but added work/memory under uniform data; strong same-hash flat/Robin Hood/sharded controls and adversarial evidence were required. The historical source/report now being present in main does not mean V18 hierarchy became the production index.
5. **Later evidence:** `bdr_v18_matched_hash_controls.cpp`, `bdr_v18_partitioned_robinhood_control.cpp`, V19 and V20 directly address part of that gap. V19 hot-rho: global Robin Hood ~0.118 us, partitioned Robin Hood ~0.124 us, adaptive phi split ~0.272 us and ~224 B/record. V20 removed unconditional phi split and selected target-density rho/local Robin Hood; at 1M target256 ~0.294 us versus global ~0.291 us. Later V21–V35 investigate durable WAL/concurrency, not a new proof of constant lookup. V36 and the stable compact index address memory layout. No later report found resolves operation-count scaling at 10M under controlled address collapse.
6. **Unanswered:** routing vs actual probes vs latency across multiple decades of N; survival of exact identity under fingerprint pressure; when adaptive routing helps over the same growing local Robin Hood; cache behavior and whether benefits justify complexity.
7. **Smallest new experiment:** keep a common encoder and exact-ID Robin Hood leaf; vary only direct fixed-depth routing versus bounded adaptive refinement, compare global RH/unordered_map, then separately time unchanged production indexes. Measure depth/probes/population independently from instrument-free latency. Normal, prefix, hot-query, rho-collapse, rho+phi-collapse, short-fingerprint and total-routing-collapse cases. 100K/1M/10M, except short-fingerprint/poor-hash insertion limited to 100K for quadratic work safety. Include depth/threshold sensitivity; no merge recommendation based on novelty.

## Important current-core distinction

The production APIs do not all use one index:

- `experimental/api_v86/CMakeLists.txt` builds stable `bdr::bdr` from `src/database_v1.cpp`, which selects **CompactIndex**, a growing partitioned Robin Hood index with a byte arena and exact key checks; fallback macro selects old **ResolutiveIndex**.
- Legacy `ResolutiveIndex` source is byte-identical between stable v1.1.0 and main (SHA-256 `7b3de31f2a6b6b3c823159e6783356d34dfaad1814927787f73e91822cb7a290`). It stores key/value strings in slots, has shared partition locks and local growth.
- `AtomicDatabase::Impl` delegates to `v110::DurableDatabase`; `durable_database.hpp` declares **std::map<std::string,std::string> state_**. GET uses its ordered state with a mutex. The atomic path therefore has O(log N) tree comparisons, plus variable-length key comparison; it must not inherit an O(1) description from the other index.
- Python reference `bdr/core.py` uses a direct bucket array, phase dictionary and fingerprint dictionary. It is a different implementation from both native paths.
- WAL, snapshot, recovery and ABI evolution do not in themselves prove index complexity.

## Evidence links

- [V1](../../docs/benchmarks/2026-08-09_user_bdr_v1.md)
- [V2 density](../../docs/benchmarks/2026-08-09_bdr_v2_density.md)
- [V2 same SHA](../../docs/benchmarks/2026-08-09_bdr_v2_vs_flat_hash_same_sha.md)
- [V3 failures](../../docs/benchmarks/2026-08-09_bdr_v3_collision_stress.md)
- [V4](../../docs/benchmarks/2026-08-09_bdr_v4_cpp_robinhood.md)
- [V5](../../docs/benchmarks/2026-08-09_bdr_v5_compact_zipf.md)
- [V18 design](../../docs/experiments/hierarchical_resolutive_addressing.md)
- [V18 results](../../docs/experiments/hierarchical_resolutive_addressing_results_2026-08-14.md)
- [V19/V20 selection](../../docs/experiments/v02_engine_selection_2026-08-14.md)
- [Later warm-index limitation](../../docs/experiments/v34_v35b_scale_mixed_results_2026-08-17.md)

No historical evidence or production code is rewritten by this experiment.
