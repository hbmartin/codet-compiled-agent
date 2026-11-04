# Completed-input cleanup: captured results

The source freeze is `54261a385f7908198048ab17da85f0f6ace3e2a5`. The fetched baseline is `6669219b85d74a28ba935c92d7dc039054535c83`. Work used the existing isolated worktree on `codex/completed-input-cleanup-followup`; the original checkout was preserved.

The immutable [v5 audit bundle](20261104T193903Z-54261a3/report.json) records 43 checks: **41 required checks passed**; baseline and changed analysis each returned exit 1 for the same known findings. The [manifest](20261104T193903Z-54261a3/manifest.json) contains the concrete commands, controlled environment, resolved tools, dependencies, build outputs, and producing-check links. Source and validation fingerprints were rechecked during publication. No historical report, sample, manifest, or archived source blob was rewritten.

## Runtime results

Successful worker completion now publishes usage, the requested output file, and an answer-display attempt before pending-input handling can wait or stop. Late quit/EOF exits normally; late SIGINT/SIGTERM discards follow-ups and returns to a usable interactive prompt after quiescence. Real file/display errors remain errors and do not rewrite persisted completion. Active/completed dispatch shares request/origin validation, command policy, rejection reporting, and FIFO ownership. Existing #55 handoff, quit, and ordering tests remain enabled.

A frozen line uses a destination/boundary receipt for final-newline admission. Intervening records defer and invalidate presentation; successful final-newline delivery is irrevocable. Ordinary unterminated fragments concatenate with ordinary diagnostics but establish a boundary before input presentation. The control-only editor consumes overflow before its retained prefix, with saturation staying nonrecoverable until a line boundary.

The [native controls](20261104T193903Z-54261a3/artifacts/native-negative-controls/native-negative-controls.json) contain 22 passing-baseline/intended-failure controls, including completion publication, late quit/signal behavior, command/FIFO/origin handling, receipt admission, fragment separation, newline acknowledgement, and overflow editing. The [Python controls](20261104T193903Z-54261a3/artifacts/python-negative-controls/negative-controls.json) add 28 and [Semgrep controls](20261104T193903Z-54261a3/artifacts/semgrep-controls/semgrep-controls.json) add two. Compilation, discovery, timeout, launch, and unrelated failures are not counted as successful controls.

## Validation

Debug and ASan/UBSan each passed all ten CTest targets. Debug uses the normal in-process unit suite. Host Darwin ASan uses the audited fresh-process launcher described below. Python 3.9.6 passed 123 evidence-tooling tests and all 113 integration tests. Runner checks passed 37 tests. The new gated integration campaign passed 140 Debug cases (seven methods repeated 20 times) and seven matching TSan cases. The native focused and TSan filters each passed 93 cases per repetition for 20 repetitions; the separate ASan helper repeated 20 times. Pinned lint and Semgrep passed.

Every check's actual outcome is retained below; known analyzer observations are not successful checks.

