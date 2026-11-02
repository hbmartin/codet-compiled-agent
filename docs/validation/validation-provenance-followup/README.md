# Validation provenance follow-up

Baseline: `1842b76c783895253b1f1ade83c81e318c9e0339`, freshly fetched main after PR #50.
This follow-up changes developer tooling and tests, not production runtime behavior.

Version 2 captures bind each command to both production inputs and validation inputs.
Derived results carry their campaign, producing capture/check, and input hashes;
the capture records their finished output hashes. Complete analyzer execution is
validated separately from finding-related failure status. Benchmark medians, counts,
and work comparisons are derived from complete raw sample groups.

Publication stages a complete immutable bundle and verifies sources and sidecars
before exposing it. Previous PR #49/#50 reports, raw samples, and exact command
manifests remain historical and unchanged. Their executable compatibility entry
points delegate to maintained tools and require explicit output destinations.

Replay settings are resolved from `checks/validation-replay.json`; see the complete
prepare/capture/bundle commands in the repository README. No developer shell PATH
is copied into the replay template. Python 3.9 checks verify interpreter identity.
Process inspection preserves actual argument boundaries on Darwin and Linux, and
fails closed when eligibility cannot be established. No runtime dependency is added.

The two Sonar path alerts on the capture folder and output path do not identify an
application trust boundary: this local developer runner intentionally executes
manifest-provided command vectors and writes caller-selected report paths. It is
not a production agent tool or a sandbox for untrusted manifests. Confining report
paths to cwd would not make hostile command manifests safe. No alerts are dismissed
or quality gates weakened by this follow-up.

Fresh command outcomes, two sets of Release measurements, regression controls,
and host/platform limitations are recorded in the new run bundle linked below.
The previous LLVM 23 observations and Python 3.9 results are historical until
replaced by these newly captured outcomes; they are not assumed successful.

## Fresh acceptance results

Validated implementation: `6490ce5`. Subsequent report changes are documentation/data only.

- All **30 required captured checks pass**; the two analyzer observations retain exit 1.
- Debug and ASan/UBSan each pass **10/10 CTest groups**.
- **70 focused tests × 20 repetitions** pass in Debug and TSan. The one disabled
  native assertion probe remains intentionally invoked by its parent regression.
- Python **3.9.6** passes all **41 evidence tests** and **101 integration methods**.
- Pinned lint, Semgrep **1.177.0**, runner tests, and host cross-selection pass.
  The resolved manifest selects pinned `pysemgrep` through the existing `SEMGREP`
  override, avoiding an unrelated global `pysemgrep` 1.180.0 found on PATH.
- Baseline and changed analysis each complete **25/25 sources** per analyzer:
  150 Clang-Tidy findings, zero Cppcheck findings, and one known Clang-analyzer
  finding. Complete normalized comparison passes; nonzero findings remain observations.
- Six negative controls pass on unmodified code and fail through their intended
  assertions after mutation. Errors, missing tests, skips, and timeouts are rejected.
- [Required Ubuntu workflow](https://github.com/hbmartin/codet-compiled-agent/actions/runs/41342763373)
  passes all jobs, including designated Unicode coverage and strict static analysis.
- Historical preliminary captures remain under their original fingerprints and
  outcomes, including interrupted runs and the rejected Semgrep launcher mismatch.
  They satisfy no required acceptance check.

[Complete immutable bundle](6ab4afa4de724384bda8b264638f14cb/report.json), [inventory](6ab4afa4de724384bda8b264638f14cb/inventory.json), [raw samples](6ab4afa4de724384bda8b264638f14cb/samples.csv), [medians](6ab4afa4de724384bda8b264638f14cb/medians.csv).

The separately captured [Tiger availability observation](platform-2841a31c911f4cd59fa9ce2a6ad9ae3a/report.json)
retains exit 2: Docker, the configured PPC compiler, and the Tiger SDK are absent.
No native Tiger build or runtime success is claimed; host cross-selection is a
separate portability check.

## Fresh Release measurements

Both source variants match the recorded production files, use identical harness
sources and Release settings, and have recorded executable hashes. One warm-up
and five alternating measured samples per variant/workload were repeated in two
sets, with no concurrent local analyzers or tests. The verifier recomputes all
medians from 528 raw samples (440 measured) across 22 workloads.

Allocation counts, allocated bytes, output operations, and metadata-query work
match in every comparison. Unchanged prompt flushes allocate and write nothing.
Timing differences are observations; no optimization or performance benefit is
claimed. In particular, standalone diagnostics are +111.96% in set 1 and +0.18%
in set 2; both are retained rather than selecting the more favorable result.

| Workload | Set 1 latency change | Set 2 latency change |
|---|---:|---:|
| `allocation-positive` | -1.80% | +3.73% |
| `bounds-positive` | +3.32% | -9.86% |
| `busy` | -2.16% | -0.82% |
| `deferred-tail` | -0.34% | -3.00% |
| `deletion` | -0.83% | -2.95% |
| `diagnostics-amplified` | -4.05% | -0.20% |
| `diagnostics-pipe-amplified` | +1.54% | -27.48% |
| `diagnostics-standalone-amplified` | +111.96% | +0.18% |
| `editing` | -0.48% | +0.03% |
| `editing-basic` | -0.21% | +2.57% |
| `flags-positive` | +3.78% | +0.47% |
| `geometry-positive` | +0.18% | +0.02% |
| `large` | -1.36% | +2.64% |
| `native-boundary` | -4.13% | -0.32% |
| `native-busy` | +3.85% | -1.24% |
| `native-editing` | +0.15% | -0.38% |
| `native-flush-boundary` | +1.26% | +2.41% |
| `native-unchanged` | +0.82% | +4.01% |
| `small` | -0.81% | +0.40% |
| `stat-positive` | +1.44% | -2.67% |
| `unchanged` | +0.14% | -0.04% |
| `write-positive` | +3.53% | +0.33% |
