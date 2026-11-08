# Configuration witnesses and live benchmark acceptance

The complete v5 campaign is [0f99473487bc486c93c7f661cf9b34aa](0f99473487bc486c93c7f661cf9b34aa/manifest.json).
All **42 required checks passed**. The two analyzer observation checks retain
actual exit 1; their findings are unchanged and are not relabeled as passing.

Baseline: `bac5b342151a255fa6ef8faf8406a0cdea990640`.
Frozen implementation, tests and producers: `c65fcddd8d6aa2fa51a83287a7e6e7650ed88c98`.
Campaign ID: `7c42cb97b92742c189c42cc1af7056b2`.
The later evidence commit adds documentation only; production and validation
fingerprints are rechecked against the frozen capture.

## Changes and reproductions

- Owned cache metadata is validated independently of its public help marker.
  Bad/missing private tokens, relative values and invalid origins fail instead
  of becoming explicit permanent pins. Real current-format overrides can replace
  damaged records; obsolete owned records require reset and reapplication.
- Internal selection format 3 binds a generated receipt to an independent success
  witness and its digest. Missing either artifact alone stops reuse. Both absent
  with coherent provisional metadata permits failed-first-configure recovery;
  deleting both artifacts cannot establish a previous commitment. Scoped normal
  values, explicit overrides, real Ninja/Make regeneration and literal dollar
  paths retain their tested behavior.
- The CLI and ordinary host benchmark call the same production preparation
  facade. Normal builds no longer slice CLI source. Historical comparison
  adapters remain checked, dependency-light and atomically replaced.
- Live preparation measurements require integer version 2, a known consistent
  role, 256 changed advisory checks, zero changed saturated payload allocations,
  and one wrapper per accepted changed CLI preparation. Historical interpretation
  is explicit and cannot satisfy current campaign acceptance.
- Borrowed raw input rejects temporary strings at compile time. Both expansion
  guards and guarded self-move remain intact; public runtime interfaces, limits,
  Unicode policies and storage formats are unchanged.

Reproduction coverage includes `DependencyReconfiguration` corruption, missing
success-artifact, digest, override/reset and wrapped legacy-help cases;
`PreparationRecovery` changed-excerpt, interrupted-write, metric downgrade,
role and counter cases; and `InputPreparation` facade, lifetime and ownership
cases. Every native/integration mutation site is also checked by the normal
Python suite before expensive controls run. Compile-time lifetime assertions are
regular build checks, not claimed as executed negative-control failures.

The captured controls require passing unmodified baselines and intended executed
assertions. There are **63 Python/CMake controls**, **61 native/CLI controls** and
**2 production Semgrep controls**. Skips, discovery errors, compilation errors,
launch failures and subprocess timeouts do not count as detection.

## Captured validation

| Check | Outcome |
| --- | --- |
| Debug CTest | 13/13 passed |
| ASan/UBSan CTest | 13/13 passed, Darwin unit cases isolated |
| Focused input/diagnostic repetitions | 20 passed |
| TSan focused repetitions | 20 passed |
| Real Make benchmark build, smoke and unchanged build | Passed |
| Python 3.9 tooling, attempt auditing, Tiger fixtures and CLI integration | Passed; interpreter verified as 3.9.6 |
| Pinned lint, Semgrep, runners and host cross-selection | Passed |
| Analyzer canaries and complete source comparison | Passed for all 25 translation units |
| Clang-Tidy observations | 147 findings in each variant; exit 1 retained |
| Cppcheck observations | Zero findings in each variant |
| Clang analyzer observations | One finding in each variant; exit 1 retained |
| Added/removed findings or reviewed relocations | None |
| Fresh baseline/change Release measurements | Passed |
| Relocated offline audit and deliberate corruption rejection | Passed under Python 3.9.6 |

The first full attempt, capture `c3430adb5d3944868dd58d0158e0ea2a`, failed when
an integration mutation still targeted the old lambda location; its benchmark
check was correctly blocked. A subsequent development probe also showed that
removing a redundant routing guard did not remove control ownership. The control
now mutates actual ownership at handoff. The first attempt's original manifest,
records, logs and produced artifacts are retained by `historical-attempt` under
their own provenance. They do not substitute for any result above. The prepared
second folder was never captured and has no reconstructed outcomes.