| Check | Policy | Outcome | Exit |
| --- | --- | --- | --- |
| [provision-offline](20261104T193903Z-54261a3/capture/provision-offline.log) | required | passed | 0 |
| [debug-configure](20261104T193903Z-54261a3/capture/debug-configure.log) | required | passed | 0 |
| [debug-build](20261104T193903Z-54261a3/capture/debug-build.log) | required | passed | 0 |
| [debug-ctest](20261104T193903Z-54261a3/capture/debug-ctest.log) | required | passed | 0 |
| [asan-configure](20261104T193903Z-54261a3/capture/asan-configure.log) | required | passed | 0 |
| [asan-build](20261104T193903Z-54261a3/capture/asan-build.log) | required | passed | 0 |
| [asan-ctest](20261104T193903Z-54261a3/capture/asan-ctest.log) | required | passed | 0 |
| [asan-helper-20](20261104T193903Z-54261a3/capture/asan-helper-20.log) | required | passed | 0 |
| [tsan-configure](20261104T193903Z-54261a3/capture/tsan-configure.log) | required | passed | 0 |
| [tsan-build](20261104T193903Z-54261a3/capture/tsan-build.log) | required | passed | 0 |
| [tsan-20](20261104T193903Z-54261a3/capture/tsan-20.log) | required | passed | 0 |
| [focused-20](20261104T193903Z-54261a3/capture/focused-20.log) | required | passed | 0 |
| [python39](20261104T193903Z-54261a3/capture/python39.log) | required | passed | 0 |
| [python39-integration](20261104T193903Z-54261a3/capture/python39-integration.log) | required | passed | 0 |
| [lint](20261104T193903Z-54261a3/capture/lint.log) | required | passed | 0 |
| [semgrep](20261104T193903Z-54261a3/capture/semgrep.log) | required | passed | 0 |
| [runner-tests](20261104T193903Z-54261a3/capture/runner-tests.log) | required | passed | 0 |
| [cross-placeholder](20261104T193903Z-54261a3/capture/cross-placeholder.log) | required | passed | 0 |
| [cross-configure](20261104T193903Z-54261a3/capture/cross-configure.log) | required | passed | 0 |
| [cross-build](20261104T193903Z-54261a3/capture/cross-build.log) | required | passed | 0 |
| [cross-tests](20261104T193903Z-54261a3/capture/cross-tests.log) | required | passed | 0 |
| [historical-attempt](20261104T193903Z-54261a3/capture/historical-attempt.log) | required | passed | 0 |
| [input-integration-debug](20261104T193903Z-54261a3/capture/input-integration-debug.log) | required | passed | 0 |
| [input-integration-tsan](20261104T193903Z-54261a3/capture/input-integration-tsan.log) | required | passed | 0 |
| [baseline-source](20261104T193903Z-54261a3/capture/baseline-source.log) | required | passed | 0 |
| [baseline-configure](20261104T193903Z-54261a3/capture/baseline-configure.log) | required | passed | 0 |
| [baseline-build](20261104T193903Z-54261a3/capture/baseline-build.log) | required | passed | 0 |
| [baseline-selftest](20261104T193903Z-54261a3/capture/baseline-selftest.log) | required | passed | 0 |
| [baseline-analysis](20261104T193903Z-54261a3/capture/baseline-analysis.log) | observation | failed | 1 |
| [changed-configure](20261104T193903Z-54261a3/capture/changed-configure.log) | required | passed | 0 |
| [changed-build](20261104T193903Z-54261a3/capture/changed-build.log) | required | passed | 0 |
| [changed-selftest](20261104T193903Z-54261a3/capture/changed-selftest.log) | required | passed | 0 |
| [changed-analysis](20261104T193903Z-54261a3/capture/changed-analysis.log) | observation | failed | 1 |
| [static-comparison](20261104T193903Z-54261a3/capture/static-comparison.log) | required | passed | 0 |
| [semgrep-controls](20261104T193903Z-54261a3/capture/semgrep-controls.log) | required | passed | 0 |
| [python-negative-controls](20261104T193903Z-54261a3/capture/python-negative-controls.log) | required | passed | 0 |
| [native-negative-controls](20261104T193903Z-54261a3/capture/native-negative-controls.log) | required | passed | 0 |
| [benchmark-harness](20261104T193903Z-54261a3/capture/benchmark-harness.log) | required | passed | 0 |
| [hash-experiment](20261104T193903Z-54261a3/capture/hash-experiment.log) | required | passed | 0 |
| [benchmarks](20261104T193903Z-54261a3/capture/benchmarks.log) | required | passed | 0 |
| [platform-availability](20261104T193903Z-54261a3/capture/platform-availability.log) | required | passed | 0 |
| [build-metadata](20261104T193903Z-54261a3/capture/build-metadata.log) | required | passed | 0 |
| [reproductions](20261104T193903Z-54261a3/capture/reproductions.log) | required | passed | 0 |

Both analyzer variants completed their own 25-source inventories. [Comparison](20261104T193903Z-54261a3/artifacts/static-comparison/analysis.json) reports 150 Clang-Tidy findings, zero Cppcheck findings, and one Clang analyzer finding in each variant. The 49 Clang-Tidy location shifts use the explicit source-bound relocation registry; there are no added or removed identities. Analyzer commands retain their actual exit 1 outcomes.

## Sanitizer and platform limits

Earlier Apple Clang ASan attempts crashed in system child-side atfork hooks before exec (`os_unfair_lock is corrupt`, StackStore, Objective-C/notification hooks). [Reproduction observations](20261104T193903Z-54261a3/artifacts/reproductions/reproductions.json) and [attempt history](20261104T193903Z-54261a3/artifacts/historical-attempt/history.json) retain these failures/interrupted attempts under their original provenance. They do not satisfy acceptance. An optional LLVM23 ASan preflight failed to compile and is also explanatory, not a passing alternative toolchain check.

Host Darwin ASan test helpers use checked `posix_spawn` close actions; Tiger/non-ASan helpers retain fork/exec. Production process creation is unchanged. The CMake `TEST_LAUNCHER` runs each enabled unit case exactly once in a fresh process, with no retries or removed assertions/sanitizer flags. The [capture-bound isolation artifact](20261104T193903Z-54261a3/artifacts/asan-ctest/unit-isolation.json) verifies all 668 enabled cases against the 669-case inventory: 666 passed and the two existing opt-in cases skipped (TLS network and native Tiger desktop). The pre-existing disabled intentional failure-printer probe remains disabled. Inventory, command/filter/XML names, exits, coverage, and executable before/after hashes are independently verified by the offline auditor.

