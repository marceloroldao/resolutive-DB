from __future__ import annotations

import argparse
import json
from pathlib import Path
import tempfile

from v122_frozen_memoria_topological import MEMORIA_VALIDATED_COMMIT, run_size


SIZES = (5000, 10000)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="bdr-v127-") as temp_dir:
        root = Path(temp_dir)
        samples = [run_size(size, args.library, root) for size in SIZES]

    report = {
        "benchmark": "v127_frozen_memoria_extended_scale",
        "memoria_validated_commit": MEMORIA_VALIDATED_COMMIT,
        "sizes": list(SIZES),
        "contract": {
            "fixture_reuses_v122_exact_frozen_shape": True,
            "codec_imported_from_memoria": True,
            "one_snapshot_one_atomic_batch": True,
            "sqlite_is_oracle_only": True,
            "performance_thresholds": False,
            "semantic_parity_required": True,
            "negative_results_must_be_recorded": True,
            "bulk_prefetch_is_benchmark_adapter_not_bdr_semantics": True,
        },
        "samples": samples,
    }
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
