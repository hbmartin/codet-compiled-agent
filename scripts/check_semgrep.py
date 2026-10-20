"""Run pinned local policies and reject findings or incomplete success reports."""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

from modern_sources import ROOT, policy_inputs

VERSION = "1.177.0"


def rule_ids(root):
    # Keep rule IDs in the checked-in single-line `- id:` convention. Semgrep
    # validates YAML; comparison with its test results catches unsupported forms.
    files = sorted((root / "checks/semgrep").glob("*.yaml"))
    ids = [
        name
        for path in files
        for name in re.findall(r"^\s+- id: ([\w.-]+)\s*$", path.read_text(), re.MULTILINE)
    ]
    if not files or not ids or len(ids) != len(set(ids)):
        raise ValueError("Missing or duplicate local rule IDs")
    return set(ids)


def relative_path(value, root):
    path = Path(value)
    return (root / path).resolve().relative_to(root.resolve()).as_posix()


def validate_scan(data, expected, root):
    if not isinstance(data, dict) or data.get("version") != VERSION:
        raise ValueError("Missing or incompatible scan version")
    for field in ("results", "errors", "skipped_rules"):
        if not isinstance(data.get(field), list) or data[field]:
            raise ValueError(f"Nonempty or missing {field}")
    paths = data.get("paths", {})
    scanned = paths.get("scanned")
    if not isinstance(scanned, list):
        raise ValueError("Missing scanned-file inventory")
    actual = [relative_path(p, root) for p in scanned]
    if not expected or len(actual) != len(set(actual)) or set(actual) != set(expected):
        raise ValueError(f"Incomplete scan: expected {sorted(expected)}, scanned {sorted(actual)}")
    if paths.get("skipped"):
        raise ValueError("Unexpected skipped files")


def validate_tests(data, expected_ids):
    if not isinstance(data, dict):
        raise ValueError("Invalid fixture report")
    for field in ("config_missing_tests", "config_missing_fixtests", "config_with_errors"):
        if not isinstance(data.get(field), list) or data[field]:
            raise ValueError(f"Nonempty or missing {field}")
    results = data.get("results")
    if not isinstance(results, dict) or not results:
        raise ValueError("Missing fixture results")
    seen = set()
    for result in results.values():
        checks = result.get("checks")
        if not isinstance(checks, dict) or not checks:
            raise ValueError("Missing rule tests")
        for name, check in checks.items():
            if name in seen or check.get("passed") is not True or check.get("errors") != []:
                raise ValueError("Failed, duplicate or incomplete rule test: " + name)
            matches = check.get("matches")
            if not isinstance(matches, dict) or not matches:
                raise ValueError("Missing fixture matches: " + name)
            expected_count = 0
            for match in matches.values():
                lines = match.get("expected_lines")
                if not isinstance(lines, list) or lines != match.get("reported_lines"):
                    raise ValueError("Fixture detection mismatch: " + name)
                expected_count += len(lines)
            if not expected_count:
                raise ValueError("Rule has no positive fixture: " + name)
            seen.add(name)
    if seen != expected_ids:
        raise ValueError("Fixture rule inventory differs from checked-in policy")


def snapshot(root, sources):
    paths = set(sources) | set((root / "checks/semgrep").glob("*"))
    return {
        p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(paths)
        if p.is_file()
    }


def run(mode, binary, root=ROOT):
    folder = root / "build/semgrep"
    folder.mkdir(parents=True, exist_ok=True)
    filename = "scan.json" if mode == "scan" else "tests.json"
    report = folder / filename
    report.unlink(missing_ok=True)
    manifest = folder / (mode + ".manifest.json")
    manifest.write_text(json.dumps(dict(passed=False, failure="Scan has not completed")) + "\n")
    env = dict(os.environ, SEMGREP_ENABLE_VERSION_CHECK="0", SEMGREP_SEND_METRICS="off")
    inputs = policy_inputs(root)
    before = snapshot(root, inputs)
    ids = rule_ids(root)
    version = subprocess.run(
        [binary, "--version"],
        cwd=root,
        env=env,
        check=True,
        text=True,
        capture_output=True,
        timeout=15,
    )
    if version.stdout.strip() != VERSION:
        raise ValueError(f"Semgrep {VERSION} is required")
    command = [
        binary,
        "scan",
        "--metrics=off",
        "--disable-version-check",
        "--strict",
        "--no-rewrite-rule-ids",
        "--json",
    ]
    if mode == "scan":
        command += ["--error", "--config", "checks/semgrep"]
        command += [p.relative_to(root).as_posix() for p in inputs]
    else:
        command += ["--test", "checks/semgrep"]
    # The CI job also has a wall-clock limit. Always preserve both streams and
    # the original failure status; successful output is independently validated.
    with report.open("w") as stdout, (folder / (mode + ".stderr.log")).open("w") as stderr:
        result = subprocess.run(
            command, cwd=root, env=env, stdout=stdout, stderr=stderr, timeout=600, check=False
        )
    if result.returncode:
        print(f"Semgrep check failed; report: {report}", file=sys.stderr)
        print(report.read_text(), file=sys.stderr)
        print((folder / (mode + ".stderr.log")).read_text(), file=sys.stderr)
        return result.returncode
    data = json.loads(report.read_text())
    if mode == "scan":
        validate_scan(data, [p.relative_to(root).as_posix() for p in inputs], root)
    else:
        validate_tests(data, ids)
    after = snapshot(root, policy_inputs(root))
    if before != after:
        raise ValueError("Policy or production inputs changed during scan")
    manifest.write_text(
        json.dumps(
            dict(
                version=VERSION, command=command, inputs=before, rule_ids=sorted(ids), passed=True
            ),
            indent=2,
        )
        + "\n"
    )
    print(f"Semgrep {mode}: PASS; {len(inputs)} production inputs; {len(ids)} rules")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("scan", "test"))
    parser.add_argument("--semgrep", default=str(ROOT / "build/semgrep-venv/bin/semgrep"))
    args = parser.parse_args()
    try:
        return run(args.mode, args.semgrep)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"Semgrep check failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
