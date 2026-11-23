# Reliable signal tests and committed evidence publication

**41 of 42 required checks passed; benchmarks are explicitly unavailable.**
The main bundle is an integrity-verified incomplete attempt with
`complete=false`, `eligible_current=false`, and zero accepted samples.

Baseline: `f691bc03dcaf9483de4fae489cd3c1eb3f22353f` (merged PR #70).
Frozen implementation, tests, and producers:
`d440710cb70ba4f624908bb63f8e0be5a7a8fd3d`.
Main capture: [`9bf7f1cc70ea4fd4b0c8feb1ccaecbb6`](9bf7f1cc70ea4fd4b0c8feb1ccaecbb6/manifest.json).
Campaign: `661343d5874d4f0b8b717d7d252ca050`.

## Fixes and reproductions

The controlled signal child now restores `signal.default_int_handler` before
readiness, including an ignored initial disposition. Every subtest owns a separate
parent directory and foreign-owned staging fixture. A monotonic five-second gate
uses sleeps of at most 50 ms and reports explicit deadline expiry. Real subprocess
checks cover a missing signal and an exited parent. The deadline negative control
uses a fake clock and fails an executed assertion, rather than a supervisor timeout.

Cooperative interruption before publication unwinds only owned staging and
preserves the primary exception and handlers. The exclusive rename and committed
receipt are protected together by the existing signal-deferral scope. Signals
during rename admission, successful rename, and cleanup are recorded in the
committed result, which is returned after cleanup. Genuine cleanup failures retain
their exception and carry the committed receipt. Existing destinations are never
replaced. SIGKILL, host failure, and actual cleanup errors can still leave staging;
there is no automatic reclamation.

Configuration identity errors collect disagreements across both generated records,
name each record and its corresponding private cache fields, and join multiple
fields readably. Restore-first GUI guidance shows the recorded previous root.
Strict Configure-only recovery, reset behavior, scoped variables, generated records,
and successful-selection pinning remain unchanged. Real Make/Ninja and ccmake
fixtures retain their wrapping, generation, and recovery assertions.

Layout/extraction workload contracts version 2 distinguish operations from files.
Layout reports zero files and separately observes successful resolutions and
returned locations. Extraction reports its actual expanded files/bytes and its
operation count. These are scoped logical counters, not total metadata syscalls
or allocation events. Historical version-1 observations keep their original
interpretation only in explicit historical auditing.

Campaign configuration now records one shared carrier descriptor with archive and
transport identities. Both variants receive it explicitly; their implementation
source remains variant-specific. Carrier identities are checked around extraction,
the returned archive identity is checked, and sample/configuration/archive links
are verified offline. One selected-workload helper supplies both manifest and
producer declarations. Omitting extraction also omits its measurement inputs;
the independent historical-audit check retains its own inputs.

The captured controls remove SIGINT restoration, the gate deadline, publication
signal deferral, location accumulation, carrier return/sample/archive binding, and
selected declarations independently. Passing baselines and intended executed
assertions are required. Skips, discovery/launch/compilation errors, and timeouts
are not accepted as detected defects. Existing queue, cancellation, image,
approval, original-exception, allocation, and diagnostic controls remain present.

## Captured outcomes

All 44 main check records are retained. The sole unmet required check is
`benchmarks: failed`; both analyzer observations retain their actual exit 1.

| Check | Captured outcome |
| --- | --- |
| Debug / ASan+UBSan CTest | 13/13 each |
| TSan / regular focused runs | Twenty repetitions of 140 cases each |
| Verified Python 3.9.6 | 182 tooling tests, 54 configuration/provenance tests, attempt audits, 138 integration cases |
| Make | Configure, fresh build, eight-workload smoke, and unchanged rebuild passed |
| Pinned checks | Lint, Semgrep, runner checks passed |
| Controls | 93 Python/CMake, 63 native/CLI, two production Semgrep passed |
| Cross-selection | Host build and fixtures passed; no native PPC/Tiger acceptance |
| Analyzer comparison | All 25 translation units matched; no additions/removals |
| Analyzer observations | Each variant: 147 Clang-Tidy, zero Cppcheck, one Clang analyzer finding; exit 1 retained |
| Historical carrier | 477 files / 383,683,534 expanded bytes; historical integrity verified, current eligibility false |
| Release variants | Fresh baseline/change libraries and shared harness built |

CMake 4.4.4 and real ccmake were exercised. CMake 3.20 and cmake-gui coverage was
not exercised locally. Docker, Podman, and the PPC compiler were unavailable in the
captured platform check. Local Ubuntu Unicode runtime and native Tiger acceptance
remain unavailable; host fixtures establish neither. No older CI run substitutes
for changed-source validation.

## Measurement eligibility

Before the first sample, the strict quiet-process check detected external PID
36537 running Python 3.15 `-m unittest discover -s Tests/Scripts`. That process was
left untouched. The selected unavailable-benchmark fallback was used.

No samples, medians, timing differences, speed claims, or unchanged-work claims
are accepted. `measurements.json` is an empty list and the main report's sample
count is integer zero. The original failed check and its diagnostic are preserved.
Counter assertions and unit fixtures are validation evidence, not a completed
measurement campaign.

## Parent-disposition supplement

A separate capture launched the actual signal regression from explicit normal
and ignored SIGINT parents after the measurement attempt. Both checks passed,
and both assert the parent's disposition remains unchanged after the test.
Each executes the signal regression's eight phase/disposition subtests.

Supplement capture:
[`0929f49c3f1648149cb5e3175bc57e21`](0929f49c3f1648149cb5e3175bc57e21/manifest.json),
campaign `972df13e0d63475a99e21d3a6aacd4a0`. Its verified/current result applies
only to its two-check manifest. The main campaign remains incomplete, and the
supplement does not supply measurements.

## Offline-audit scope correction

The [earlier receipt](../gui-recovery-path-cleanup-followup/6a57bd36aa6f46a0b8a384af95753a71/offline-audit.json)
used universal original-checkout-I/O and process-launch prevention flags that
exceeded its guard coverage. Its archived bytes remain unchanged. New receipts
list the guarded APIs and interpreter instead:

- `builtins.open`, `io.open`, `os.open`, `os.stat`, and `os.lstat`.
- `subprocess.Popen`, `run`, `check_output`, `check_call`, and `call`.

The guard implementation is preserved. Cached pathlib metadata accessors,
directory listing, and direct `os` process primitives are outside its coverage.
It is not an OS sandbox. The tests compare receipt claims with the driver's actual
guard loops. Both relocated audits used archived verifier sources under Python
3.9.6 and rejected corrupted archived input; those integrity results are separate
from guard coverage and campaign acceptance.

## Immutable carriers and replay

| Bundle | Expanded files / bytes | Compressed bytes | Inventory SHA-256 |
| --- | --- | --- | --- |
| Main incomplete attempt | 532 / 394,721,847 | 19,061,681 | `beada0fedaf0745d6f3c26fbeffddb84b9ed01d557496a04a259a4346c498546` |
| Targeted signal checks | 328 / 6,959,421 | 1,566,261 | `a36819b2939a41e31126abbf24f42770d2aacf151eb2fff6eef210abc4973cbf` |

Every expanded member was decompressed and digest-compared, and the original
inventory was rechecked. Reports, source snapshots, configs, logs, and samples
remain expanded byte-for-byte inside each carrier; no report compaction was used.
Archive digests, embedded transport driver source, and exact argv are in each
`transport.json`. Both destinations use exclusive publication.

From the repository root, extract into a fresh directory and audit using the
frozen verifier without the original checkout:

```sh
mkdir /tmp/codet-publication-audit
base=docs/validation/historical-publication-evidence-followup
tar -xzf "$base/9bf7f1cc70ea4fd4b0c8feb1ccaecbb6/audit-bundle.tar.gz" -C /tmp/codet-publication-audit
tar -xzf "$base/0929f49c3f1648149cb5e3175bc57e21/audit-bundle.tar.gz" -C /tmp/codet-publication-audit
main=/tmp/codet-publication-audit/9bf7f1cc70ea4fd4b0c8feb1ccaecbb6
signal=/tmp/codet-publication-audit/0929f49c3f1648149cb5e3175bc57e21
python3 "$main/verify-attempt.py" "$main"
PYTHONPATH="$main/verifier_tooling" python3 -c 'import json,sys; from pathlib import Path; from evidence_bundle import verify_bundle; print(json.dumps(verify_bundle(Path(sys.argv[1]))))' "$signal"
```

Use the maintained capture/replay documentation for rerunning commands and builds.
Offline verification checks recorded integrity and provenance; it does not rebuild
or rerun the recorded binaries. Public runtime interfaces, Unicode/input behavior,
storage formats, evidence v5, selection format 3, Tiger provenance 3, and sysroot 2
are preserved. The hashed sysroot builder and historical reports remain unchanged.
