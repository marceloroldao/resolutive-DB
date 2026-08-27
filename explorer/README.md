# Resolutive DB Explorer

Status: experimental v0.1 scaffold

The Resolutive DB Explorer is a read-only visual layer for BDR. Its first purpose is to make the resolutive address space observable without changing the stable database engine or depending on private internals.

## Design rules

1. **Do not modify the stable BDR v1 public contract.**
2. **Read through public APIs only.** The initial adapter uses `estatisticas()` and `entidades()`.
3. **Read-only first.** Administrative mutation, repair, migration and destructive actions are deliberately out of scope for v0.1.
4. **Visual semantics must correspond to real BDR fields.** The UI must not invent semantic relationships that the database does not store.
5. **Keep the module separable.** It lives inside the BDR repository during maturation but must be removable into its own repository later.
6. **Prepare for Memoria.ia integration.** The visual vocabulary should later accept memory nodes, occurrences, trajectories and semantic relations without coupling those concepts to the BDR core.

## v0.1 visual model

Each BDR entity is rendered as a resolutive node:

- radial distance: normalized `rho_R`;
- angular position: `theta`;
- node scale: derived from `f_nu`;
- node details: `id`, `rho_R`, `phi`, `theta`, `f_nu`, payload size, payload preview and fingerprint;
- global panel: public `estatisticas()` values.

No graph edge is drawn unless an actual relationship exists in a future public data contract.

## Architecture

```text
BDR public API
    |
    v
ExplorerSnapshotProvider
    |
    +--> /api/health
    +--> /api/snapshot
             |
             v
       static web UI
       - overview
       - resolutive map
       - entity inspector
```

The web server intentionally uses only Python's standard library in the first scaffold. This keeps the Explorer optional and prevents new runtime dependencies from contaminating the BDR engine.

## Run the demo

From the repository root:

```bash
python -m explorer.server --demo
```

Then open:

```text
http://127.0.0.1:8765
```

The demo creates a small in-memory BDR dataset and exposes it through the same adapter that a real database instance will use.

## Planned increments

### E01 — Read-only scaffold (this branch)

- public-API snapshot adapter;
- health endpoint;
- static visual shell;
- resolutive radial map;
- entity inspection;
- zero changes to the BDR core.

### E02 — Persistent BDR connection

- open an existing BDR instance in read-only/inspection mode;
- display persistence format and recovery state only through explicit public contracts;
- never parse private WAL/snapshot internals directly from the UI.

### E03 — Operational observability

Consume the future v1.3 public telemetry contract when available:

- WAL bytes;
- dirty mutations;
- checkpoint sequence/time;
- records/slots/index memory;
- durability mode.

### E04 — Memoria.ia visual overlay

Add a separate adapter for Memoria.ia concepts:

- logical memories;
- nodes and occurrences;
- trajectories;
- semantic relations;
- layer/update-time information.

The BDR map remains the storage/address substrate; Memoria.ia semantics remain a distinct overlay.

## Boundary with the stable engine

Explorer code must remain under `explorer/` (plus its own tests/workflow when needed). Any request for new engine observability must be implemented first as a documented backward-compatible BDR public API before the Explorer consumes it.
