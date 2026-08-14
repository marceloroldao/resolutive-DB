# BDR v0.1.0 — Experimental / Research Preview

First public research release of the Banco de Dados Resolutivo (BDR) / Resolutive Database Engine.

## Scope

BDR v0.1.0 is a reproducible research baseline for an experimental persistent key-value database architecture based on deterministic resolutive addressing, local partitioning, adaptive local structures, and crash-safe persistence.

This release is **not production-ready** and does **not** claim strict worst-case O(1) complexity for the complete engine.

## Highlights

- Deterministic resolutive addressing experiments using `rho_R`, phase-derived channels and independent fingerprints.
- In-memory baseline implementation and benchmark history against Python `dict`, binary search, flat hash maps and C++ `unordered_map`.
- Partitioned C++ prototypes with local Robin Hood hashing.
- Compact read-oriented per-partition representation.
- Adaptive Compact/Robin-Hood partition experiments.
- Persistent engine with `PUT`, `GET` and `DELETE`.
- Segmented append-only WAL with monotonic sequence numbers and CRC32 per record.
- Group commit and explicit durable writes.
- Versioned BDR2 snapshots with CRC32.
- Atomic checkpoint protocol using `fsync`, atomic rename and directory synchronization.
- Safe WAL retirement after committed checkpoints.
- Differential fuzz testing, soak testing, multiwriter testing and real POSIX `SIGKILL` crash-recovery validation.
- Python 3.11/3.12 CI and C++ compile/smoke gates.
- Durability-parity benchmark against SQLite with final-state equality verification.

## Scientific publication

Related preprint:

Marcelo Roldão Matos (2026). *Banco de Dados Resolutivo (BDR): Arquitetura Experimental de Endereçamento Densitário, Persistência Transacional e Avaliação Reprodutível*. Zenodo.

DOI: **10.5281/zenodo.21937842**

The preprint and software are separate citable objects. The preprint DOI is not the software-release DOI.

## Licensing

### Software

The software is distributed under the **BDR Academic and Non-Commercial Research License v1.0**.

- Academic, educational and non-commercial research use: permitted under the license terms.
- Commercial use, production deployment, proprietary integration, SaaS and monetized use: require a separate written commercial license from ETBRA Tecnologias.
- No patent license is granted.
- The software is source-available and is not represented as OSI-approved open source.

### Manuscript

The scientific manuscript is separately licensed under **CC BY-NC-SA 4.0**. That license does not apply to or expand the permissions granted for the software.

## Reproducibility

For scientific comparison, future versions must identify the exact release/tag and preserve v0.1.0 as the published baseline.

Recommended checks:

```bash
pip install -e .
pytest -q
python benchmarks/bdr_v01_sqlite_parity.py
```

See also:

- `CHANGELOG.md`
- `docs/DISK_FORMAT_V1.md`
- `paper/BDR_v0.1_preprint.md`
- `LICENSE`
- `paper/LICENSE`
- `CITATION.cff`

## Release classification

**Experimental / Research Preview**

This release is intended for reproducible research, independent benchmarking, review and further development.
