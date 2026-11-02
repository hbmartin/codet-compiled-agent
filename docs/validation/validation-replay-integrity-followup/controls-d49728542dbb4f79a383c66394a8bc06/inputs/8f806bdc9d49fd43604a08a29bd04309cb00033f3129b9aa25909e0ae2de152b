"""Inspect a source-bound measurement artifact; medians are recomputed from raw rows."""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from evidence_benchmarks import verify_samples


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=Path)
    parser.add_argument("config", type=Path)
    args = parser.parse_args()
    value = json.loads(args.artifact.read_text())["payload"]
    config = json.loads(args.config.read_text())
    print(json.dumps(verify_samples(value["rows"], value["summary"], config["cases"], sets=config["sets"], measured=config["measured"]), indent=2))
