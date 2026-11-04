"""Compare complete analyzer runs without turning known findings into passes."""

import json
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
    if not manifest["versions"] or any(not (v.get("stdout") or v.get("stderr")) or v.get("failure") or v.get("exit_code") != 0 for v in manifest["versions"]):
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
    findings = [dict(f, _translation_unit=normalize_path(r.get("source"), root)) for r in results for f in r["findings"]]
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
                message.get("FileOffset"), message["Message"],
                finding.get("_translation_unit", normalize_path(message["FilePath"], root)))
    if tool == "cppcheck":
        locations = tuple(sorted((normalize_path(loc.get("file"), root) or "", loc.get("line") or "",
                                  loc.get("column") or "", loc.get("info") or "") for loc in finding["locations"]))
        return finding["id"], finding["msg"], locations
    def locations(value):
        if isinstance(value, dict):
            return {key: normalize_path(item, root) if key == "file" and isinstance(item, str) else locations(item)
                    for key, item in value.items()}
        if isinstance(value, list):
            return [locations(item) for item in value]
        return value
    return (finding["check_name"], finding["description"], finding.get("issue_context"),
            finding.get("issue_hash_content_of_line_in_context"), json.dumps(locations(finding.get("location")), sort_keys=True), finding.get("_translation_unit"))


def relocation_identity(tool, key):
    # Review can move positions, never a diagnostic's identity or source ownership.
    if tool == "clang-tidy":
        return key[0], key[1], key[3], key[4]
    if tool == "cppcheck":
        return key[0], key[1], tuple(sorted((loc[0], loc[3]) for loc in key[2]))
    location = json.loads(key[4])
    return key[0], key[1], key[2], key[3], location["file"], key[5]


def diagnostic_context(tool, key, root, expected=None):
    if tool == "clang-tidy":
        name, offset = key[1], key[2]
        data = (root / name).read_bytes()
        if not isinstance(offset, int) or not 0 <= offset < len(data):
            raise ValueError("Invalid relocation source offset")
        start = data.rfind(b"\n", 0, offset) + 1
        line = data[start:data.find(b"\n", offset) if b"\n" in data[offset:] else len(data)].decode()
        if expected is not None and expected in line:
            begin = start + len(line[:line.index(expected)].encode())
            if begin <= offset < begin + len(expected.encode()):
                return expected
        return line
    if tool == "cppcheck":
        name, line = key[2][0][:2]
    else:
        loc = json.loads(key[4])
        name, line = loc["file"], loc["line"]
    return (root / name).read_text().splitlines()[int(line) - 1]


def cppcheck_contexts_match(entry, before, after, baseline_root, changed_root):
    mappings = entry.get("location_contexts")
    if not isinstance(mappings, list) or len(mappings) != len(before[2]) or len(mappings) != len(after[2]):
        return False
    old, new = [], []
    for mapping in mappings:
        if not isinstance(mapping, dict) or set(mapping) != {"old", "new", "context"}:
            return False
        if not all(isinstance(mapping[name], (list, tuple)) for name in ("old", "new")):
            return False
        previous, current = tuple(mapping["old"]), tuple(mapping["new"])
        context = mapping["context"]
        if (len(previous) != 4 or len(current) != 4 or not isinstance(context, str) or not context or
                (previous[0], previous[3]) != (current[0], current[3])):
            return False
        for location, root in ((previous, baseline_root), (current, changed_root)):
            try:
                line = location[1]
                if not (type(line) is int or (type(line) is str and line.isdecimal())):
                    return False
                number = int(line)
                if number <= 0 or (root / location[0]).read_text().splitlines()[number - 1] != context:
                    return False
            except (OSError, IndexError, TypeError, ValueError):
                return False
        old.append(previous)
        new.append(current)
    return (len(set(old)) == len(old) and len(set(new)) == len(new) and
            Counter(old) == Counter(before[2]) and Counter(new) == Counter(after[2]))


def compare_analysis(baseline, changed, baseline_root, changed_root, expected, changed_expected=None, relocations=()):
    if baseline["tool"] != changed["tool"]:
        raise ValueError("Analyzer tool mismatch")
    tool = baseline["tool"]
    a = verify_analysis(baseline, baseline_root, expected)
    b = verify_analysis(changed, changed_root, changed_expected if changed_expected is not None else expected)
    if baseline["versions"] != changed["versions"]:
        # Tool paths may change between checkouts; compare actual version output.
        def versions(m):
            return [(v.get("stdout"), v.get("stderr"), v.get("exit_code")) for v in m["versions"]]
        if versions(baseline) != versions(changed):
            raise ValueError("Analyzer version mismatch")
    old = Counter(finding_key(tool, f, baseline_root) for f in a)
    new = Counter(finding_key(tool, f, changed_root) for f in b)
    used_old, used_new = set(), set()
    for entry in relocations:
        before = tuple(entry["old"])
        after = tuple(entry["new"])
        # JSON arrays nested in identities need their immutable tuple representation.
        def frozen(value):
            return tuple(frozen(x) for x in value) if isinstance(value, (list, tuple)) else value
        before, after = frozen(before), frozen(after)
        if tool == "cppcheck":
            context_matches = cppcheck_contexts_match(entry, before, after, baseline_root, changed_root)
        else:
            context_matches = bool(entry.get("context")) and (
                diagnostic_context(tool, before, baseline_root, entry["context"]) == entry["context"] and
                diagnostic_context(tool, after, changed_root, entry["context"]) == entry["context"])
        if (relocation_identity(tool, before) != relocation_identity(tool, after) or
                not entry.get("rationale") or not context_matches or
                before in used_old or after in used_new or old[before] != 1 or new[after] != 1):
            raise ValueError("Invalid or ambiguous reviewed analyzer relocation")
        used_old.add(before)
        used_new.add(after)
        old.subtract([before])
        new.subtract([after])
    equal = +old == +new
    return dict(tool=tool, equal=equal, baseline_findings=len(a), changed_findings=len(b),
                baseline_passed=baseline["passed"], changed_passed=changed["passed"],
                completed_sources=len(expected), reviewed_relocations=len(relocations),
                removed=[dict(identity=list(key), count=count) for key, count in (+old - +new).items()],
                added=[dict(identity=list(key), count=count) for key, count in (+new - +old).items()])
