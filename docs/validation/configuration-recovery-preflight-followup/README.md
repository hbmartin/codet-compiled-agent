# Recoverable overrides and guarded submission preparation

Fetched baseline: `214cb3d3c29e06e34d6232be5dc6defc2d8f27d4` (merged PR #65). Frozen final implementation, tests and producers: `ec857e520d1ecbe416fb468970fb1a06e4619184`. Branch: `codex/configuration-recovery-preflight-followup`. Later changes add only this evidence; the original checkout and historical reports remain unchanged.

**Final benchmark acceptance is unavailable.** Run [`1e8d1c6f2a2644b5a01a82f2d9260b7a`](1e8d1c6f2a2644b5a01a82f2d9260b7a/manifest.json) captured all 40 check outcomes. **37 of 38 required checks passed**; `benchmarks` remains failed (exit 1). Two separate analyzer observation checks retain their actual exit 1. This is an explicitly incomplete evidence-v5 attempt, with `complete=false`, `eligible_current=false`, and zero accepted final measurement samples. Integrity verification does not grant campaign acceptance.

## Changes and reproductions

- A receipt file alone no longer owns a newly created cache. Explicit typed/untyped dependency-root overrides replace current receipts before those receipts are parsed. Ordinary reuse still requires a valid matching receipt. Real CMake tests reproduce orphan, truncated and mismatched receipts, including equal-valued overrides.
- Obsolete owned cache metadata still requires reset; there is no migration. Errors name `CMakeCache.txt` and explain resetting and reapplying the desired path. Reset with `-UCODET_DEPENDENCY_ROOT` in one configure, then reapply `-DCODET_DEPENDENCY_ROOT=/chosen/root` in a separate configure, or recreate the build directory. Empty untyped overrides retain their reset behavior.
- Cache inspection preserves both values and unset properties. Help text again includes human reset guidance while retaining the ownership marker needed for equal-valued explicit assignments. The prior current-format marker spelling remains readable without changing selection origin. Scoped variables, successful-selection pinning, literal-dollar receipts and actual-cwd resolution remain covered by real Ninja/Make tests.
- An explicitly supplied expansion with `command=false` now fails before allocation, accounting or commitment. Raw-limit errors retain precedence. Tests verify no steering content escapes, the reservation remains safely releasable, and later input works. Ordinary prepared prose and barrier-only commands remain valid.
- Borrowed raw-input validation is carried through preparation without repeatedly validating the same bytes. Advisory preflight remains non-mutating, normalized sizes are reused, and final insertion remains atomic under the existing mutex. Raw hook bytes, backing sharing, independent budgets, FIFO and cancellation precedence remain intact. Guarded self-move behavior is retained.
- The preparation benchmark is now part of the default host test build. A checked adapter is generated from the actual CLI preparation lambda with declared source/script dependencies. CTest smoke runs all eight workloads. New preparation metric version 2 validates workload/variant types, requires zero payload-sized allocations in changed saturated samples, and rejects duplicate changed command wrappers. Historical metric version 1 retains its original observation policy.
- Allocation measurement uses scope-bound restoration, including nested scopes, `bad_alloc` and ordinary exceptions, without resetting unrelated fault state.

Named regressions are in `DependencyReconfiguration`, `SubmissionQueue`, `InputPreparation`, `PreparationFault` and `PreparationRecovery`. They cover the public admission path, gated turn/session changes, saturated queues, malformed UTF-8 growth, error ordering, immutable hook/provider content and normal benchmark generation. The real CLI integration suite retains line/curses, command-freezing, image, request, FIFO and completion regressions.

## Captured validation and unavailable coverage

| Check | Actual result |
| --- | --- |
| Full Debug CTest | 13/13 passed |
| Full ASan/UBSan CTest | 13/13 passed, including audited per-case Darwin unit isolation |
| Focused input/diagnostic/lifecycle | 20 repetitions passed |
| Applicable TSan suite | 20 repetitions passed |
| Python 3.9 | `/usr/bin/python3` verified as 3.9.6; tooling, attempt audit, Tiger/configuration fixtures and all 138 CLI integration tests passed |
| Pinned lint, Semgrep, runners, offline provisioning | Passed |
| Host cross-selection configure/build/tests | Passed; not PPC or native Tiger acceptance |
| Native / Python-CMake / production Semgrep controls | 60 / 55 / 2 passed |
| Baseline and changed analyzer runs | Each exited 1 as an observation: 147 Clang-Tidy findings, zero Cppcheck findings, one Clang analyzer finding |
| Strict analyzer comparison | Passed; no added/removed findings and no location relocations required |
| Final benchmark check | Failed, exit 1; measurement eligibility unavailable |

Mutation controls require a passing baseline and the intended executed assertion failure. Skips, discovery/compilation/launch errors and timeouts do not count. Known analyzer findings and their real nonzero exits are preserved, not relabeled as successful analyzer checks.

The selected CMake 4.4.4 was exercised. A CMake 3.20 runtime was not exercised. Docker, Podman and the PPC compiler were unavailable, so Ubuntu Unicode, actual PPC builds and native Tiger runtime acceptance remain unavailable. Host fixtures do not establish target acceptance. No older CI result substitutes for this source. Tiger provenance format 3, sysroot format 2 and the hashed sysroot builder are unchanged.

## Measurements and rejected consolidation

Two complete **limited measurement campaigns** precede the final attempt. Each has its own eight-check manifest, capture identity, immutable source snapshots, configurations, build/tool/library/executable identities, logs, raw samples and offline audit. They do not replace the final failed benchmark check.

| Phase | Baseline / change | Capture run | Samples |
| --- | --- | --- | --- |
| Baseline / correctness | `214cb3d` / `acfc52e` | `8ddfdc846d5a48c8878e6e34d0ddf2dc` | 1,008 raw; 840 measured; 42 workloads |
| Correctness / candidate | `acfc52e` / `5378eb0` | `b0ec47e6c343492897926178f2d1d1b6` | 1,008 raw; 840 measured; 42 workloads |

Both used one warm-up plus five alternating measured runs per variant, repeated twice, in verified quiet windows. Their declared shared harness/support hashes match. The final production fingerprint equals the correctness variant's fingerprint; validation-input fingerprints differ. Those source-equivalent runtime measurements remain attributed to their original captures and do not satisfy final campaign acceptance.

The separate consolidation candidate reduced observed advisory budget callbacks from 256 to 128 for 128 submissions, with unchanged observed allocation counts in that workload. It failed the retention criteria:

| Candidate versus correctness | Set 1 median difference | Set 2 median difference |
| --- | ---: | ---: |
| Advisory-check workload | -10.09% | -1.44% |
| Compatibility preparation and consumption | +2.30% | +1.45% |
| Normalization | +3.09% | +2.02% |
| Actual CLI preparation | +5.85% | +3.24% |

Compatibility preparation was slower in all ten measured pairs; normalization in nine, and CLI preparation in eight. The candidate was **rejected and reverted**. All samples and differences, including other increases, remain published. No general speed claim or timing-based CI threshold is made.

`advisory_budget_checks` counts the instrumented budget callback in its declared preparation path, not native mutex or kernel operations. Allocation counters cover the overridden C++ `operator new` calls and requested bytes, not total allocation activity, RSS or string capacity. Payload-sized means at least 32,768 requested bytes. Wrapper counts sum within-submission wrapper identities. Positive controls and the published raw data define each scope. An unsuccessful development native-mutex counter was rejected by its positive control and is not used as evidence.

The first measurement capture rejected a still-running owned test helper; its original records/logs and derived artifacts are preserved through the final `historical-attempt` artifact and archived-input index. The retried limited campaign completed. In the final full campaign, process inspection returned `EINVAL` from Darwin `KERN_PROCARGS2` after the eight first-set preparation workloads. This did not establish a quiet window; it does not prove contamination. No complete measurement artifact was produced, no partial sample set is accepted, and no missing samples or outcomes were reconstructed.

## Immutable audit carrier and replay

[`transport.json`](1e8d1c6f2a2644b5a01a82f2d9260b7a/transport.json) records the lossless carrier digests, every-member round-trip verification and its explicit-argument packaging driver. Expanded reports are unchanged; report compaction remains deferred.

- [`audit-bundle.tar.gz`](1e8d1c6f2a2644b5a01a82f2d9260b7a/audit-bundle.tar.gz): the complete final **incomplete-attempt** inventory, snapshots, configs, build metadata, outcomes, logs and accepted derived artifacts.
- [`correctness` supplement](1e8d1c6f2a2644b5a01a82f2d9260b7a/supplements/correctness/manifest.json) and [`consolidation` supplement](1e8d1c6f2a2644b5a01a82f2d9260b7a/supplements/consolidation/manifest.json): separate complete limited measurement bundles, each with original samples/medians and audit receipts. CSV previews are copies of the unchanged archived sidecars.
- [`offline-audit.json`](1e8d1c6f2a2644b5a01a82f2d9260b7a/offline-audit.json): relocated verification using archived tooling under Python 3.9.6, with original-checkout I/O and subprocess launches forbidden. Deliberately corrupted archived input was rejected. The same protections were verified for both supplements.

Extract the final carrier into a fresh directory and audit the attempt:

```sh
mkdir -p /tmp/codet-recovery-audit
tar -xzf docs/validation/configuration-recovery-preflight-followup/1e8d1c6f2a2644b5a01a82f2d9260b7a/audit-bundle.tar.gz -C /tmp/codet-recovery-audit
python3 /tmp/codet-recovery-audit/1e8d1c6f2a2644b5a01a82f2d9260b7a/verify-attempt.py /tmp/codet-recovery-audit/1e8d1c6f2a2644b5a01a82f2d9260b7a
```

Exit 0 verifies attempt integrity only; `eligible_current` must remain false and `unmet_required` must retain `benchmarks: failed`. Supplement archives expand under their own capture IDs and can be checked with the evidence-v5 bundle verifier. The audit receipts include the self-contained relocation driver and exact arguments. Auditing does not rebuild or rerun binaries. Executables, SDKs, tools and dependency installations retain identities and references rather than being bundled.

Use the recorded concrete manifest to inspect exact commands and outcomes. Prepare any replay in a new campaign directory with explicit root/interpreter options; never reuse an old capture as a new outcome. Public interfaces, Unicode policies, storage formats, terminal ownership and diagnostic ordering remain unchanged.
