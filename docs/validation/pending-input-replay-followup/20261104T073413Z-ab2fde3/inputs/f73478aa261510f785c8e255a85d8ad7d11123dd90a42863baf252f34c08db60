"""Verify raw benchmark samples, source identity, and negative controls."""

import math
import statistics
import subprocess
from pathlib import Path

from validation_evidence import digest, production_hashes, validation_hashes


def verify_git_source(repository, source, ref):
    resolved = subprocess.check_output(["git", "-C", str(repository), "rev-parse", ref + "^{commit}"],
                                       text=True, timeout=30).strip()
    from evidence_inputs import git_hashes
    expected = git_hashes(repository, resolved, "production")
    if not expected or expected != production_hashes(source):
        raise ValueError("Baseline source does not match recorded Git commit")
    return dict(ref=resolved, production_hashes=expected)


def verify_git_validation(repository, root, ref):
    from evidence_inputs import git_hashes
    expected = git_hashes(repository, ref, "validation")
    if not expected or expected != validation_hashes(root):
        raise ValueError("Validation inputs do not match recorded Git commit")
    return expected


def verify_samples(rows, summary, workloads, *, sets=2, measured=5):
    expected = {(s, harness, workload, variant) for s in range(1, sets + 1)
                for harness, names in workloads.items() for workload in names
                for variant in ("baseline", "changed")}
    if (type(sets) is not int or type(measured) is not int or sets < 1 or measured < 1 or
            len(expected) != sets * 2 * sum(len(names) for names in workloads.values())):
        raise ValueError("Invalid benchmark group registry")
    grouped = {key: [] for key in expected}
    seen = set()
    for row in rows:
        key = (row["set"], row["harness"], row["workload"], row["variant"])
        if type(row["set"]) is not int or key not in grouped:
            raise ValueError("Unexpected benchmark group")
        identity = key + (row["sample"],)
        if identity in seen or type(row["sample"]) is not int or row["sample"] not in range(measured + 1):
            raise ValueError("Duplicate or invalid benchmark sample")
        seen.add(identity)
        if type(row["warmup"]) is not bool or row["warmup"] != (row["sample"] == 0):
            raise ValueError("Invalid benchmark warmup")
        grouped[key].append(row)
    summaries = {}
    calculated = []
    for key, values in grouped.items():
        if len(values) != measured + 1:
            raise ValueError("Missing benchmark samples")
        retained = [r for r in values if not r["warmup"]]
        metrics = {k for k, v in retained[0].items() if type(v) in (int, float) and k not in ("set", "sample")}
        if not metrics or "milliseconds" not in metrics:
            raise ValueError("Missing benchmark metrics")
        for r in values:
            actual = {k for k, v in r.items() if type(v) in (int, float) and k not in ("set", "sample")}
            if actual != metrics or any(not math.isfinite(r[k]) or r[k] < 0 for k in metrics):
                raise ValueError("Invalid benchmark metrics")
        calculated.append(dict(set=key[0], harness=key[1], workload=key[2], variant=key[3],
                               **{k: statistics.median(r[k] for r in retained) for k in sorted(metrics)}))
    for row in summary:
        key = (row["set"], row["harness"], row["workload"], row["variant"])
        if key in summaries:
            raise ValueError("Duplicate benchmark summary")
        summaries[key] = row
    if set(summaries) != expected:
        raise ValueError("Missing or unexpected benchmark summaries")
    for row in calculated:
        key = (row["set"], row["harness"], row["workload"], row["variant"])
        if summaries[key] != row:
            raise ValueError("Raw samples do not match benchmark summary: " + repr(key))
    comparisons = []
    for s, harness, workload, variant in sorted(expected):
        if variant != "baseline":
            continue
        a, b = summaries[(s, harness, workload, "baseline")], summaries[(s, harness, workload, "changed")]
        differences = {k: [a[k], b[k]] for k in a if k not in ("variant", "milliseconds") and a[k] != b[k]}
        comparisons.append(dict(set=s, harness=harness, workload=workload, baseline_ms=a["milliseconds"],
                                changed_ms=b["milliseconds"],
                                percent=100 * (b["milliseconds"] / a["milliseconds"] - 1) if a["milliseconds"] else None,
                                work_differences=differences))
    return dict(samples=len(rows), measured_samples=sum(not r["warmup"] for r in rows),
                workloads=sum(len(names) for names in workloads.values()), comparisons=comparisons,
                all_measured_work_equal=not any(c["work_differences"] for c in comparisons))


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise ValueError("Mutation must match exactly once")
    return source.replace(before, after, 1)


def verify_unittest_result(baseline, mutant):
    """Structured results emitted by the child, not inferred from exit 1 or log text."""
    for result in (baseline, mutant):
        if (result["tests_run"] != 1 or result["errors"] or result["skipped"] or
                result.get("unexpected_successes") or result.get("expected_failures")):
            raise ValueError("Negative control did not execute exactly one assertion test")
    if baseline["exit"] != 0 or baseline["failures"]:
        raise ValueError("Negative-control baseline failed")
    if mutant["exit"] != 1 or mutant["failures"] < 1 or not mutant["assertion_failure"]:
        raise ValueError("Mutant did not fail through intended assertion")


def executable_provenance(path):
    if not Path(path).is_file():
        raise ValueError("Missing benchmark executable")
    return dict(path=str(path), sha256=digest(Path(path)))
