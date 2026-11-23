# Committed audit receipts and interruption-aware measurements

The implementation fixes publication-exit receipt loss, canonicalizes offline
guard roots, installs the existing selected guards before verifier imports, and
rejects interrupted extraction measurements. Public runtime behavior, selection
format 3, evidence v5, Tiger provenance 3, sysroot 2, and the hashed sysroot builder
are unchanged. Historical reports, manifests, samples, receipts, and blobs remain
unchanged.

## Source and acceptance

- Fresh baseline: `8fb0bed7107baa86db747055262018ef0385ddf3`.
- Frozen implementation/tests/producers: `fb21c97728604ee2ce8e5f5918aa4dd7158ae6ea`.
- Main capture: `d551c60d7ee841cb93a1da346e724960`; campaign
  `2ce2089688514589a1f731e4d00fee1c`.
- The 131 production and 156 validation fingerprint entries still match the
  frozen source after publication. The original checkout is preserved.

**41 of 42 required checks passed. Benchmarks are unavailable.** The strict
quiet-process check detected external PID 97649 running Python 3.15
`-m unittest discover -s scripts/tests -p test_faster_verification.py` before
exports in the first set. That process was left untouched. The incomplete
candidate measurements cannot satisfy the declared two-set campaign; the main
bundle contains zero accepted samples and an empty `measurements.json`.
No timings, medians, differences, or performance/equivalence claims are published.
The original failed check, exact command and log remain in the audit bundle.

The main bundle is explicitly `complete=false`, `eligible_current=false`.
Successful offline integrity auditing does not turn it into a passing campaign.
The two analyzer observations retain their actual nonzero exits rather than
being relabeled successful checks.

## Captured validation

| Check | Actual result |
| --- | --- |
| Debug CTest | 13/13 passed |
| ASan/UBSan CTest | 13/13 passed |
| ThreadSanitizer | Twenty repetitions of 140 focused cases passed |
| Regular focused tests | Twenty repetitions of 140 cases passed |
| Verified Python 3.9.6 tooling | 193 tests passed |
| Python 3.9 configuration/provenance | 54 tests passed |
| Python 3.9 attempt auditor and CLI integration | Passed; integration 138 cases |
| Make build, preparation smoke, unchanged rebuild | Passed |
| Pinned lint, Semgrep and runner checks | Passed |
| Host cross-selection configure/build/tests | Passed |
| Python/CMake controls | 102 passing baselines and intended assertion failures |
| Native/CLI controls | 63 passing baselines and intended assertion failures |
| Production Semgrep controls | Two passed |
| Fresh Release variants and declared shared harness | Built and verified |
| Required benchmarks | Failed strict quiet check; no eligible campaign samples |
| Historical carrier audit | 477 files / 383,683,534 expanded bytes verified |
| Metadata and platform checks | Captured |

All 25 analyzer translation units have complete matching findings: each variant
has 147 Clang-Tidy findings, zero Cppcheck findings, and one Clang analyzer finding.
Both analysis commands exited 1. The comparison found no additions or removals;
no location-relocation exceptions were needed.

Installed CMake 4.4.4, ccmake, Make and Ninja coverage ran. CMake 3.20 and
cmake-gui were not exercised. Docker, Podman, and the PPC compiler were unavailable.
No local Ubuntu runtime or native Tiger acceptance is claimed; host fixtures and
host cross-selection do not establish target acceptance. No older CI run is
substituted for this source.

## Reproductions and controls

Publication owns its committed receipt through cleanup and scope teardown.
Readiness-gated SIGINT/SIGTERM tests cover rename, publication and cleanup; new
scope-exit and handler-restoration tests retain the receipt after deferred
handlers end. Pre-publication failures still unwind owned staging. Genuine
post-publication cleanup errors preserve exception identity and carry the receipt.
SIGKILL and host failure can leave staging; no reclamation sweep was added.

Committed interruptions remain successful publication observations. The benchmark
consumer rejects any such observation before emitting a sample and prints the
committed receipt in failure diagnostics. It does not convert outer supervisor
timeouts to success. The control removing observation rejection fails through the
intended executed assertion.

Offline roots are canonicalized before relocation and import filtering. Guard
resolution uses saved native primitives to avoid recursive patched metadata calls.
Relative roots, symlink aliases, import-time captured file calls, restoration after
import failure, and valid/expected incomplete dispositions have regressions.
The driver accepts the underlying auditor's validated result; optional
`--expect-unmet` checks an explicit JSON-map expectation without widening the
auditor's permitted failure policies or current eligibility.

