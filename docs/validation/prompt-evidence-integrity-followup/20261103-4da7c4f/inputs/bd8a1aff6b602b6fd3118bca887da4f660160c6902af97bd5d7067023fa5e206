"""Compare complete analyzer runs without turning known findings into passes."""

from collections import Counter
from pathlib import Path


def normalize_path(value, root):
    if not value:
        return value
    path = Path(value)
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def verify_analysis(manifest, root, expected):
    required = [normalize_path(s, root) for s in expected]
    supplied = [normalize_path(s, root) for s in manifest["expected_sources"]]
    completed = [normalize_path(s, root) for s in manifest["completed_sources"]]
    if not required or len(set(required)) != len(required):
        raise ValueError("Invalid expected production source registry")
    if (len(set(supplied)) != len(supplied) or len(set(completed)) != len(completed) or
            set(supplied) != set(required) or set(completed) != set(required)):
        raise ValueError("Incomplete or duplicate analyzer source coverage")
    if (manifest["tool"] == "cppcheck" and not manifest["canaries"]) or not all(c["passed"] for c in manifest["canaries"]):
        raise ValueError("Analyzer canary did not pass")
    if any(v.get("failure") or v.get("exit_code") != 0 for v in manifest["versions"]):
        raise ValueError("Analyzer version inspection failed")
    results = manifest["results"]
    actual = []
    for result in results:
        if result.get("failure") or result.get("status") not in ("passed", "findings"):
            raise ValueError("Analyzer execution or parser failure")
        if manifest["tool"] == "cppcheck":
            actual.extend(normalize_path(s, root) for s in result["completed_sources"])
        else:
            actual.append(normalize_path(result["source"], root))
    if Counter(actual) != Counter(required):
        raise ValueError("Analyzer result coverage mismatch")
    findings = [f for r in results for f in r["findings"]]
    if any(bool(r["findings"]) != (r["status"] == "findings") for r in results):
        raise ValueError("Analyzer result status mismatch")
    if manifest["passed"] != all(r["status"] == "passed" for r in results):
        raise ValueError("Analyzer passed status mismatch")
    if len(findings) != manifest["findings"]:
        raise ValueError("Analyzer finding count mismatch")
    return findings


def finding_key(tool, finding, root):
    if tool == "clang-tidy":
        message = finding["DiagnosticMessage"]
        return (finding["DiagnosticName"], normalize_path(message["FilePath"], root),
                message.get("FileOffset"), message["Message"])
    if tool == "cppcheck":
        locations = tuple(sorted((normalize_path(loc.get("file"), root) or "", loc.get("line") or "",
                                  loc.get("column") or "", loc.get("info") or "") for loc in finding["locations"]))
        return finding["id"], finding["msg"], locations
    return (finding["check_name"], finding["description"], finding.get("issue_context"),
            finding.get("issue_hash_content_of_line_in_context"))


def compare_analysis(baseline, changed, baseline_root, changed_root, expected):
    if baseline["tool"] != changed["tool"]:
        raise ValueError("Analyzer tool mismatch")
    tool = baseline["tool"]
    a = verify_analysis(baseline, baseline_root, expected)
    b = verify_analysis(changed, changed_root, expected)
    if baseline["versions"] != changed["versions"]:
        # Tool paths may change between checkouts; compare actual version output.
        def versions(m):
            return [(v.get("stdout"), v.get("stderr"), v.get("exit_code")) for v in m["versions"]]
        if versions(baseline) != versions(changed):
            raise ValueError("Analyzer version mismatch")
    equal = Counter(finding_key(tool, f, baseline_root) for f in a) == Counter(
        finding_key(tool, f, changed_root) for f in b)
    return dict(tool=tool, equal=equal, baseline_findings=len(a), changed_findings=len(b),
                baseline_passed=baseline["passed"], changed_passed=changed["passed"],
                completed_sources=len(expected))