Local Docker, Podman, and the PPC compiler are unavailable. The passing host cross-selection build verifies test selection without host interposers; it is **not** native PPC/Tiger runtime verification. Ubuntu Unicode runtime coverage is unavailable in this local bundle. A matching PR workflow can provide additional coverage; an older CI run is not attributed to this source. Both Release builds use the same Apple Clang21 configuration and the same two existing warning accommodations (`implicit-int-float-conversion` and `unused-result`).

## Fresh measurements

The [raw 672 samples](20261104T193903Z-54261a3/measurements/benchmarks/measure.json/samples.csv), [112 group medians](20261104T193903Z-54261a3/measurements/benchmarks/measure.json/medians.csv), and [producer/provenance](20261104T193903Z-54261a3/artifacts/benchmarks/measure.json) cover 28 workloads. Each variant has one warm-up and five measured runs in each of two alternating sets: 560 measured samples. Both native variants use one separately declared identical harness/support snapshot against verified immutable production libraries/headers; compiler, configuration, source, link, harness, and executable identities are retained. Quiet-process eligibility passed before and after measured workloads. Earlier attempts blocked by unrelated tests or ambiguous inspection remain history. Measurements finished before the user selected the unavailable-benchmark fallback; these completed samples are reported as observations, not a performance acceptance claim.

All times below are median milliseconds (baseline → changed). No timing thresholds are used. Results are mixed: no universal speedup or unchanged-work claim is made. The fragment workload intentionally adds one safe separator per iteration (2,500 → 2,600 selected stdout bytes and 400 → 500 output operations over 100 iterations). Native unchanged flushes perform zero allocation/preparation/output/metadata work in both variants but show slightly higher measured time. Set 1's native-flush-boundary outlier and set 2's small-export difference are retained, without rerunning or deleting them.

| Harness / workload | Set 1 ms | Set 2 ms |
| --- | ---: | ---: |
| export / large | 288.1921 → 288.2425 (+0.02%) | 268.5112 → 269.3210 (+0.30%) |
| export / small | 80.4336 → 81.1303 (+0.87%) | 57.2420 → 65.5328 (+14.48%) |
| line / allocation-positive | 0.0023 → 0.0023 (+1.79%) | 0.0023 → 0.0023 (-1.80%) |
| line / busy | 0.1531 → 0.1452 (-5.12%) | 0.1531 → 0.1361 (-11.10%) |
| line / deferred-tail | 0.3177 → 0.3233 (+1.74%) | 0.3117 → 0.3198 (+2.61%) |
| line / deletion | 0.1204 → 0.0952 (-20.94%) | 0.1203 → 0.0953 (-20.75%) |
| line / editing | 3.3280 → 2.8385 (-14.71%) | 3.3124 → 2.8350 (-14.41%) |
| line / editing-basic | 2.6055 → 2.6362 (+1.18%) | 2.6145 → 2.6284 (+0.53%) |
| line / geometry-positive | 0.2104 → 0.2097 (-0.36%) | 0.2095 → 0.2123 (+1.33%) |
| line / native-boundary | 1.4589 → 0.9173 (-37.13%) | 1.0015 → 1.1580 (+15.64%) |
| line / native-busy | 1.6492 → 1.7047 (+3.37%) | 0.9295 → 0.8906 (-4.19%) |
| line / native-editing | 26.2604 → 26.0334 (-0.86%) | 21.4017 → 21.1110 (-1.36%) |
| line / native-flush-boundary | 1.0580 → 5.3557 (+406.19%) | 0.8620 → 0.8617 (-0.03%) |
| line / native-pending-fragment | 1.8787 → 2.0529 (+9.28%) | 1.8556 → 2.0595 (+10.99%) |
| line / native-pending-submission | 1.1562 → 1.1646 (+0.72%) | 1.1433 → 1.1653 (+1.92%) |
| line / native-unchanged | 0.1890 → 0.1969 (+4.19%) | 0.1857 → 0.1949 (+4.96%) |
| line / pending-submission | 0.3164 → 0.3218 (+1.70%) | 0.3114 → 0.3139 (+0.82%) |
| line / unchanged | 0.1172 → 0.1172 (+0.03%) | 0.1169 → 0.1170 (+0.07%) |
| output / bounds-positive | 0.0307 → 0.0306 (-0.27%) | 0.0327 → 0.0318 (-2.93%) |
| output / diagnostics-amplified | 420.5781 → 412.6258 (-1.89%) | 418.0960 → 418.6696 (+0.14%) |
| output / diagnostics-pipe-amplified | 731.2291 → 663.3450 (-9.28%) | 480.7634 → 442.1181 (-8.04%) |
| output / diagnostics-standalone-amplified | 431.7600 → 508.2239 (+17.71%) | 406.5099 → 410.3677 (+0.95%) |
| output / flags-positive | 0.0173 → 0.0170 (-1.69%) | 0.0169 → 0.0172 (+1.48%) |
| output / stat-positive | 0.0937 → 0.0943 (+0.62%) | 0.0922 → 0.0925 (+0.27%) |
| output / write-positive | 0.1286 → 0.1263 (-1.81%) | 0.1219 → 0.1243 (+1.98%) |
| tooling / git-inspection | 91.1510 → 94.8355 (+4.04%) | 89.2119 → 95.1488 (+6.65%) |
| tooling / hashing | 32.3761 → 32.9285 (+1.71%) | 32.3881 → 33.7088 (+4.08%) |
| tooling / supervision | 390.6351 → 220.3691 (-43.59%) | 382.3889 → 220.9237 (-42.23%) |

