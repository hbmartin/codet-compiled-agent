"""Offline lint entry points; provisioning is an explicit, separate Make target."""

import argparse
import json
import os
import re
import shutil
import subprocess
import tempfile

from modern_sources import ROOT, maintained_native_files

GROUPS = ("format", "python", "shell", "workflows")
VERSION_PATTERNS = {
    "clang-format": r"(?:Ubuntu |Debian |Apple )?clang-format version (\d+\.\d+\.\d+)(?: \([^\n]*\))?",
    "ruff": r"ruff (\d+\.\d+\.\d+)",
    "shfmt": r"v(\d+\.\d+\.\d+)",
    "actionlint": r"v?(\d+\.\d+\.\d+)",
    "zizmor": r"zizmor (\d+\.\d+\.\d+)",
    "shellcheck": r"version: (\d+\.\d+\.\d+)",
}


def tool_version(name, output):
    pattern = VERSION_PATTERNS.get(name)
    if pattern is None:
        raise ValueError("Unknown lint tool: " + name)
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    if name == "shellcheck":
        if not lines or lines[0] != "ShellCheck - shell script analysis tool":
            raise ValueError("Unrecognized ShellCheck banner")
        matches = [match for line in lines
                   if (match := re.fullmatch(pattern, line))]
    else:
        match = re.fullmatch(pattern, lines[0]) if lines else None
        # actionlint appends build provenance. Other tools have one version line.
        matches = [match] if match and (name == "actionlint" or len(lines) == 1) else []
        if name == "actionlint" and any(re.match(r"v?\d+\.", line) for line in lines[1:]):
            matches = []
    if len(matches) != 1:
        raise ValueError("Missing, ambiguous or malformed " + name + " version")
    return matches[0].group(1)


def write_report(group, data):
    folder = ROOT / "build/lint"
    folder.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=folder, delete=False) as stream:
            temporary = stream.name
            stream.write(json.dumps(data, indent=2) + "\n")
        os.replace(temporary, folder / (group + ".json"))
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)


def invalidate(groups):
    for group in groups:
        write_report(group, dict(passed=False, failure="Check has not completed", results=[]))


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
        "format": [("clang-format", "--version", "18.1.8", [
            ["--dry-run", "--Werror", "--style=file", *native]])],
        "python": [("ruff", "--version", "0.17.1", [
            ["check", "scripts", "tests"]])],
        "shell": [
            ("shellcheck", "--version", None, [shells]),
            ("shfmt", "--version", "3.14.1", [["-d", "-i", "2", "-ci", *shells]]),
        ],
        "workflows": [
            ("actionlint", "-version", "1.7.12", [["-shellcheck", binary("shellcheck"), *workflows]]),
            ("zizmor", "--version", "1.30.1", [
                ["--offline", "--no-progress", "--format", "plain", *workflows]]),
        ],
    }[group]


def _run(group):
    try:
        requested = commands(group)
    except (OSError, ValueError) as error:
        write_report(group, dict(passed=False, failure=str(error), results=[]))
        return False
    results = []
    for name, version_arg, expected, invocations in requested:
        tool = binary(name)
        result = dict(tool=name, passed=False, runs=[])
        try:
            version = subprocess.run([tool, version_arg], capture_output=True, text=True,
                                     check=True, timeout=15)
            result["version"] = version.stdout + version.stderr
            result["parsed_version"] = tool_version(name, result["version"])
            if expected is not None and expected != result["parsed_version"]:
                raise ValueError(f"Expected {name} {expected}; run make fetch-lint-tools")
            if name == "actionlint":
                # actionlint silently omits its shell analysis if this is absent.
                shellcheck = subprocess.run([binary("shellcheck"), "--version"], capture_output=True,
                                           text=True, check=True, timeout=15)
                result["shellcheck_version"] = tool_version(
                    "shellcheck", shellcheck.stdout + shellcheck.stderr)
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
    write_report(group, dict(passed=passed, results=results))
    return passed


def run(group):
    invalidate((group,))
    return _run(group)


def run_groups(groups):
    # Invalidate later groups before the first inventory or tool can interrupt us.
    invalidate(groups)
    return all([_run(group) for group in groups])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("group", choices=("all", "format", "python", "shell", "workflows"))
    args = parser.parse_args()
    groups = GROUPS if args.group == "all" else (args.group,)
    # Evaluate every group even if an earlier check fails.
    return int(not run_groups(groups))


if __name__ == "__main__":
    raise SystemExit(main())