Live tooling rows require integer metric version 2. Unversioned observations
remain historical-only. Extraction workload contract 3 measures the complete
verified operation, including carrier admission, extraction/audit, final carrier
verification, and owned cleanup. Its observed three archive-hash passes and streamed
bytes are checked separately from expanded files/bytes, and the archive size is
bound to the archived input. Historical workload contracts 1/2 retain their original
interpretation only in explicit historical mode. These are scoped work counters,
not total allocation-event or kernel-metadata-syscall counts.

Signal fixtures independently clean an owned child even when the intermediate
parent has exited. They verify the unique argv/group before signalling, retain
bounded escalation, and stop release gates explicitly on EOF instead of spinning.
The five-second monotonic gate remains. New negative controls also disable
publication recovery, canonicalization, pre-import guarding, generic disposition
handling, live version enforcement, orphan cleanup, EOF handling, and observed
archive-byte counting. Skips, errors, launch/compilation failures, and timeouts
do not count as detected mutants.

The CMake per-record disagreement reset was already present and remains unchanged.
The earlier shared-verifier replay example used identical recorded identities for
all 44 Python verifier sources; it was not a cross-attribution defect. The current
main and targeted bundles likewise contain the same frozen verifier identities.

## Separate inherited-signal checks

Capture `17979ad3cafb441389a9f8d20d771175` runs the real-signal regression under
both default and genuinely ignored parent SIGINT dispositions, asserts that the
parent disposition is preserved, and passed both checks. Each check executes the
eight phase/signal child subtests. Its `eligible_current=true` applies only to
that two-check manifest; it is not a substitute for main campaign acceptance.
It ran after the main benchmark attempt to avoid measurement interference.

## Relocated auditing and lossless transport

Both post-publication audits used Python 3.9.6 and their own archived verifier
sources after relocation, passed integrity verification, and rejected corrupted
archived input. New receipts enumerate the canonical original roots, interpreter,
selected APIs, and guarded phase (verifier import through corruption control).
Previously cached aliases, cached pathlib accessors, directory listing, and direct
OS process primitives remain outside coverage. This is not an OS sandbox and
does not claim to block every original-checkout access or process-launch mechanism.
Historical receipts are not rewritten.

Every expanded member was decompressed and digest-compared before exclusive
carrier publication. Reports remain expanded byte-for-byte inside the archives;
no report compaction or outcome reconstruction was used. Each `transport.json`
contains the exact argv, complete transport helper source, previews inventory,
and round-trip results.

| Bundle | Expanded files / bytes | Compressed bytes | Inventory SHA-256 |
| --- | --- | --- | --- |
| Main incomplete attempt | 531 / 394,783,096 | 19,048,831 | `69eb5d6a90cba67d0e60f614ed03d50682b7abe62c02079b1239af491f61e1b1` |
| Targeted inherited-signal checks | 328 / 6,984,221 | 1,570,778 | `4622d46f8471005550ce035726b0a24fffb3142666a0394c4b2880c8483ae7f1` |

Extract both carriers into a fresh directory. The main archived verifier can
audit the supplement because their recorded verifier-source identities match;
each verification still checks the selected bundle's own inventory and provenance.
The supplement's standalone scripts are retained in digest-keyed archived inputs.

```sh
mkdir /tmp/codet-historical-audit
base=docs/validation/historical-audit-interruption-followup
tar -xzf "$base/d551c60d7ee841cb93a1da346e724960/audit-bundle.tar.gz" -C /tmp/codet-historical-audit
tar -xzf "$base/17979ad3cafb441389a9f8d20d771175/audit-bundle.tar.gz" -C /tmp/codet-historical-audit
main=/tmp/codet-historical-audit/d551c60d7ee841cb93a1da346e724960
signal=/tmp/codet-historical-audit/17979ad3cafb441389a9f8d20d771175
python3 "$main/verify-attempt.py" "$main"
PYTHONPATH="$main/verifier_tooling" python3 -c 'import json,sys; from pathlib import Path; from evidence_bundle import verify_bundle; print(json.dumps(verify_bundle(Path(sys.argv[1]))))' "$signal"
```

Offline auditing checks recorded integrity and provenance. It does not rebuild or
rerun recorded binaries, restore measurement eligibility, or establish native
platform acceptance.
