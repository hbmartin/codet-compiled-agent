# Stable configuration and bounded submission ownership

The ready follow-up is based on freshly fetched `e6309e3595571ab3698eae0ba4166c51e9c8636b` and validates frozen implementation `7bde14d666c0822328fe26c75aabbb78a7039f8f`. The original checkout was preserved. No historical report, measurement, or review thread was changed.

## Current evidence

- [Immutable v5 audit bundle](41f02d408f76444da6a7870352af5119/report.json), campaign `c628185e1bc746fe99756cb068202a58`, capture `41f02d408f76444da6a7870352af5119`.
- [Concrete command manifest](41f02d408f76444da6a7870352af5119/manifest.json), [capture records and logs](41f02d408f76444da6a7870352af5119/capture), [archived input index](41f02d408f76444da6a7870352af5119/inputs.json), and [inventory](41f02d408f76444da6a7870352af5119/inventory.json).
- [Independent relocated offline audit](41f02d408f76444da6a7870352af5119-offline-audit.json): Python 3.9.6, original-checkout file access and subprocess launch blocked; all 40 checks and 12,982 audit-input declarations verified. Corruption of an archived input was rejected. This audits captured evidence; it does not rerun binaries.
- [Benchmark artifact](41f02d408f76444da6a7870352af5119/artifacts/benchmarks/measure.json), [raw samples](41f02d408f76444da6a7870352af5119/measurements/benchmarks/measure.json/samples.csv), and [recomputed medians](41f02d408f76444da6a7870352af5119/measurements/benchmarks/measure.json/medians.csv).

All 38 required checks passed. Two analyzer checks retain their actual nonzero observation outcomes; their strict reviewed-difference check passed. The bundle archives production and validation inputs, producer scripts, source/build/configuration identities, executable identities, command outcomes, and digest-linked sidecars. Expanded reports are retained.

## Behavior and reproduction coverage

| Area | Reproduction and verified result |
| --- | --- |
| Dependency selection | Real Ninja and Make regeneration after CMake edits retains the selected absolute dependency root with the build environment unset or changed. Explicit overrides, `-U` reset, failed initial configuration, relative roots, spaces, symlinks, misleading `PWD`, and legacy cached overrides are exercised. |
| Fresh versus pending input | Fresh argv and libedit prose above 65,536 bytes reaches the provider, including an initial image. Pending raw input remains capped at 65,536 bytes; cached-editor bounds remain unchanged. Scope is frozen before preparation. |
| Payload budgets | Pending submissions retain independent 8,388,608-byte effective-content and retained-text budgets, plus the 100-entry and 262,144-byte custom-expansion constraints. Exact fit, overflow, malformed UTF-8 growth, tiny expansions with large originals, shared backing, reservation cleanup, and allocation failures are exercised. |
| Preparation work | Immutable normalized backings move into the queue without allocation during commit/handoff. A fault regression rejects aggregate expansion growth before allocating the expanded backing. Valid command originals alias the normalized backing; separately retained raw/normalized/expanded strings are charged once per distinct backing within each entry. |
| Rejection | Line and curses preparation failures retain both the submitted-text/pending-message notice and the specific error. Later FIFO input works. Cancellation, completed-result publication, command freezing, and original hook input are retained. |
| Images | Interactive `/memory consolidate` disposes initial images with a warning. A portable fake-artifact execution test checks provider attachment ownership; real Darwin codec integration remains separate. Headless memory semantics are unchanged. |
| Cleanup/barriers | Reserved placeholders never appear in `take_pending()` output. Their accounting is cancelled and outstanding leases may safely destruct later. Prepared commands retain a follow-up barrier. |
| Tooling | Missing/malformed historical metadata raises identifying `ValueError`s. Controls reject skipped runs. Sysroot manifest validation requires its lock argument. Explicit hash-profile and historical audit support remains. |

The retained-text budget bounds retained submission payload, not total process memory, object overhead, string capacity, or bounded presentation storage. Fresh ordinary prose intentionally remains outside pending aggregate limits. Filesystem lookup remains synchronous and has no bounded-latency guarantee.

### CMake boundary clarification

The selection is committed atomically at successful **project configuration completion**, using APIs available at the declared CMake minimum. CMake can subsequently fail generation after writing generated files; CMake 3.20 offers no post-generation callback. This implementation therefore retains the validated root across such later generation errors until an explicit override or `-UCODET_DEPENDENCY_ROOT`. The behavior was disclosed during implementation, documented, and tested. It does not claim a pin only after every generator operation succeeds. An initial project-configuration failure does not pin a derived choice. Installed CMake 4.4.4 ran these fixtures; a CMake 3.20 executable was unavailable.

