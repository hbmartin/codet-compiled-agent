# Prompt cleanup and validation evidence

Baseline: `ef2155c7be251ae37d13ead8c5d0a34f37dbc3e2` (fetched main after PR49).
Validated production source: `6479ac6`; subsequent evidence changes are documentation only.

The JSON report is assembled only after the source-bound publisher accepts captured
command outcomes. Required checks have actual zero statuses. Local LLVM23 findings
remain nonzero observations, with a separately successful normalized comparison:
150 Clang-Tidy findings, zero Cppcheck findings, and one existing `core.DivideZero`.
The initial sandbox-restricted localhost-server failure and interrupted capture are
retained separately. No success was reconstructed from their log text.

Debug and ASan/UBSan each pass 10 CTest groups. Debug contains 644 unit cases and 68
publication-fault cases; the existing two opt-in skips remain. The disabled assertion
probe is intentionally run in a child process by its parent regression. All 101
integration methods also pass under Python3.9. Twenty repetitions of 70 focused cases
pass in Debug and TSan. The required Ubuntu workflow succeeds at
https://github.com/hbmartin/codet-compiled-agent/actions/runs/41323114292.
Host cross-selection passes; a native PPC/Tiger toolchain and runtime are unavailable.

Six negative controls fail their intended tests: deferred cleanup, observer separation,
fixture capture leakage, constant check success, constant source equivalence, and
process-command substring matching. Native controls put the mutated object first in
the link to ensure its inline definitions are tested rather than an existing weak copy.

Release measurements use seven identical harness/support sources, identical compiler
flags, one warm-up plus five alternating measured samples, repeated twice. There are
528 samples across 22 workloads; raw samples and all medians are included. Positive
controls validate allocation and syscall counters. Allocation counts, allocated bytes,
and measured work match; unchanged prompt flushes allocate and write nothing.

Timing results are mixed. Small-export medians increased 42–49% across the two sets;
large exports increased roughly 2% and 9%. These are reported observations, not claims
of causation or improved performance. No optimization is retained or revived, and no
timing-based CI threshold is added.

To repeat validation, configure the desired builds, adjust the captured manifest's
root/tool paths for the host, then run:

```sh
python3 scripts/validation_evidence.py capture MANIFEST CAPTURE_DIRECTORY
python3 scripts/validation_evidence.py publish MANIFEST CAPTURE_DIRECTORY --output NEW_REPORT
```

A new capture invalidates records from older captures. Required results must match the
manifest, source fingerprints, and captured logs. Build/test prerequisites are separate;
a failed build blocks test execution. Analyzer observations retain their actual exits.
A benchmark argument must contain recorded full `production_hashes`; mismatches are
explicit and do not attribute old measurements to current sources. Historical reports
and samples remain unchanged. The compatibility publisher refuses its archived output.

For benchmarks, build the recorded baseline and changed Release variants using the
configuration arguments in the report (adjusting checkout paths). Run the published
`-measure.py` script from the repository root after validation/analyzers finish. It
checks active process identities and harness hashes before measuring. Published native
negative-control scripts require the corresponding Debug build and compile database.

## Current compatibility entry points

The measurements and commands above describe the archived run. Current standalone
producers require an explicit resolved campaign config and `--output`; see the
[maintained replay examples](../../README.md). Native and Python controls have
separate entry points. Standalone results are unbound and cannot replace captured
check evidence. The original historical JSON, CSVs, and exact command manifest
remain unchanged.
