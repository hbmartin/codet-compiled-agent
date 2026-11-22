# Strict GUI recovery and reliable evidence guards

**Tests and controls passed; benchmarks are unavailable.** This is an integrity-verified
incomplete attempt with zero accepted measurement samples and current eligibility false.

This follow-up addresses the seven confirmed PR #68 findings. It deliberately
keeps strict dependency-selection recovery: Configure-only provisional metadata
cannot distinguish a GUI edit from private-state corruption.

## Configuration behavior and recovery

The real ccmake regression uses Configure, quits without Generate, edits the
public PATH value, and configures again. No success records exist, so rejection
is intentional. The error now explains this boundary and the supported recovery:

1. After a successful initial Configure, run Generate before editing the derived
   root. ccmake may require another Configure to enable `g`.
2. If Generate is unavailable, relaunch cmake-gui or ccmake with
   `-DCODET_DEPENDENCY_ROOT=/absolute/dependency/path`.
3. In cmake-gui, an alternative is Remove Entry followed by Add Entry for
   `CODET_DEPENDENCY_ROOT`, type PATH, the desired absolute value, and ordinary
   descriptive help. Do not edit private metadata.

Empty public values and `-U` reset before obsolete-version validation, including
obsolete empty markers. Nonempty obsolete owned records still require reset;
there is no migration. This corrects the broad claim about obsolete empty markers
in the earlier documentation. Historical reports remain unchanged.

An absolute private VALUE disagreement now identifies `CMakeCache.txt`, the
relevant private identity fields, and both generated records. It does not label
the unchanged receipt malformed. Existing precedence, scoped variables, pinning,
Make/Ninja regeneration, literal-dollar handling, later-generation-error behavior,
and the documented both-records-missing recovery boundary remain in place.

## Evidence guards

Historical sample counters accumulate completed audits and verified row counts.
The expected 32 audits / 768 rows remain independent work checks. A one-iteration
mutation is rejected through executed assertions. The earlier unmodified producer
did perform 32 iterations; this follow-up does not claim its measurements skipped
work or reinterpret archived samples.

Adapters compare and write exact UTF-8/LF bytes, then hash and validate the actual
completed file. CRLF and lone-CR outputs are repaired. Atomic replacement,
permissions, process umask, and unchanged-output timestamps remain covered.

Replay build specifications are relative, contained, disjoint paths. Duplicate,
normalized-alias, parent-traversing, absolute, overlapping, reserved-workspace,
and symlink-escape paths fail before tool probes or campaign creation. One layout
resolver supplies both validation and the recorded `build_specs`/resolved `builds`.
Valid nested paths with spaces and the legacy Make default remain supported.

Historical carrier metadata requires positive integer counts, excluding booleans.
Extraction, inventory validation, strict rejection, and complete historical audit
run in sibling staging. Exclusive atomic publication refuses existing destinations;
failures and interruptions release staging and preserve the original exception.
Tests cover failure after an earlier member, retries, verification failure, and
a destination created during publication. Integrity-only historical auditing
continues to report `eligible_current=false`.

Internal selection format 3, evidence v5, Tiger provenance format 3, sysroot
format 2, and public runtime behavior/interfaces are unchanged. The hashed sysroot
builder is unchanged. No optimization experiment or timing threshold is added.

## Captured validation and measurements

- Baseline: `1926b14fecd26f7d1310906284eca308b1ded68b`.
- Frozen implementation/tests/producers: `b1bc22695f7852f4324fe0a0296c16a542e607d5`.
- Capture: [`d6a86ec158ab4b95aada34c9e591217a`](d6a86ec158ab4b95aada34c9e591217a/manifest.json); campaign `241464c9df1947e5a2f8a429f2125911`.
- **42 of 43 required checks passed.** Benchmarking failed its strict quiet gate.
  Both analyzer observations retain actual exit 1. There are 45 current check records.

| Validation | Captured result |
| --- | --- |
| Debug / ASan+UBSan CTest | 13/13 each |
| TSan / regular focused runs | 20 repetitions of 140 cases each |
| Verified Python 3.9.6 | 166 tooling tests, 51 configuration/provenance tests, attempt audits, all 138 integration cases |
| Make | Fresh build, eight-workload smoke, unchanged build passed |
| Pinned checks | Lint, Semgrep, runner checks passed |
| Controls | 79 Python/CMake, 63 native/CLI, two production Semgrep passed |
| Host cross-selection | Build and host tests passed; not PPC/native Tiger acceptance |
| Static comparison | All 25 translation units matched, no added/removed findings or relocations |
| Analyzer observations | Each variant: 147 Clang-Tidy, zero Cppcheck, one Clang analyzer finding; exit 1 retained |
| Historical carrier | 477 files / 383,683,534 bytes fully audited, current eligibility false |

Two earlier attempts are retained as historical inputs, not acceptance substitutes.
`final-campaign-01` observed a cancellation fixture reading the output file after
streamed text but before final publication; ASan CTest exited 8. The capture was
interrupted during TSan (recorded child exit -15 / driver exit 130). A test-only,
first-turn publication gate and acknowledged release now distinguish streaming
from the atomic output-file publication; a native mutant fails its executed
assertion. The partial attempt's missing outcomes are not reconstructed.

