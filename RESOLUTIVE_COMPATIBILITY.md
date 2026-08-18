# Resolutive Science Compatibility

This document records the normative relationship between Banco de Dados Resolutivo (BDR) and the Resolutive Science specification.

- Resolutive Science repository baseline: `v0.1.1`
- RSMS compatibility: `1.0-rc.1` — candidate compatibility
- Project governance baseline: `RSPS 1.0-draft`

## Scope of compatibility

BDR reuses Resolutive terminology and symbols as computational architecture hypotheses. In particular, objects such as `rho_R`, `phi`, `theta`, frequency/state metadata and resolutive addressing must not silently redefine a meaning already fixed by the applicable RSMS.

The computational semantics of BDR remain project-specific unless an RSMS definition explicitly applies. Benchmark success, deterministic addressing, persistence behavior or asymptotic observations in BDR do not constitute experimental validation of Resolutive Physics.

## Re-audit rule

This compatibility declaration must be re-audited when RSMS 1.0 becomes stable and before any future BDR release that claims stable compatibility with Resolutive Science.

Any intentional deviation from RSMS terminology or semantics must be documented and versioned rather than introduced silently.
