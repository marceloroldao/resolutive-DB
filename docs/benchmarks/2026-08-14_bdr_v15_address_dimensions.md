# BDR V15 — C++ address-dimension ablation

Date: 2026-08-14  
Branch: `experiment/v0.2-cpp-encoder`

## Goal

Measure the marginal effect of the resolutive address dimensions while keeping the same encoder family, key set, partition count and local container.

Variants:

1. `rho + fingerprint`
2. `rho + phi + fingerprint`
3. `rho + theta + fingerprint`
4. `rho + phi + theta + fingerprint`

Configuration: `M=10,000` rho partitions, `phi=65,536` buckets, `theta=65,536` buckets, 200,000 random hit lookups per run, three runs per N, compiled with `g++ -O3 -std=c++20`.

## Median lookup latency

| N | rho+fp | rho+phi+fp | rho+theta+fp | rho+phi+theta+fp |
|---:|---:|---:|---:|---:|
| 100,000 | 0.201769 us | 0.203026 us | **0.196231 us** | 0.200618 us |
| 1,000,000 | 0.799634 us | **0.737453 us** | 0.765986 us | 0.792357 us |
| 2,000,000 | 0.857075 us | 0.816902 us | 0.824120 us | **0.772092 us** |

Relative to `rho+fingerprint`, median lookup improvement was approximately:

- N=100k: theta +2.8%; full +0.6%; phi -0.6%.
- N=1M: phi +8.4%; theta +4.4%; full +0.9%.
- N=2M: full +11.0%; phi +4.9%; theta +4.0%.

## Interpretation

No single extra coordinate wins at every scale. The results do show that adding `phi` or `theta` does not necessarily impose a lookup penalty once N is large; in some runs it improves the local hash distribution/cache behavior. However, this benchmark does not prove a specifically resolutive advantage: the extra dimensions are derived from decorrelated fast hashes, and the observed gains may result from improved local key dispersion rather than any intrinsic physical interpretation of phase or orientation.

The scientifically defensible conclusion is therefore:

> Additional address dimensions can improve local lookup behavior in some density regimes, but V15 does not identify a universal winner or establish that `phi`/`theta` have a unique advantage beyond decorrelation and key-space dispersion.

## Next tests

1. Count collisions/probe distribution per local partition for each variant.
2. Repeat with Zipf/hot-key and adversarially clustered rho distributions.
3. Measure encoder-only nanoseconds/op separately from container lookup.
4. Compare against a control with two arbitrary decorrelated channels named `h2`/`h3`, to test whether `phi` and `theta` are functionally special or simply extra hash entropy.
5. Only promote an additional coordinate if the advantage survives those controls.
