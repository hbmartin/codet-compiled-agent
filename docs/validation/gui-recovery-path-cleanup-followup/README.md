# Portable campaign paths and interrupt-safe evidence cleanup

**42 of 43 required checks passed; benchmarks are explicitly unavailable.**
This is an integrity-verified incomplete attempt with zero accepted samples,
`complete=false`, and `eligible_current=false`.

Baseline: `555c37c33cad3bf0522d24a6b5787137bb555654` (merged PR #69).
Frozen implementation, tests, and producers:
`f964cae5e3908c79505332e0824f76b58fb31447`.
Capture: [`6a57bd36aa6f46a0b8a384af95753a71`](6a57bd36aa6f46a0b8a384af95753a71/manifest.json).
Campaign: `ced3be389a524ce4b61e98998355fcf1`.

## Fixes and reproductions

Campaign layouts now compare component-wise Unicode NFC/case-folded identities
for collisions, overlaps, reserved locations, and control prefixes. Both submitted
and resolved locations are checked before tool probes or campaign creation, while
I/O retains the actual selected paths and resolved-containment checks. The old
guard accepted `debug`/`Debug`, `Home`, `CAPTURE`, and composed/decomposed Unicode
aliases; the local filesystem confirmed those aliases refer to the same locations.
The new regressions reject them independently of host filesystem case sensitivity.
This is a conservative portable policy for Linux/macOS and Tiger-related host
workflows, not an emulation of every APFS or HFS+ Unicode rule.

Strict Configure-only recovery is retained. After a rejected edit, restore the
previous derived value, Configure, Generate, then edit again. Before the initial
edit, Generate after successful Configure. If this cannot succeed, use an explicit
`-DCODET_DEPENDENCY_ROOT=/absolute/path` with plain `cmake` or `ccmake`. In
`cmake-gui`, use Remove Entry followed by Add Entry for `CODET_DEPENDENCY_ROOT`,
PATH type, the desired absolute value, and ordinary descriptive help. `cmake-gui`
does not accept the CLI `-D` recovery advice. These current instructions supersede
the earlier validation report's wording; historical reports remain unchanged.

Identity disagreements identify the specific receipt or witness being compared,
the mismatching fields, and corresponding private cache fields. The diagnostic
describes disagreement without assuming which side was corrupted. Phrase tests
collapse whitespace, covering CMake's wrapped/indented errors across short and
long paths; separate real ccmake PTY gates still check interaction and record state.
Empty/reset, obsolete metadata, scoped values, explicit overrides, literal-dollar
paths, generation boundaries, and Make/Ninja regeneration remain covered.

Historical extraction now handles cooperative SIGTERM through the existing
interruption scope. Cleanup removes only its owned staging, restores handlers,
and preserves the primary exception if cleanup also fails. Readiness-gated child
tests exercise SIGTERM and Ctrl-C during actual extraction and historical audit,
then verify retry, no unpublished destination, and an untouched sibling owned by
another invocation. The original probe left staging after a supervisor timeout;
the regressions now require cleanup through executed assertions. SIGKILL, host
failure, or actual cleanup errors can still leave staging. There is no automatic
stale-tree sweep; inspect ownership and stop relevant operations before manual
deletion.

Containment and count controls now require dedicated assertion markers identifying
the intended guard or field. An unrelated regex assertion is rejected. Removing
normalization, interruption handling, record specificity, marker enforcement, or
either new measurement-carrier guard fails an executed assertion against a passing
baseline. Skips, import/discovery/compilation errors, launches, and timeouts do not
count. Stronger markers do not imply the archived controls failed for unrelated
reasons.

## Captured validation

All 45 current checks have captured records. The sole unmet required check is
`benchmarks: failed`; both analyzer observations retain their actual exit 1.

| Check | Captured outcome |
| --- | --- |
| Debug / ASan+UBSan CTest | 13/13 each |
| TSan / regular focused runs | Twenty repetitions of 140 cases each |
| Verified Python 3.9.6 | 173 tooling tests, 53 configuration/provenance tests, attempt audits, 138 integration cases |
| Make | Fresh build, eight-workload smoke, unchanged build passed |
| Pinned checks | Lint, Semgrep, runner checks passed |
| Controls | 85 Python/CMake, 63 native/CLI, two production Semgrep passed |
| Host cross-selection | Build and host fixtures passed; not native PPC/Tiger acceptance |
| Static comparison | All 25 translation units matched; no additions, removals, or reviewed relocations |
| Analyzer observations | Each variant: 147 Clang-Tidy, zero Cppcheck, one Clang analyzer finding; exit 1 retained |
| Historical carrier audit | 477 files / 383,683,534 expanded bytes; historical integrity verified, current eligibility false |

CMake 4.4.4 and real ccmake were exercised. A CMake 3.20 runtime and cmake-gui were
unavailable locally. Docker, Podman, and the PPC compiler were unavailable, so local
Ubuntu Unicode runtime and native Tiger acceptance were not exercised. Portable
fixtures do not establish native target acceptance. No older or post-push CI result
is substituted for this frozen source.

## Measurements and retained attempts

Fresh baseline/change Release libraries and identical declared harnesses built.
The final measurement check failed its strict quiet-process gate before its first
sample: PID 98219 was running `python -m unittest discover -s scripts/tests -v`.
The executable identity, argv, and actual failure are retained in the capture log.
That process was not stopped. This establishes ineligibility for this attempt,
not contamination of earlier measurements.

The declared campaign retains one warm-up and five alternating measured runs per
variant, repeated twice, with deterministic work checks. New workloads cover
portable layout resolution and full historical extraction/auditing. Both variants
use the same declared carrier bytes while retaining their own verifier code.
Input envelopes include those carrier hashes only when extraction is selected,
preserving older standalone configurations. Counters distinguish logical completed
work, C++ allocation/output observations, and Python retained/peak traced memory;
retained blocks are not allocation events or total metadata syscalls.

There are **zero accepted samples** here: `measurements.json` is an empty list.
There are no measurement CSVs or computed timing differences to report. No prior
sample is reused and no performance or work-equivalence claim is made. Workload
and producer-link guards passed tests/controls; that is not a Release measurement.

Two interrupted attempts are preserved under their original provenance. The first
froze `dc82d02d94624a72adf0ba8eed1507e7fd50e17c`; it was stopped after a declaration/
producer-input mismatch was identified before publication. Its driver exit is 130,
and its benchmark record remains interrupted with child exit -2. The second froze
`94b7257981872d9d661c777766840f39a4a8cf65`; it was stopped when inspection identified
the need for a shared declared carrier instead of the baseline's separate copy.
Its driver exit is 130, and ASan CTest remains interrupted with child exit -15.
Owned processes were reaped. Missing later outcomes remain absent. Their logs and
artifacts are archived as history, never current acceptance substitutes, and their
partial measurements are not accepted.

## Offline integrity and lossless transport

The maintained incomplete-attempt publisher rechecked fingerprints, commands,
inputs, outcomes, logs, produced identities, and artifacts before exclusive
publication. It does not weaken strict complete-campaign publication.

The [relocated audit receipt](6a57bd36aa6f46a0b8a384af95753a71/offline-audit.json)
records verification under Python 3.9.6 using archived verifier code, with
original-checkout I/O and subprocess launches forbidden. Intentional archived-input
corruption was rejected. Integrity success retains `complete=false` and
`eligible_current=false`.

The [lossless carrier](6a57bd36aa6f46a0b8a384af95753a71/audit-bundle.tar.gz)
contains **633 files / 572,170,090 expanded bytes**.

- Archive bytes: `22837466`.
- Archive SHA-256: `e6b2dceb8e4b3a07af5b2e74014e944e07d3882cbd32483336cbaad60d33018c`.
- Expanded inventory SHA-256: `6c70ddc9651f97e1612bca3fa3007e87f293df7049e1c81407c3ecbeff234cd3`.
- Prefix: `6a57bd36aa6f46a0b8a384af95753a71`.

Every member was decompressed and hash-compared with the original bundle, then
the original inventory was rechecked. The [transport receipt and complete helper
source](6a57bd36aa6f46a0b8a384af95753a71/transport.json) preserve the exact operation.
Reports remain fully expanded inside the carrier; no report compaction occurred.
After checking the archive digest, extract into a fresh directory and audit:

```sh
python3 /fresh/extracted/6a57bd36aa6f46a0b8a384af95753a71/verify-attempt.py \
  /fresh/extracted/6a57bd36aa6f46a0b8a384af95753a71
```

Exit zero establishes integrity only; `benchmarks` must remain unmet.

For a new run, prepare a fresh manifest from the repository-relative template,
select provisioned pinned interpreters explicitly, and establish a new quiet window:

```sh
build/lint-venv/bin/python scripts/evidence_campaign.py prepare --root "$PWD" \
  --folder build/gui-path-cleanup-replay --baseline 555c37c33cad3bf0522d24a6b5787137bb555654 \
  --check-python "$PWD/build/lint-venv/bin/python" \
  --analysis-python "$PWD/build/lint-venv/bin/python" --python39 /path/to/python3.9
build/lint-venv/bin/python scripts/validation_evidence.py capture \
  build/gui-path-cleanup-replay/manifest.json build/gui-path-cleanup-replay/capture
```

Only a fully successful campaign may use strict `bundle` publication. For the
explicitly authorized unavailable-measurement fallback, after all other required
checks pass, use `scripts/evidence_attempt.py` with explicit `--root`, `--campaign`,
fresh `--output`, and an actual `--unavailable-reason`; never relabel the failure.
The exact concrete command manifest is shipped beside the carrier. Auditing does
not rebuild or rerun binaries.

Selection format 3, evidence v5, Tiger provenance 3, sysroot 2, public runtime
interfaces/behavior, and historical artifacts are unchanged. The hashed sysroot
builder is unchanged. The final evidence commit changes only this new namespace;
frozen fingerprints are rechecked after publication. The original checkout remains
preserved. No merge or review-thread modifications are part of delivery.
