# Reliable configuration pins and atomic submission ownership

This follow-up starts from freshly fetched `7e453200f7e7ceec0ff82d54a7f8e5eb0238251b` (the merge of #63) and validates frozen implementation `92499f646e0bb23c005fd51956581f150b927ee6`. The isolated worktree was reused; the original checkout and historical evidence were preserved. No review thread was changed.

## Current evidence

Capture `c3ca1ccd18d2446f8f1b5b5fcf5ce95d`, campaign `53c17c1d96b74d69a6b04dd35e240fa5`:

- [Complete immutable v5 bundle, lossless transport](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/audit-bundle.tar.gz).
- [Transport digest, exact member round-trip check, and packaging source](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/transport.json).
- [Exact command manifest](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/manifest.json), [raw samples](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/samples.csv), and [recomputed medians](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/medians.csv). These previews are byte-identical copies of the archived members.
- [Relocated offline audit](c3ca1ccd18d2446f8f1b5b5fcf5ce95d/offline-audit.json): Python 3.9.6 verified all 39 checks and 12,685 audit-input declarations with original-checkout access and subprocess launches blocked. Corrupting an archived input failed inventory verification. This audits evidence and does not rerun binaries.

All **37 required checks passed**. The other two checks retain actual nonzero analyzer observations; their required strict comparison passed. The archive contains all 472 original bundle files (382,827,082 bytes), including expanded reports, source snapshots, configs, build metadata, executable/tool identities, logs, samples, producer links, and the digest-keyed input index. No report was compacted or outcome reconstructed. Lossless gzip transport is necessary because the expanded report and native-control artifact exceed GitHub's individual-file limit. The transport is post-publication packaging, not a new captured check or a substitute for campaign acceptance.

## Behavior and regressions

| Area | Verified behavior |
| --- | --- |
| Selection/reset | Empty `-DCODET_DEPENDENCY_ROOT=` and `-UCODET_DEPENDENCY_ROOT` reset both explicit and derived selections. Real Ninja and Make regeneration retain committed absolute selections with changed/unset environments and different build cwd. Relative paths, spaces, symlinks, explicit precedence, and actual invoking cwd are covered. |
| Configuration success | A generated receipt commits derived choices only after project configuration succeeds. Initial `SEND_ERROR` and `FATAL_ERROR` runs leave no usable derived pin. Provisional cache values are not promoted into explicit overrides. A later generation error may retain a selection after project configuration succeeded. |
| Scope and recovery | Toolchain/parent normal variables remain scoped, are not cached, and do not replace a remembered derived pin. Removing them restores discovery/pinning. Corrupt/obsolete records identify their path and reset procedure. JSON escaping is explicitly included, including standalone helper use. Missing dependencies identify the selected root, missing input, and provisioning/override/reset options. |
| Payloads/accounting | A validated immutable payload owns original bytes, normalized original content, optional normalized expansion, and checked sizes. Unchanged backings alias within an entry; distinct raw/normalized/expanded strings are charged once. Malformed compatibility expansions normalize before commitment, and charged effective bytes match the committed normalized bytes. |
| Atomic admission | Compatibility submission prepares outside the interaction mutex, then checks capacity, classifies, inserts, and accounts in one locked transition. The deterministic gate performs turn cleanup/restart during preparation without exposing a reservation or losing the later admission. Frozen CLI reservations retain their owning lease and command barriers. |
| Rejection | Actual count/effective/retained errors survive admission and are reported once with the pending-message notice. Line/curses tests exercise all three constraints and later FIFO usability. No false count-limit error replaces a byte-budget error. |
| Images | Portable tests use the real CLI image-disposition path with an injected validated-image backend and recording HTTP provider. They exercise accepted image turns, image-only/large fresh prompts, local commands, memory consolidation, failed/cancelled preparation, and unrelated later input. Real Darwin codec integration remains separate. The memory-disposal control waits for and checks the actual memory provider request. |
| Cleanup | No reserved placeholders escape `take_pending()`. Outstanding leases remain safe after cleanup. Cancellation, original hook input, command freezing, request identity, completed answers/output files, allocation faults, and FIFO regressions remain covered. |

Pending limits remain 65,536 raw bytes, 262,144 normalized expansion bytes, 100 entries, and independent 8,388,608-byte effective-content and retained-text budgets. Fresh argv/libedit prose retains its prior larger-input behavior; the cached editor remains bounded. Retained-text accounting bounds payload bytes, not string capacity, object overhead, or total process memory. Filesystem lookup remains synchronous with no bounded-latency guarantee.

There is no migration for prior pin records. Reset existing build directories with `-UCODET_DEPENDENCY_ROOT` (or an empty override), or recreate them. Explicit cache selections remain authoritative. This implementation uses CMake-3.20-compatible APIs; installed CMake 4.4.4 ran the fixtures, and a 3.20 executable was unavailable.

The sysroot builder was not edited. #63's conservative helper-source cache invalidation remains documented; sysroot format 2 and Tiger provenance format 3 are unchanged. No cache reuse or pruning was introduced.

## Captured validation

| Check | Outcome |
| --- | --- |
| Debug CTest | 12/12 passed |
| ASan/UBSan CTest | 12/12 passed; audited per-case unit isolation retained |
| Focused input/diagnostic/lifecycle | 131 tests per repetition, 20 repetitions passed |
| Applicable TSan coverage | 131 tests per repetition, 20 repetitions passed |
| Python 3.9.6 | 135 evidence-tooling tests, 32 Tiger/configuration fixtures, attempt-audit and 138 CLI integration tests passed |
| Pinned lint, Semgrep, runner checks | Passed |
| Negative controls | 51 native/integration, 46 Python, and 2 production-site Semgrep controls passed; passing baselines and intended mutant assertions required. Skips, discovery, compilation, launch, and timeout failures do not count. |
| Static analysis | Each variant completed 25 translation units and successful canaries. 147 Clang-Tidy findings, 0 Cppcheck findings, and 1 Clang Analyzer finding remain actual observations. Strict comparison accepted 13 source-bound location-only moves and no new/removed diagnostics. |
| Host cross-selection | Configure/build/tests passed with the declared host placeholder archive; this does not establish PPC linking or native Tiger acceptance. |
| Platforms | Darwin validation passed. Docker, Podman, and the PPC compiler were unavailable. Ubuntu Unicode, target cross-build, and native Tiger runtime coverage were not established; no older CI substitutes for frozen-source coverage. |

Both Release configurations use the same recorded compiler/settings, including host warning accommodations `-Wno-error=implicit-int-float-conversion -Wno-error=unused-result`. Analyzer nonzero observations are not relabeled as successful analyzer checks.

Development preflights rejected stale mutation anchors and an image assertion that inspected requests too early. The assertion and intended markers were repaired before freezing; only the complete fresh campaign satisfies acceptance. Development scratch logs remain separate from the immutable campaign.

## Fresh measurements

Strict quiet-environment checks passed. A shared declared harness/support snapshot compiled against each immutable production variant. Each of two sets contains one warm-up and five alternating measured runs per variant for 34 workloads: **816 rows, 680 measured**. Positive controls verify allocation, output, geometry, metadata, preparation, and configuration counters. All raw samples and computed differences are retained.

Representative medians in milliseconds, baseline → changed:

| Workload | Set 1 | Set 2 |
| --- | --- | --- |
| Bounded expansion | 88.889 → 69.003 | 83.694 → 49.190 |
| Preparation/queue | 7.656 → 6.610 | 6.225 → 6.428 |
| Unchanged row flush | 0.147 → 0.147 | 0.150 → 0.153 |
| Amplified diagnostics | 403.895 → 405.797 | 416.627 → 429.857 |
| Large export | 264.895 → 266.883 | 279.441 → 294.106 |
| Configuration selection/reset | 437.572 → 435.558 | 423.524 → 426.629 |

The new configuration metric version 1 verifies three real CMake commands and three root-selection checks per invocation. It does not claim total metadata syscalls or child-process allocations. Python metrics distinguish explicit stat calls, DirEntry calls, retained traced blocks/bytes, and peak traced bytes; retained blocks are not allocation events.

New ownership objects increase counted preparation/queue allocations from 3,588 to 3,844 (461,720 to 475,032 bytes) for 128 entries. Bounded expansion counts 3,716 versus 3,972 allocations. Frozen-line workloads also record added shared-backing allocations. Commit/handoff itself remains allocation-free in fault tests, and unchanged redraws retain zero counted allocation/output/geometry work. These costs and all slower observations are published. Hashing/Git workloads cover 129 baseline versus 130 changed files, so they do not establish equal work. No general performance improvement or timing threshold is claimed, and no optional optimization experiment was added.

## Offline audit and replay

Extract into a fresh directory, rather than over this transport directory:

```sh
archive=docs/validation/configuration-submission-integrity-followup/c3ca1ccd18d2446f8f1b5b5fcf5ce95d/audit-bundle.tar.gz
shasum -a 256 "$archive"
# Expected: 8aab91040f36ba08b4585007444eb31d9f6d6d5477c4cfeb834d60548fd397cb
mkdir /tmp/codet-configuration-audit
tar -xzf "$archive" -C /tmp/codet-configuration-audit
python3 scripts/validation_evidence.py verify-bundle /tmp/codet-configuration-audit/c3ca1ccd18d2446f8f1b5b5fcf5ce95d
```

The archive's `report.json`, `inventory.json`, `inputs.json`, captures, artifacts, and measurement sidecars are the original strict publication. The offline receipt embeds the complete independent driver: it extracts digest-checked archived verifier tooling, relocates the bundle, denies original-checkout access/process launch, verifies it, and proves corruption rejection. The transport receipt separately verifies every extracted member against the original published bytes. Audit verification does not rebuild or rerun binaries.

The exact manifest is single-attempt, pathful provenance, not a portable command to run unchanged at another checkout. Prepare another fresh folder using `scripts/evidence_campaign.py prepare --help` with explicit root, baseline, interpreter, and tool settings. Immutable destinations are never overwritten. Historical reports, manifests, archived sources, and measurements remain unchanged.
