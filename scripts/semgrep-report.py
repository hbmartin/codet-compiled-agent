#!/usr/bin/env python3
"""Record the provisioned environment or display a failed local Semgrep report."""
import importlib.metadata
import json
import platform
import sys
from pathlib import Path


def main():
    if sys.argv[1:] == ["environment"]:
        print(json.dumps({"python": sys.version, "executable": sys.executable,
                          "platform": platform.platform(),
                          "semgrep": importlib.metadata.version("semgrep")}, indent=2))
        return
    if len(sys.argv) != 3 or sys.argv[1] != "failure":
        raise SystemExit("usage: semgrep-report.py environment | failure REPORT")
    report = Path(sys.argv[2])
    print(f"Semgrep check failed; report: {report}", file=sys.stderr)
    try:
        data = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        print(f"Report is missing or invalid: {error}", file=sys.stderr)
        return
    # Test-result and production schemas differ. Print the complete structured result so
    # parser failures, unmatched fixture annotations and scan errors stay visible too.
    print(json.dumps(data, indent=2, ensure_ascii=False), file=sys.stderr)


if __name__ == "__main__":
    main()