`final-campaign-02` completed but failed the Python adapter-rewrite control: the
new integrity guard raised `ValueError`, which the existing regression classified
as an error instead of its intended assertion. Benchmarking was blocked by that
failed prerequisite. The narrow assertion repair then passed all 79 controls
before the final source freeze. Both failed attempts retain their original
fingerprints, outcomes, and logs in the captured history artifact. No earlier
result or sample is relabeled as current success.


**Benchmarks are unavailable; no samples or timings are accepted.** The fresh
baseline/change Release libraries and shared declared harnesses compiled, but the
strict process check detected PID 17045 running `python -m unittest discover -s
scripts/tests -v` before the first measured workload. The actual identity and argv
are retained in `capture/benchmarks.log` inside the carrier. This establishes
ineligibility for this attempt, not contamination of any prior measurements.

The selected fallback preserves that failure: `complete=false`,
`eligible_current=false`, integer `measurement_samples=0`, and `measurements.json`
is an empty list. The sole unmet required check is `benchmarks: failed`. No old
samples are reused, no medians or measurement CSVs are invented, and no latency,
allocation, work-equivalence, or general performance claim is made. The 32/768
historical work counters are covered by tests and mutation controls, not presented
as accepted Release measurements here.


## Auditing, replay, and limitations

The maintained incomplete-attempt publisher revalidated the frozen production and
validation fingerprints, check records, declarations, input/output identities,
logs, and derived artifacts before immutable publication. It does not relax the
strict complete-campaign publisher.

The expanded attempt passed relocated offline integrity auditing under Python
3.9.6 using only archived verifier code, with original-checkout I/O and subprocess
launches forbidden. Intentional archived-input corruption was rejected. See the
[audit receipt](d6a86ec158ab4b95aada34c9e591217a/offline-audit.json). Successful integrity auditing does
not grant complete campaign acceptance.

The [lossless carrier](d6a86ec158ab4b95aada34c9e591217a/audit-bundle.tar.gz) preserves all
**649 files / 573,023,850 expanded bytes**.
Every member was decompressed and hash-compared with the original complete bundle,
then its source inventory was rechecked. There is no report compaction.

- Compressed bytes: `23252349`.
- Archive SHA-256: `976042ea5520692ff1f6a92f02cd8efd326d47d640405a700c282452200331bd`.
- Expanded inventory SHA-256: `5db43202b93853500d9062b8c1fb7fe25c18b8b4b6ed1b12614fa6e1edd07807`.
- Extraction prefix: `d6a86ec158ab4b95aada34c9e591217a`.
- [Transport receipt and complete helper source](d6a86ec158ab4b95aada34c9e591217a/transport.json).

Verify the archive digest above before extracting into a fresh directory. Audit
the extracted attempt with its own verifier:

```sh
python3 /fresh/extracted/d6a86ec158ab4b95aada34c9e591217a/verify-attempt.py \
  /fresh/extracted/d6a86ec158ab4b95aada34c9e591217a
```

Exit zero here means integrity verification only; the audit must continue to
report `eligible_current=false` and `benchmarks` as unmet.


The exact concrete command manifest is shipped beside the lossless carrier.
For a new checkout, prepare a fresh manifest from the repository-relative replay
template; override interpreter/tool paths explicitly as needed:

```sh
python3 scripts/evidence_campaign.py prepare --root "$PWD" \
  --folder build/gui-recovery-replay --baseline 1926b14fecd26f7d1310906284eca308b1ded68b \
  --check-python "$PWD/build/lint-venv/bin/python" \
  --analysis-python "$PWD/build/lint-venv/bin/python" --python39 /path/to/python3.9
python3 scripts/validation_evidence.py capture \
  build/gui-recovery-replay/manifest.json build/gui-recovery-replay/capture
python3 scripts/validation_evidence.py bundle \
  build/gui-recovery-replay/manifest.json build/gui-recovery-replay/capture \
  --output build/gui-recovery-replay-published
python3 scripts/validation_evidence.py verify-bundle build/gui-recovery-replay-published
```

Prepared campaigns are single-attempt. Choose fresh folders; retain failures under
their original provenance. Use the pinned Make lint/Semgrep targets, existing
provisioned tools, and a verified Python 3.9 interpreter. Independent replays must
establish their own quiet measurement window. Auditing a bundle does not rebuild
or rerun binaries. Use `verify-bundle --historical` only for historical integrity;
that option never grants current campaign acceptance.

CMake 4.4.4 and real ccmake were exercised. A CMake 3.20 runtime and cmake-gui were
not available here. Docker, Podman, and the PPC compiler were unavailable, so local
Ubuntu Unicode runtime and native Tiger acceptance were not exercised. Portable
host fixtures and host cross-selection do not establish target acceptance. No
older CI result substitutes for this frozen source and no post-push CI is claimed.

For the selected unavailable-benchmark fallback, only after every other required
check passes, publish an explicitly incomplete attempt instead of calling `bundle`:

```sh
python3 scripts/evidence_attempt.py --root "$PWD" \
  --campaign build/gui-recovery-replay --output build/gui-recovery-replay-attempt \
  --unavailable-reason "Strict quiet-process inspection blocked measurement; see captured log"
python3 scripts/evidence_attempt_audit.py build/gui-recovery-replay-attempt
```

This does not turn a failed measurement gate into acceptance. The output destination
must be fresh. The final evidence commit adds only this new report and carrier;
the frozen source fingerprints remain unchanged. Historical documentation,
reports, manifests, archives, and samples are preserved.