Native allocation/work counters use the instrumented harness and positive controls. The line harness observes **selected stdout** bytes/writes; stderr diagnostics use the separate output harness. Unchanged redraws allocate no full draft. Pending-submission work is unchanged except the required fragment boundary above. Amplified ordinary diagnostics retain 0 allocations, 4,000,000 bytes, 200,000 writes/fstat/fcntl observations in both variants. Small exports retain 50 descriptor writes for 1,320,000 bytes; large exports retain 474 for 30,960,000 bytes, with matching allocation/work counts. All per-sample counters remain available, including geometry/readiness operations.

Tooling metric version 2 distinguishes actual Git command invocations from process-inspection command invocations. Supervision observes five owned processes and zero files, and five process-inspection commands in both variants; it returns sooner when a group is inactive rather than waiting the whole grace period. The harness measures live traced blocks/bytes and peak traced bytes, **not allocation events**. Production hashing/Git workloads cover 116 baseline versus 118 changed production files, so those timings are not an identical-file-count speed comparison. The full artifact explicitly records work differences.

### Hash profiling

The separate [72 hash-profile samples](20261104T193903Z-54261a3/artifacts/hash-experiment/hash-profile.json) use one common immutable input set for both implementations (60 measured samples, three roles, two sets). The new payload's metric version is 2; evidence-container format remains v5. `explicit_stat_calls`, `direntry_is_file_calls`, and `direntry_is_dir_calls` are distinct profiled Python API calls. Cached method calls are not summed into claimed kernel operations. `retained_traced_blocks`, `retained_traced_bytes`, and `peak_traced_bytes` are live/peak traced memory, not total allocation activity. Positive controls and per-function profiles verify counts independently of unprofiled timing. Historical version-1 samples keep their original labels/interpretation.

No hashing optimization is retained by this follow-up. The latency gate is false and observed API-call counts match. The table retains the slower samples as measured.

| Role | Files | Explicit stat / is_file / is_dir | Set 1 ms | Set 2 ms |
| --- | ---: | ---: | ---: | ---: |
| production | 118 | 20 / 117 / 122 | 53.2641 → 61.8343 | 52.4028 → 54.8008 |
| validation | 137 | 33 / 116 / 118 | 51.2442 → 51.6768 | 52.0882 → 54.0035 |
| build | 289 | 44 / 265 / 274 | 123.8549 → 126.0938 | 120.5949 → 116.0708 |

## Offline audit and preserved evidence

[Post-publication audit commands/results](20261104T193903Z-54261a3-audits.json) record four successful audits: in place and relocated, each with Python3.12 and Python3.9. Relocated verification reconstructs the verifier from archived validation-source blobs, runs outside the checkout, and verifies 13,243 indexed input references and all 43 check records without rerunning binaries. The audit record binds to the bundle inventory digest. It is a supplemental post-publication observation, not a replacement for captured required checks.

The bundle contains source/config/build snapshots, logs, raw samples, derived sidecars, tools/executable identities, and exact-command provenance. [Inventory](20261104T193903Z-54261a3/inventory.json) and [input index](20261104T193903Z-54261a3/inputs.json) permit offline digest and producer-link checks. It remains an audit archive rather than a relocatable executable distribution. Historical evidence retains its original source identities and outcomes; absent historical results are not reconstructed.

Replay/audit commands are in [README.md](README.md). Public interfaces, Unicode/raw-data policies, completed-journal authority, cancellation precedence, replay order, and terminal ownership remain. Blocking TTY output is still outside the bounded-cancellation guarantee. Emergency delivery remains best effort with accepted partial-output duplicates; no broader serialization or exactly-once guarantee is added.

