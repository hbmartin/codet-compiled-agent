"""Compatibility entry point; historical reports and samples remain unchanged."""
import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from evidence_campaign import produce


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path, help="Resolved campaign config from evidence_campaign.py prepare")
    parser.add_argument("--output", type=Path, help="Standalone result or manifest-declared captured output")
    args = parser.parse_args()
    raise SystemExit(produce(args.config, 'measure', output=args.output))
