"""Generate the checked CLI adapter and smoke-test the host preparation benchmark."""
import argparse
import json
import subprocess
import tempfile
import time
from pathlib import Path

from evidence_benchmarks import verify_preparation_metrics
from evidence_harness import cli_preparation_adapter
from process_supervision import run

WORKLOADS = ("allocation-positive", "saturated-count", "saturated-effective", "saturated-retained",
             "cli-preparation", "compatibility-preparation", "normalization")


def generate(source, output):
    _, adapter = cli_preparation_adapter(Path(source))
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text() != adapter:
        output.write_text(adapter)


def smoke(binary):
    deadline = time.monotonic() + 60
    with tempfile.TemporaryDirectory(prefix="codet-preparation-smoke-") as directory:
        for workload in WORKLOADS:
            output = Path(directory) / (workload + ".json")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise ValueError("Preparation smoke deadline expired")
            run([str(binary), workload, str(output)], check=True, timeout=min(30, remaining))
            row = json.loads(output.read_text())
            if row.get("case") != workload or row.get("preparation_metric_version") != 2:
                raise ValueError("Preparation smoke returned wrong workload or metric version")
            verify_preparation_metrics(dict(row, harness="preparation", workload=workload), variant="changed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    adapter = commands.add_parser("adapter")
    adapter.add_argument("--source", required=True, type=Path)
    adapter.add_argument("--output", required=True, type=Path)
    check = commands.add_parser("smoke")
    check.add_argument("binary", type=Path)
    options = parser.parse_args()
    try:
        if options.action == "adapter":
            generate(options.source, options.output)
        else:
            smoke(options.binary)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
