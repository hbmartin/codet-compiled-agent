"""Publish a new source-bound report; archived PR49 evidence is never rewritten."""

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from validation_evidence import publish


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path, help="Expected checks used by the capture runner")
    parser.add_argument("folder", type=Path, help="Captured command outcomes, not inferred log statuses")
    parser.add_argument("--benchmark", type=Path, help="Benchmark provenance including full production_hashes")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/validation/prompt-recovery-evidence-followup.json")
    args = parser.parse_args()
    archived = ROOT / "docs/validation/diagnostic-record-lifecycle-followup.json"
    if args.output.resolve() == archived:
        parser.error("Historical evidence must remain unchanged; select a new report")
    publish(args.manifest, args.folder, args.output,
            json.loads(args.benchmark.read_text()) if args.benchmark else None)


if __name__ == "__main__":
    main()
