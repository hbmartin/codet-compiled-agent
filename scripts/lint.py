"""Offline lint entry points; provisioning is an explicit, separate Make target."""

import argparse
import json
import os
import shutil
import subprocess

from modern_sources import ROOT, maintained_native_files


def binary(name):
    override = os.environ.get(name.upper().replace("-", "_"))
    if override:
        return override
    for directory in (ROOT / "build/lint-venv/bin", ROOT / "build/lint-tools/bin"):
        candidate = directory / name
        if candidate.is_file():
            return str(candidate)
    return shutil.which(name) or name


def commands(group):
    native = [str(p.relative_to(ROOT)) for p in maintained_native_files()]
    shells = [str(p.relative_to(ROOT)) for p in sorted((ROOT / "scripts").rglob("*.sh"))]
    workflows = [str(p.relative_to(ROOT)) for p in sorted((ROOT / ".github/workflows").iterdir()) if p.suffix in {".yml", ".yaml"}]
    return {
        "format": [("clang-format", "--version", "version 18.1.8", [
            ["--dry-run", "--Werror", "--style=file", *native]])],
        "python": [("ruff", "--version", "ruff 0.17.1", [
            ["check", "scripts", "tests"]])],
        "shell": [
            ("shellcheck", "--version", "ShellCheck", [shells]),
            ("shfmt", "--version", "v3.14.1", [["-d", "-i", "2", "-ci", *shells]]),
        ],
        "workflows": [
            ("actionlint", "-version", "1.7.12", [["-shellcheck", binary("shellcheck"), *workflows]]),
            ("zizmor", "--version", "zizmor 1.30.1", [
                ["--offline", "--no-progress", "--format", "plain", *workflows]]),
        ],
    }[group]


def run(group):
    folder = ROOT / "build/lint"
    folder.mkdir(parents=True, exist_ok=True)
    results = []
    for name, version_arg, expected, invocations in commands(group):
        tool = binary(name)
        result = dict(tool=name, passed=False, runs=[])
        try:
            version = subprocess.run([tool, version_arg], capture_output=True, text=True,
                                     check=True, timeout=15)
            result["version"] = version.stdout + version.stderr
            if expected not in result["version"]:
                raise ValueError(f"Expected {name} {expected}; run make fetch-lint-tools")
            if name == "actionlint":
                # actionlint silently omits its shell analysis if this is absent.
                subprocess.run([binary("shellcheck"), "--version"], capture_output=True,
                               check=True, timeout=15)
            for args in invocations:
                completed = subprocess.run([tool, *args], cwd=ROOT, text=True, capture_output=True,
                                           timeout=120, check=False)
                result["runs"].append(dict(command=[tool, *args], exit_code=completed.returncode,
                                           stdout=completed.stdout, stderr=completed.stderr))
                print(completed.stdout + completed.stderr, end="")
            result["passed"] = bool(result["runs"]) and all(
                r["exit_code"] == 0 for r in result["runs"])
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            result["failure"] = str(error)
            print(f"{name}: FAIL: {error}")
        results.append(result)
    passed = bool(results) and all(r["passed"] for r in results)
    (folder / (group + ".json")).write_text(json.dumps(
        dict(passed=passed, results=results), indent=2) + "\n")
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", choices=("all", "format", "python", "shell", "workflows"))
    args = parser.parse_args()
    groups = ("format", "python", "shell", "workflows") if args.group == "all" else (args.group,)
    # Evaluate every group even if an earlier check fails.
    return int(not all([run(group) for group in groups]))


if __name__ == "__main__":
    raise SystemExit(main())
