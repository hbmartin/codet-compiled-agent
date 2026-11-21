"""Generate the checked CLI adapter and smoke-test the host preparation benchmark."""
import argparse
import json
import subprocess
import tempfile
import time
from pathlib import Path

from preparation_adapter import generate


def smoke(binary):
    from evidence_benchmarks import verify_preparation_metrics
    from preparation_metrics import METRIC_VERSION, WORKLOADS
    from process_supervision import run

    deadline = time.monotonic() + 60
    with tempfile.TemporaryDirectory(prefix="codet-preparation-smoke-") as directory:
        for workload in WORKLOADS:
            output = Path(directory) / (workload + ".json")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise ValueError("Preparation smoke deadline expired")
            run([str(binary), workload, str(output)], check=True, timeout=min(30, remaining))
            row = json.loads(output.read_text())
            if row.get("case") != workload or (type(row.get("preparation_metric_version")) is not int or row["preparation_metric_version"] != METRIC_VERSION):
                raise ValueError("Preparation smoke returned wrong workload or metric version")
            verify_preparation_metrics(dict(row, harness="preparation", workload=workload), variant="changed")


def unchanged(cmake, build):
    from process_supervision import run

    build = Path(build)
    def state():
        return {str(p.relative_to(build)): (p.stat().st_size, p.stat().st_mtime_ns)
                for p in build.rglob('*') if p.is_file() and
                (p.suffix in ('.o', '.a', '.hpp') or p.name == 'codet_preparation_bench')}
    before = state()
    if not before or not (build / 'codet_preparation_bench').is_file():
        raise ValueError('Unchanged-build check requires a built preparation benchmark')
    run([str(cmake), '--build', str(build), '--target', 'codet_preparation_bench', '--verbose'],
        check=True, timeout=60)
    if state() != before:
        raise ValueError('Unchanged preparation build performed compilation or linking')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    adapter = commands.add_parser("adapter")
    adapter.add_argument("--source", required=True, type=Path)
    adapter.add_argument("--output", required=True, type=Path)
    check = commands.add_parser("smoke")
    check.add_argument("binary", type=Path)
    stable = commands.add_parser("unchanged")
    stable.add_argument("--cmake", required=True, type=Path)
    stable.add_argument("build", type=Path)
    options = parser.parse_args()
    try:
        if options.action == "adapter":
            generate(options.source, options.output)
        elif options.action == "smoke":
            smoke(options.binary)
        else:
            unchanged(options.cmake, options.build)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
