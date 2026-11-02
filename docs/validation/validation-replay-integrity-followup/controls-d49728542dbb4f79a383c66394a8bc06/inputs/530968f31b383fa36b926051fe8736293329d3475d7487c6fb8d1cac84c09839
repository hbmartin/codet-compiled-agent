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
    parser.add_argument("--output", type=Path, required=True, help="Standalone unbound result; cannot satisfy capture validation")
    args = parser.parse_args()
    raise SystemExit(produce(args.config, 'native-negative-controls', output=args.output))