## Captured validation

| Check | Result |
| --- | --- |
| Debug CTest | 12/12 passed |
| ASan/UBSan CTest | 12/12 passed; unit cases isolated with retained per-case outcomes |
| Focused input/diagnostic/lifecycle | 129 tests per repetition, 20 repetitions passed |
| Applicable TSan coverage | 129 focused tests per repetition, 20 repetitions passed |
| Python 3.9 | Verified `/usr/bin/python3` 3.9.6; tooling, attempt auditing, Tiger fixtures, and integration checks passed |
| Pinned lint, Semgrep, runner checks | Passed |
| Negative controls | 46 native/integration, 42 Python, and 2 production-site Semgrep controls passed; passing unmodified baselines and intended mutant assertions were required; skips, discovery, compilation, launch, and timeout failures do not count |
| Analyzer self-tests and comparison | Both variants completed 25-source coverage. 147 Clang-Tidy findings and 1 Clang Analyzer finding remain nonzero observations; Cppcheck has 0 findings. The comparison accepted 42 explicit source-bound location-only moves and no new/removed findings. |
| Host cross-selection | Configure/build/tests passed using the declared host placeholder archive; this is not PPC linking or native Tiger acceptance |
| Platform availability | Darwin host validation passed. Docker, Podman, and the PPC compiler were unavailable. Ubuntu Unicode, native PPC/Tiger cross-build, and target runtime acceptance were not established. |

Two interrupted development captures are preserved as declared historical inputs: one stopped during Debug CTest and one during ASan build. They retain their original interrupted outcomes and cannot satisfy the final campaign. A separately prepared, never-executed attempt has no invented outcomes. No older CI run substitutes for changed-source coverage.

The compiler configuration includes the recorded host warning accommodations `-Wno-error=implicit-int-float-conversion -Wno-error=unused-result` identically for both Release variants. No analyzer finding was relabeled as a passing analyzer check.

## Fresh measurements

The strict quiet-environment check passed. Both immutable production variants used the same declared harness/support snapshot and compiler/configuration settings. Each of two sets contains one warm-up and five alternating measured runs per variant for 33 workloads: 792 rows total, 660 measured. Positive controls verify observed allocation, write, metadata, and geometry counters. Samples include preparation/queueing, expansion, pending presentation, unchanged flushes, diagnostics, exports, dependency resolution, build-context inspection, Git inspection, hashing, and cleanup.

Representative medians in milliseconds (baseline → changed):

| Workload | Set 1 | Set 2 |
| --- | --- | --- |
| Bounded expansion | 83.320 → 81.573 | 82.821 → 81.650 |
| Preparation/queue | 6.224 → 6.263 | 5.989 → 5.931 |
| Unchanged row flush | 0.151 → 0.145 | 0.147 → 0.147 |
| Amplified diagnostics | 408.334 → 419.328 | 403.295 → 405.427 |
| Large export | 265.036 → 263.311 | 263.107 → 262.990 |
| Dependency resolution | 324.176 → 204.403 | 323.304 → 204.254 |
| Git inspection | 108.298 → 117.326 | 109.886 → 114.835 |

For the bounded-expansion harness invocation, counted allocated bytes fell from 113,599,384 to 80,050,072 in both sets; preparation/queue counted 440,216 versus 428,952. These are harness-scoped allocation counters, not total process memory. Git inspection traverses 127 versus 129 files, so those timings are not evidence of equal work. Python counters distinguish explicit stat calls, DirEntry calls, retained traced blocks/bytes, and peak traced bytes; they do not establish kernel syscall counts or allocation-event totals. Full observed differences, including slower results, are published without timing thresholds or a general speed claim. No distribution-input or full-provenance parsing optimization was introduced.

## Audit and replay

From the checkout, audit the immutable bundle with:

```sh
python3 scripts/validation_evidence.py verify-bundle docs/validation/submission-budget-config-followup/41f02d408f76444da6a7870352af5119
```

The offline receipt includes its complete driver source. It extracts top-level verifier scripts from digest-checked `fingerprint:validation` archive entries into temporary storage, imports those archived scripts, verifies a relocated bundle with original checkout access denied, and proves corruption rejection. Original checkout paths remain opaque provenance.

The exact capture is single-attempt and cannot be reused. To run another campaign, resolve a fresh folder and manifest using `scripts/evidence_campaign.py prepare --help`; use explicit root, baseline, interpreter, tool, and output settings. The archived manifest is the exact executed command record, not a portable command to run unchanged at another checkout path. Existing immutable destinations are never overwritten.
