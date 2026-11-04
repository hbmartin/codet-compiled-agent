# PR #54 review fixes

The new immutable [v5 bundle](20261104T154408Z-pr54-review-fixes-af3d0d5/report.json) records campaign `6a2c2efd355f45a8b0ad083880e4cd30`. The frozen implementation, tests and control producers are `af3d0d5147ede3cf3858031626626a37d564e120`; fetched baseline remains `ddfbcc0bd7fa216cc12d36c1b5a1d2059b496344`. The unrelated original checkout edits were left untouched. This report supplements, rather than replaces, the [original results](RESULTS.md).

## Runtime corrections

1. Partial control input no longer stalls an already frozen submission. Handoff proceeds after output is available without requiring another Enter. Extra typing remains control-only through its line boundary, including across handoff and return to idle; it cannot leak a suffix into a later user message. Completed quit controls remain available.
2. A frozen `/quit` or `/exit` is honored when the originating worker has already completed successfully. The CLI exits without starting an earlier held follow-up, and the persisted completed turn remains complete.
3. Presented-but-held and still-pending ordinary lines now use one ordered staging sequence. A later line cannot overtake an earlier held line by entering either the steering path or the post-completion follow-up queue.

The internal lifecycle observation seam is optional and nonthrowing. Existing CLI signatures remain wrappers with no observer. It allows tests to gate the exact return from `work.get()` instead of relying on sleeps.

## Reproductions and assertion controls

The unchanged reviewed head `8c74f585` reproduced all three defects with admitted diagnostic reservations. The new PTY helper gates input consumption, frozen ownership, handoff and successful worker completion independently. Tests assert both terminal output and exact provider message order/counts.

- A partial ordinary or overflowing control draft must not delay frozen `abc`. An input-consumed gate precedes diagnostic release, and the handoff gate must arrive before another Enter. Ignored input is completed both during active work and after return to idle; a subsequent fresh message succeeds.
- A partial `/qu` remains available as a quit control after the original line hands off.
- Both `/quit` and `/exit` are frozen behind output while an earlier follow-up is held. The test waits for successful worker completion before releasing output, then requires prompt exit, one provider request, pending-message rejection and complete persisted status.
- FIFO is tested with the later line immediately presented and separately with it still pending behind a second reservation.

Five new native controls independently restore the partial-control progress guard, remove control-line ownership, lose completed quit, and reverse follow-ups through either admission path. All pass on unchanged code first, apply exactly one mutation, then fail through the intended assertion marker. Compilation, discovery, launch errors and timeouts do not count. The complete native control suite now has 15 controls.

## Fresh captured validation

All **33 required checks passed**. Two analyzer observations retain actual exit status 1 for known findings. The [concrete manifest](20261104T154408Z-pr54-review-fixes-af3d0d5/manifest.json) and bundle preserve commands, controlled environments, cwd relationships, production/validation/build inputs, executable identities, logs and outcomes.

- Debug CTest: 10/10 targets; ASan/UBSan CTest: 10/10.
- Ordinary and TSan focused native suites: each 20 repetitions of 81 tests.
- The four new pending-input integration methods: 20 repetitions, 80 tests; another four-test run passed against the TSan build.
- Python 3.9.6: 106 tooling tests and all 107 CLI integration tests.
- Pinned lint, Semgrep, enforcement runners, analyzer canaries and host cross-selection configure/build/tests passed.
- Controls: 15 native, 21 Python and two production-site Semgrep controls.
- Complete 25-source analyzer inventories matched: 150 Clang-Tidy findings, zero Cppcheck findings and one Clang Analyzer finding per variant. Five reviewed location moves passed the source-bound identity/context checks. There were no added or removed findings. Known nonzero analyzer results remain observations, not clean scan claims.

This review-fix campaign scopes correctness validation separately from measurements. It omits the five performance-only checks `baseline-build`, `changed-build`, `benchmark-harness`, `hash-experiment` and `benchmarks`, and adds `pending-review-20` and `tsan-pending-review`. The Release configuration caches support analyzer/build metadata; this campaign does not claim new Release benchmark binaries or measurements.

## Audit and limitations

The [original-location audit](20261104T154408Z-pr54-review-fixes-af3d0d5-audit-original.json) and [relocated audit](20261104T154408Z-pr54-review-fixes-af3d0d5-audit-relocated.json) both verify format v5, all 35 check records and 10,328 audit-input records. The relocated audit ran from `/private/tmp` under Python 3.9 with verifier scripts reconstructed from archived validation inputs. It checks inventory, digests, declarations, producer links, outcomes and cwd relationships without rerunning binaries or resolving the original checkout for content lookup.

The original `20261104T073413Z-ab2fde3` bundle, historical reports and all samples remain unchanged. Its 600 benchmark samples and isolated hashing experiment describe frozen source `ab2fde3`, **not this amended runtime**. No new performance claim is made for these review fixes.

Docker, Podman and the PPC compiler remain unavailable locally. Host cross-selection does not establish native Tiger/PPC runtime coverage. No source-matched Ubuntu CI result is included in this local bundle. The existing Apple Clang warning accommodations remain unchanged; no new analyzer suppression was added. Blocking-TTY delivery, best-effort presentation on actual output failure and intentional discarding of extra ordinary typing during a frozen line retain their existing policies. The two inapplicable Sonar alerts and GitHub review threads were left untouched.

## Delivery after PR #54 merged

PR #54 merged as `56f6be798d363561d7ba27d7ee1394855e8e1c6b` while these corrections were being validated. The three local commits were replayed onto that freshly fetched main for a separate follow-up PR. Production, validation and build-input fingerprints remain identical to the captured source; commit ancestry changed, not the validated inputs. The bundle retains its original capture identities and is not rewritten to imply a second execution.
