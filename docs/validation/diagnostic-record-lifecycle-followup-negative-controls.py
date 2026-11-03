"""Replay native controls with explicit inputs and a new, nonhistorical output."""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from evidence_campaign import produce

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('config', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.output.exists():
    parser.error('Output already exists')
raise SystemExit(produce(args.config, 'native-negative-controls', output=args.output))
