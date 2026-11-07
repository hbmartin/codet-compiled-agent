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


def verify_tooling_metrics(row):
    if row.get('harness') != 'tooling' or 'metric_version' not in row:
        return # Historical unversioned observations keep their original field interpretation.
    if row['metric_version'] != 2:
        raise ValueError('Unsupported tooling metric version')
    fields = ('files', 'owned_processes', 'check_output_calls', 'git_commands', 'process_inspection_commands',
              'retained_traced_blocks', 'retained_traced_bytes', 'peak_traced_bytes')
    if (any(type(row.get(name)) is not int or row[name] < 0 for name in fields) or
            {'retained_allocations', 'retained_bytes', 'allocations', 'metadata_queries'} & set(row)):
        raise ValueError('Missing or ambiguous tooling counters')
    cmake = row.get('cmake_commands', 0)
    if type(cmake) is not int or cmake < 0:
        raise ValueError('Invalid configuration command counter')
    if row.get('workload') == 'configuration-selection':
        if row.get('configuration_metric_version') != 1 or cmake != 3 or row.get('configuration_checks') != 3 or row['files'] or row['owned_processes']:
            raise ValueError('Configuration selection counters or controls are invalid')
    elif cmake:
        raise ValueError('Non-configuration workload claims CMake commands')
    if row['check_output_calls'] != row['git_commands'] + row['process_inspection_commands'] + cmake:
        raise ValueError('Tooling command counter classification mismatch')
    if row['retained_traced_bytes'] > row['peak_traced_bytes']:
        raise ValueError('Tooling retained memory exceeds peak')
    if row.get('workload') == 'supervision' and (row['git_commands'] or row['files'] or row['owned_processes'] != 5):
        raise ValueError('Supervision must record five owned processes, no files or Git commands')
    if row.get('workload') in ('hashing', 'git-inspection') and row['owned_processes']:
        raise ValueError('Hash/Git workloads cannot claim owned processes')
    if row.get('workload') == 'build-context':
        if (row.get('context_metric_version') != 1 or row['owned_processes'] or
                row.get('context_resolutions') not in (1, 3) or row.get('sysroot_snapshots') not in (2, 3) or
                row.get('path_stat_positive') != 1 or
                any(type(row.get(key)) is not int or row[key] < 0 for key in ('direct_file_hash_calls','path_stat_calls'))):
            raise ValueError('Build-context scoped counters or controls are invalid')


def verify_preparation_metrics(row, *, variant=None):
    if row.get("harness") != "preparation":
        return
    fields = ("iterations", "allocations", "allocated_bytes", "payload_sized_allocations", "rejections",
              "prepared_submissions", "distinct_command_wrappers", "effective_content_bytes")
    if row.get("preparation_metric_version") not in (1, 2) or any(type(row.get(k)) is not int or row[k] < 0 for k in fields) or row["iterations"] != 128:
        raise ValueError("Invalid preparation measurement counters")
    workload = row.get("workload")
    workloads = ("allocation-positive", "saturated-count", "saturated-effective", "saturated-retained",
                 "cli-preparation", "compatibility-preparation", "normalization")
    if not isinstance(workload, str) or workload not in workloads:
        raise ValueError("Invalid preparation workload: " + repr(workload))
    if row["preparation_metric_version"] == 2:
        role = row.get("variant") if variant is None else variant
        if role not in ("baseline", "changed") or ("variant" in row and row["variant"] != role):
            raise ValueError("Invalid preparation variant")
        if role == "changed" and workload.startswith("saturated-") and row["payload_sized_allocations"]:
            raise ValueError("Changed saturated preparation allocated payload-sized backing")
        if role == "changed" and workload == "cli-preparation" and row["distinct_command_wrappers"] != row["prepared_submissions"]:
            raise ValueError("Changed CLI preparation duplicated command wrappers")
    if workload == "allocation-positive" and (row["allocations"] != 128 or row["payload_sized_allocations"] != 128 or row["allocated_bytes"] != 8388608):
        raise ValueError("Preparation allocation positive control failed")
    if workload.startswith("saturated-") and row["rejections"] != 128:
        raise ValueError("Saturated preparation failed to reject every input")
    if workload in ("cli-preparation", "compatibility-preparation") and row["prepared_submissions"] != 128:
        raise ValueError("Preparation acceptance control failed")
    if workload == "cli-preparation" and (row["distinct_command_wrappers"] not in (128, 256) or row["effective_content_bytes"] != 1664):
        raise ValueError("CLI preparation wrapper/content control failed")
    if workload in ("normalization", "compatibility-preparation") and row["effective_content_bytes"] != 25165824:
        raise ValueError("Normalization content control failed")


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
        verify_tooling_metrics(row)
        verify_preparation_metrics(row)
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
