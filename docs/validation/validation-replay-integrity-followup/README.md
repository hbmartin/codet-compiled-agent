# Validation replay integrity follow-up

Fetched baseline: `7b8620f09dc4b42813f63e3048188984dfb033ca` (main after PR #51).
Validated implementation: `8b6a05a`. Subsequent changes add documentation/evidence only.
No production runtime sources, storage formats, or application CLI interfaces change.

## Results

- All **33 required checks** pass in the [35-check bundle](d4c2749fcb2d4777acc237b00700705e/report.json).
  The two analyzer observations retain their actual exit 1.
- Debug and ASan/UBSan each pass **10/10 CTest groups**.
- **70 focused tests × 20 repetitions** pass in both Debug and TSan; the existing
  disabled assertion probe remains intentionally exercised by its parent test.
- Python **3.9.6** passes **64 evidence tests** and **101 integration methods**.
  Captured-context isolation, child readiness, all four standalone/captured wrappers,
  Git-bound publication, and same-stem measurement artifacts are exercised.
- Pinned lint, Semgrep **1.177.0**, runner checks, and fresh host cross-selection pass.
  Cross-selection has an explicit configure dependency, a recorded host-only link
  placeholder, bound configuration, and an executable digest. It is not a Tiger build.
- **13 Python and three native negative controls** pass on unmodified code and fail
  through their intended assertions after mutation. Import, launch, compilation,
  missing-test, timeout, and unrelated errors cannot count as successful controls.
- Baseline and changed analyzers each complete **25/25 sources**: 150 known
  Clang-Tidy findings, zero Cppcheck findings, and one known Clang-analyzer finding.
  The normalized comparison passes; finding-related failures are not relabeled passes.
- [Required Ubuntu workflow](https://github.com/hbmartin/codet-compiled-agent/actions/runs/41393637254)
  succeeds at the frozen implementation, including real Unicode coverage and strict analysis.

The [current platform observation](platform-cc983b03c73544829278b3ba099024ca/report.json)
retains exit 2: Docker, the configured PPC compiler, and the Tiger SDK are absent.
Native Tiger compilation/runtime verification is unavailable locally.

## Build provenance and offline audit

Version 3 preserves the meanings of production and validation fingerprints and adds
actual build-source, generated-input, external installation, and output records.
Baseline source is verified against its Git commit before and after configure/build.
Consumers verify producing build outputs before launch. All build directories are
fresh and campaign-local; compiler identities come from the actual CMake caches.
Tools resolve through an explicit generated PATH, with recorded paths, hashes, and
supported version probes. Interpreter invocation paths preserve virtualenv behavior.

The [immutable inventory](d4c2749fcb2d4777acc237b00700705e/inventory.json) and
[input index](d4c2749fcb2d4777acc237b00700705e/inputs.json) bind **9,882 audit-input
references**, deduplicating their stored material. Only current expected records and
logs are staged. Complete source/configuration inputs, build metadata, and raw report
inputs are archived by digest. External SDKs, tools, dependency installations, and
executables retain identities/references; their installations are not bundled.

The [captured relocation check](verification-bd95f5c45b1a4efabc5a9eda479f8ec2/report.json)
passes after copying the bundle to a temporary location and rejecting file opens
outside that copy. It verifies complete inventory, command outcomes, producer links,
archived inputs, raw-sample medians, and CSV contents. It does not rebuild or rerun
recorded binaries, or attest to execution independently of the captured local tools.

```sh
python3 scripts/validation_evidence.py verify-bundle \
  docs/validation/validation-replay-integrity-followup/d4c2749fcb2d4777acc237b00700705e
```

[Replay manifest](d4c2749fcb2d4777acc237b00700705e/manifest.json),
[capture identity](d4c2749fcb2d4777acc237b00700705e/capture/run.json), and
[maintained prepare/capture/standalone examples](../../../README.md) preserve concrete
commands separately from portable template defaults. Captured wrappers use only
manifest-declared artifact paths; standalone results are explicitly unbound.

## Fresh Release measurements

Both variants have verified source, executable, harness, compiler, and configuration
identities. One warm-up plus five alternating measured runs per variant/workload is
repeated twice, with no registered build/test/analyzer blockers at measurement gates.
Positive controls verify counters. All **528 samples (440 measured)** across **22
workloads** and their recomputed summaries are retained.

[Raw samples](d4c2749fcb2d4777acc237b00700705e/measurements/benchmarks/measure.json/samples.csv),
[medians](d4c2749fcb2d4777acc237b00700705e/measurements/benchmarks/measure.json/medians.csv), and
[producer envelope](d4c2749fcb2d4777acc237b00700705e/artifacts/benchmarks/measure.json).

Allocation counts, allocated bytes, output operations, and metadata-query work match
in every measured comparison. Timing is noisy and mixed; no optimization, causal
speed claim, or timing-based threshold is introduced. Every workload is shown below.

| Harness / workload | Set 1 latency change | Set 2 latency change |
|---|---:|---:|
| export / large | +2.84% | +0.22% |
| export / small | -1.89% | +2.03% |
| line / allocation-positive | +3.83% | +1.94% |
| line / busy | -0.06% | -2.86% |
| line / deferred-tail | -1.53% | +0.81% |
| line / deletion | +0.69% | +6.61% |
| line / editing | +1.70% | +0.24% |
| line / editing-basic | -0.13% | -3.02% |
| line / geometry-positive | -0.96% | -0.04% |
| line / native-boundary | +0.44% | -3.19% |
| line / native-busy | +0.40% | +25.39% |
| line / native-editing | -0.43% | -0.30% |
| line / native-flush-boundary | -3.79% | +2.28% |
| line / native-unchanged | -0.29% | -6.30% |
| line / unchanged | -0.15% | -1.61% |
| output / bounds-positive | -1.61% | +9.65% |
| output / diagnostics-amplified | +0.86% | -0.42% |
| output / diagnostics-pipe-amplified | -1.87% | -0.74% |
| output / diagnostics-standalone-amplified | -0.77% | -0.26% |
| output / flags-positive | -0.72% | -0.48% |
| output / stat-positive | -1.12% | -0.17% |
| output / write-positive | +3.09% | -3.79% |

## Historical attempts and trust boundary

Four preliminary attempts retain their original manifests, records, logs, input
fingerprints, and available artifacts through the current bundle's history input
index. They include the inherited-context test failure, interrupted runs, the
Git-source-key publication failure, and the compatibility/namespace corrections.
Their outcomes are not reconstructed from log tails. Those historical check
outcomes satisfy no current required acceptance check. The final controls explicitly reproduce the corrected source-key, captured
identity, classification, and sidecar defects.

The earlier local [platform probe](platform-2596ba033666452490298fa48b4515cd/report.json)
and [classification control](controls-d49728542dbb4f79a383c66394a8bc06/report.json)
remain immutable historical supplements with their original validation fingerprints;
they are not substituted for current acceptance. PR #49–51 historical JSON, CSV, and
exact command manifests remain unchanged.

The two Sonar alerts concern trusted local developer command/path capabilities.
This tooling intentionally executes explicit command vectors and writes caller-selected
report paths; it is not a production model tool or sandbox for untrusted manifests.
Alerts, review threads, suppression rules, and quality gates are unchanged.