CMake 4.4.4 was exercised; CMake 3.20 runtime was not exercised. Docker, Podman
and the PPC compiler were unavailable. Ubuntu Unicode runtime and native Tiger
acceptance are unavailable locally. Passing host fixtures and host cross-selection
are not native PPC/Tiger acceptance. The hashed sysroot builder is unchanged.

## Measurements

The accepted campaign contains **1,008 raw samples**, including **840 measured
samples across 42 workloads**. Each variant has one warm-up and five alternating
measured runs, repeated in two sets. Both variants use an identical declared
harness/support snapshot and matching compiler settings against verified source
libraries and headers. Executable, library, compiler, configuration, harness and
link identities and quiet-window inspection proofs are retained.

All positive controls and deterministic changed-work assertions passed. There
are no timing-based CI thresholds. Measured work differs for some tooling
workloads; the data records those differences. No general speed improvement or
work-equivalence claim is made.

Observed changes in median latency (changed relative to baseline):

| Workload | Set 1 | Set 2 |
| --- | ---: | ---: |
| Advisory-check control | +2.06% | +0.84% |
| Actual CLI preparation | -1.76% | +5.50% |
| Compatibility preparation | -1.25% | +0.82% |
| Configuration selection | +0.39% | -4.65% |

All workloads, increases, decreases and scoped work counters remain in the
[raw samples](0f99473487bc486c93c7f661cf9b34aa/samples.csv) and
[recomputed medians](0f99473487bc486c93c7f661cf9b34aa/medians.csv).
The expanded audited `artifacts/benchmarks/measure.json` is authoritative for
sample counts, summaries, comparisons, invocations and provenance. Allocation
counters and requested bytes are scoped to the native harness. Python retained
traced blocks/bytes and peak traced bytes are not total allocation events;
advisory callback counts are not physical mutex or kernel-operation counts.
No consolidation optimization was attempted or revived.

## Audit and lossless transport

The [carrier](0f99473487bc486c93c7f661cf9b34aa/audit-bundle.tar.gz) retains all
577 expanded files and 415,494,135 bytes without report compaction. Its SHA-256 is
`d6e14aff33fe9975496213b4037e09363fe5c463d4e1662bb84e26b319351d35`.
Every decompressed member's SHA-256 matched the published inventory, which was
rechecked before and after packing. No single committed file reaches GitHub's size cap.
The [transport receipt](0f99473487bc486c93c7f661cf9b34aa/transport.json) embeds
its exact standalone driver and arguments; it does not grant campaign acceptance.

The [offline receipt](0f99473487bc486c93c7f661cf9b34aa/offline-audit.json) records
44 check records and 14,603 audit-input declarations, `verified=true` and
`eligible_current=true`. It relocated the bundle, imported only archived verifier
tooling, forbade original-checkout I/O and process launches, and rejected deliberate
archive corruption. It audits evidence integrity and acceptance requirements;
it does not rerun binaries or establish unavailable platform coverage. The full
source snapshots, configuration, build metadata, logs, samples and source-bound
producer envelopes are inside the carrier. Original paths are opaque provenance.
Historical bundles retain their archived verifiers and original interpretation.

Extract into a fresh directory:

```sh
tar -xzf audit-bundle.tar.gz
```

The extracted directory is named `0f99473487bc486c93c7f661cf9b34aa`.
The offline receipt embeds `driver_source` and its argument interface for repeating
the relocated audit with Python 3.9+. Its original-root arguments are denied-prefix
strings and do not require those checkouts to exist.

For a new capture, use explicit root, fresh output paths and installed pinned
interpreters/tools. This creates new provenance; it does not overwrite this run:

```sh
cd "$ROOT"
"$PYTHON" scripts/evidence_campaign.py prepare --root "$ROOT" --folder "$RUN" \
  --baseline bac5b342151a255fa6ef8faf8406a0cdea990640 \
  --check-python "$PYTHON" --analysis-python "$ANALYSIS_PYTHON" \
  --python39 "$PYTHON39" --clang-tidy "$CLANG_TIDY" --cppcheck "$CPPCHECK"
"$PYTHON" scripts/validation_evidence.py capture "$RUN/manifest.json" "$RUN/capture"
"$PYTHON" scripts/validation_evidence.py bundle "$RUN/manifest.json" "$RUN/capture" \
  --output "$BUNDLE"
```

The exact 44-check captured manifest, including optional historical-attempt
retention, is supplied alongside the carrier. Fresh portable replay omits that
local-history option unless an original attempt is explicitly supplied.
