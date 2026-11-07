# Explicit configuration intent and bounded input preparation

The actual fetched baseline is `b43fb1d286ce0297a2f94a1f6ac1f85a42905010` (merged PR #64). The frozen validated change is `6b3be69eaf4aa95ced0e57c5f6f0aabee1465664` (see the exact resolved SHA in the manifest); later changes add only this evidence. Branch: `codex/configuration-input-preflight-followup`. The original checkout remains unchanged.

The accepted evidence-v5 run is [`68a48c6fee9743c08d6a5e9f21d0aeef`](68a48c6fee9743c08d6a5e9f21d0aeef/manifest.json). All **37 required checks passed**. Two analyzer checks retain actual nonzero observation exits, with matching reviewed findings. No historical report, source blob, sample, or manifest was replaced. Tiger provenance remains format 3, sysroot format 2; the hashed sysroot builder is unchanged.

## Changes and reproductions

- Configuration intent uses private ownership tokens and cache metadata, independent of CMake's English help text. Typed and untyped equal-value `-D` assignments override provisional and committed derived selections. Same-valued normal variables stay scoped, with cache properties preserved. Receipt serialization protects literal `$<...>` paths and keeps temporary variables private.
- Obsolete owned metadata fails with an identifying reset/recreate diagnostic. There is no migration. Reset using `cmake -UCODET_DEPENDENCY_ROOT -S . -B <build>` or an untyped empty `-DCODET_DEPENDENCY_ROOT=`; automatic Ninja/Make regeneration otherwise preserves the committed absolute choice.
- Frozen input now has a const text accessor and ordinary guarded memberwise move assignment. Optional moves and self-moves preserve original bytes, identity, preparation outcome, and one owning lease. The C++17 lifetime defect is removed structurally; ownership mutations exercise the association and cleanup assertions.
- Public compatibility submission checks raw scope and count, inspects borrowed text, and checks both budgets before constructing retained backing. Advisory checks mutate no queue state; final admission still checks and inserts atomically under the existing mutex. A gated capacity/session transition verifies rejection without placeholders or leaked charges.
- The explicit command flag controls barriers independently of prepared content. Barrier-only text shares backing. One custom-command wrapper is shared by the payload and preparation outcome. Ordinary original bytes remain retained for hooks. Invalid live-admission/fresh combinations fail deterministically; cancellation wins, then pending raw size precedes a stored admission failure.
- Pure measurement-verifier tests run without CMake/Ninja. Only the optional real fixture skips when those tools are missing; required measurement production still rejects missing tools.

The named reproductions are in `DependencyReconfiguration` in `tests/test_tiger_recovery.py`, `InputPreparation`/`SubmissionQueue` in `tests/cli_diagnostics_test.cpp`, and `PreparationFault.SaturatedPublicAdmissionRejectsBeforeAnyPayloadSizedAllocation` in `tests/publication_fault_test.cpp`. Line/curses rejection, command freezing, FIFO, images, and completed-result handling remain covered by the real CLI integration suite. Captured native controls (58), Python/CMake controls (49), and production Semgrep controls (2) each require passing baselines and their intended executed assertions; skips, discovery/compilation/launch errors and timeouts do not qualify.

## Captured validation

- Full Debug CTest: **12/12** suites passed.
- Full ASan/UBSan CTest: **12/12** suites passed, using captured per-case unit isolation on Darwin.
- Focused input/diagnostic/lifecycle suite: **20** repetitions passed; applicable TSan suite: **20** repetitions passed.
- Verified `/usr/bin/python3`: **3.9.6**. Tooling, attempt-auditing, Tiger/configuration fixtures and all **138 CLI integration tests** passed under that interpreter.
- Pinned lint, Semgrep, runner self-tests, offline provisioning, host cross-selection compile/link, fresh Release builds, analyzer canaries, negative controls and benchmark verification passed.
- Fresh baseline/change analyzers: **147 Clang-Tidy findings**, **0 Cppcheck findings**, **1 Clang analyzer finding** each. Their real nonzero exits remain observations. The required comparison passed with **13** source-bound, one-to-one reviewed Clang-Tidy relocations and no added/removed findings. No analyzer failures were relabeled as passes.

The first campaign had a separate Python 3.9 integration failure: a control-pipe notification became readable before the fixture collected already-written PTY bytes. The exact visible-output assertion remains; a bounded collection step now follows the independent gate. Its focused rerun passed, then the complete campaign was regenerated. Original logs and all 27 actually captured outcomes from the interrupted first campaign are preserved under `prior-attempt/`, explicitly ineligible for current acceptance. Missing outcomes were not reconstructed.

## Release measurements

The immutable baseline tree and changed libraries/headers were built with matching compiler/configuration settings. Both variants use the same declared harness/support snapshot. For actual CLI preparation, each verified production lambda is extracted verbatim into a declared, archived adapter; source/excerpt/adapter/executable/library hashes and compile/link commands are recorded. This measures the production preparation path without claiming the adapters are identical code or measuring an entire terminal session.

One warm-up and five alternating measured runs per variant, repeated twice in verified quiet windows: **984 raw samples**, **820 measured samples**, **41 workloads**. All samples, computed medians and differences are published, including regressions; no timing threshold or general speed claim is made.

For 128 attempts using a pre-existing 65,536-byte malformed input:

| Workload | Baseline payload-sized allocations | Changed | Baseline allocated bytes | Changed |
| --- | ---: | ---: | ---: | ---: |
| Saturated count | 256 | 0 | 33,597,184 | 18,176 |
| Saturated effective budget | 256 | 0 | 33,605,888 | 26,880 |
| Saturated retained budget | 256 | 0 | 33,603,584 | 24,576 |

Both sets have those counters. Payload-sized means a requested allocation of at least 32,768 bytes. The explicit positive control observes 128 such allocations totaling 8,388,608 bytes. Actual CLI preparation uses 128 distinct command wrappers instead of 256, and 3,332 observed allocations instead of 3,460 for 128 preparations, in both sets. Original hook bytes and expanded content checks still pass.

Selected changed-vs-baseline median latency differences:

| Workload | Set 1 | Set 2 |
| --- | ---: | ---: |
| Actual CLI preparation | +1.35% | -14.87% |
| Compatibility preparation and consumption | +1.61% | +2.11% |
| Normalization | +0.98% | +1.09% |
| Pending submission | -2.90% | +1.03% |
| Unchanged flush | -5.38% | -2.85% |
| Amplified diagnostics | +0.14% | -0.17% |
| Large export | +0.32% | +0.68% |
| Configuration selection | -0.72% | +1.28% |

These host observations include small repeatable increases and inconsistent changes. Allocation counters cover overridden C++ `operator new` calls and requested bytes, not all allocation activity, RSS, or string capacity. Compatibility measurements include consumption copies. Wrapper counts describe observed distinct wrapper identities. Python tooling counters retain their precisely scoped API/tracing meanings. Full traversal, copy, lock and kernel-syscall totals are not inferred from these counters. Existing presentation/output positive controls and bounded preparation/fault assertions remain required. No unchanged-work equivalence is claimed where source sets or counters differ.

## Audit transport and replay

The expanded report and artifacts exceed GitHub's per-file limit. [`audit-bundle.tar.gz`](68a48c6fee9743c08d6a5e9f21d0aeef/audit-bundle.tar.gz) transports every expanded byte without report compaction. [`transport.json`](68a48c6fee9743c08d6a5e9f21d0aeef/transport.json) records its digest, full member round-trip verification, expanded inventory digest and packaging driver. The neighboring manifest/CSV files are previews of the unchanged expanded members.

Extract into a fresh directory and verify with the maintained entry point:

```sh
mkdir -p /tmp/codet-preflight-audit
tar -xzf docs/validation/configuration-input-preflight-followup/68a48c6fee9743c08d6a5e9f21d0aeef/audit-bundle.tar.gz -C /tmp/codet-preflight-audit
python3 scripts/validation_evidence.py verify-bundle /tmp/codet-preflight-audit/68a48c6fee9743c08d6a5e9f21d0aeef
```

[`offline-audit.json`](68a48c6fee9743c08d6a5e9f21d0aeef/offline-audit.json) also records verification using the archived verifier under Python 3.9 after relocation, with original checkout I/O and process launches forbidden. Deliberate archived-input corruption was rejected by the inventory guard. This audits integrity and summaries; it does not rebuild or rerun binaries. Exact commands, environments, tools, configurations, input snapshots, logs, outcomes and samples remain in the expanded bundle. Use `checks/validation-replay.json` and explicit root/folder/interpreter options to prepare a new campaign; immutable captures cannot be reused as new outcomes.

## Coverage limits

The installed CMake **4.4.4** was exercised with real Ninja and Make regeneration. CMake 3.20 compatibility is targeted by the implementation, but a 3.20 runtime was not exercised. Docker, Podman and the PPC compiler are unavailable locally, so Ubuntu Unicode, native Tiger and actual PPC target coverage are unavailable. The passing host cross-selection build and packaging fixtures establish host behavior only. No older CI run is substituted for these sources. Existing blocking-terminal and best-effort delivery limits remain unchanged.
