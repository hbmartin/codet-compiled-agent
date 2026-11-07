# Preparation ownership and sysroot follow-up

Baseline: `6d5706e24ea25c24bb4bb7121323488cdd294905` (merged #61).
Frozen implementation and producer revision: `3a5892f`.

Preparation cancellation now remains an input outcome: the UI releases its reservation,
collects an already-successful worker, observes completion once, publishes the answer and
requested output file, and remains usable. Move-owned admission tickets release charges
on rejection and exceptional exits. Initial images belong to one submission; local commands
(including compaction) and failed preparation discard them with a nonthrowing warning.

Interactive limits are 65,536 raw bytes, 262,144 normalized expanded bytes per input,
8,388,608 aggregate effective-content bytes, and 100 owned queue entries. Substitution is
sized before construction. Synchronous filesystem lookup has no bounded-latency guarantee.

Application-build admission checks the canonical compiled recipe before allocating a build
directory. Verification and packaging compare archived lock pins/patches with the archived,
digest-pinned manifest. Missing or legacy sysroots give a rebuild command. Environment/default
dependency roots are reevaluated by CMake; explicit and legacy cache entries remain explicit.
Relative roots resolve from the actual invocation cwd, including misleading `PWD` and symlink
cases. Clear a legacy environment-derived cache once with `-UCODET_DEPENDENCY_ROOT`.

## Captured validation

The [immutable v5 attempt](f990e297082043ef8ad4b9c33c8cdc86/README.md) contains the exact manifest, configuration,
logs, source/configuration archives, producing-build identities, controls and historical
attempt references. It has 40 captured outcomes: 37 passed, two known nonzero analyzer
observations, and one required benchmark failure. Nothing relabels the analyzer observations
or the required measurement failure as successful campaign acceptance.

| Check | Outcome |
| --- | --- |
| Fresh Debug CTest | 12/12 groups passed |
| Fresh ASan/UBSan CTest | 12/12 groups passed, including audited per-case isolation |
| TSan focused repetitions | 20 repetitions passed, 124 tests per repetition |
| Ordinary focused repetitions | 20 repetitions passed |
| Verified `/usr/bin/python3` | Python 3.9.6; tooling 131, Tiger/cleanup 25, integration 130 tests passed; attempt-auditor checks passed |
| Pinned lint, Semgrep, runners | Passed |
| Negative controls | 40 native/integration, 40 Python, two production-site Semgrep controls passed |
| Native host cross-selection | Configure, link and selected tests passed; host placeholder archive only |
| Static comparison | All 25 production translation units covered; 147 Clang-Tidy, zero Cppcheck, one Clang Analyzer findings in both variants; 38 source-bound location-only moves; no additions/removals |
| Fresh Release builds/shared harness | Baseline and changed builds, shared harness, and positive controls passed |
| Complete benchmark campaign | **Unavailable**; quiet-environment inspection failed |

The controls include preparation cancellation, filesystem errors, ticket unwinding, effective
budgets, expansion boundaries, image ownership, empty frozen-line cancellation, recipe
admission, root precedence, cleanup exception identity, constructor cleanup, command freezing,
and shared fixture/variant implementation selection. Mutants must fail their intended
assertion; launch, discovery, compilation, timeout or unrelated errors do not establish a pass.

Apple SDK captures use the recorded existing `-Wno-error=implicit-int-float-conversion` and
`-Wno-error=unused-result` accommodations, identically in both Release variants. Compiler,
SDK, configuration and executable identities are retained. Repository lint/analyzer policies
remain unchanged; known analyzer exits stay nonzero observations.

## Measurements and earlier attempts

The final campaign declared identical harness/support snapshots, one warm-up and five
alternating measured runs per variant, repeated in a second set. Its 33 workloads cover line
input, pending presentation, preparation/queueing, exact-bound expansion, diagnostics, exports,
hashing/Git inspection, supervision, build-context inspection (including recipe admission),
Python dependency-root resolution and temporary-resource cleanup.

After seven first-set workloads, Darwin `KERN_PROCARGS2` argv inspection returned `EINVAL`.
The required quiet-environment check failed and the campaign stopped; this does not establish
contamination. **Zero accepted benchmark samples or timing comparisons are attributed to the
final source.** `measurements.json` is an empty list, the required failure remains in the
manifest/capture, and no performance claim or optimization experiment is presented.

Both earlier attempts remain under their original provenance through the historical-attempt
artifact and its archived inputs:

- Capture 01 (`0af8907`) found a benchmark namespace compile error and two new image-ownership
  Tidy findings, then was intentionally interrupted. Those defects were repaired.
- Capture 02 (`a0e7d5a`) recorded passing supported checks and raw samples, but build-context
  imports selected per-variant fixture modules: baseline used one archived input while changed
  used two. Its paired-comparison evidence is not accepted here. No old statuses or samples
  were rewritten. The corrected import order and a well-formed shadow-fixture negative control
  now prove the shared fixture is selected while each variant still supplies its own core.

Counter scope remains explicit: native allocation observations and output/metadata operations;
Python retained traced blocks/bytes and peak traced bytes (not allocation events), command
counts, context resolutions, sysroot snapshots and selected direct hash/Path.stat calls (not
all kernel syscalls or JSON parsing). No full-provenance parsing optimization was attempted.
The resource-cleanup workload times temporary storage cleanup; exception/patch/tracing cleanup
is additionally checked by tests, without claiming an isolated total cleanup latency.

## Offline audit and replay

The [postpublication audit receipt](f990e297082043ef8ad4b9c33c8cdc86-offline-audit.json) records Python 3.9.6, verifier
and inventory identities, the exact guard driver source, relocation, original-checkout I/O
and process-launch prohibition, and an identifying corruption rejection. It is an additional
manual audit, not a replacement captured check or campaign-acceptance claim.

Run the maintained, bundled auditor after moving the run directory:

```sh
/usr/bin/python3 -B /absolute/path/to/moved-run/verify-attempt.py /absolute/path/to/moved-run
```

Exit zero means attempt integrity only. The result must retain `complete: false`,
`eligible_current: false`, `measurement_samples: 0`, and `benchmarks: failed`.

For a new full replay, resolve a fresh exact manifest first (never reuse a managed attempt):

```sh
python3 scripts/evidence_campaign.py prepare --root /absolute/path/to/checkout \
  --folder /absolute/path/to/fresh-campaign --baseline 6d5706e24ea25c24bb4bb7121323488cdd294905 \
  --check-python /absolute/path/to/pinned-python --analysis-python /absolute/path/to/pinned-python \
  --python39 /absolute/path/to/verified-python3.9 \
  --clang-tidy /absolute/path/to/clang-tidy --cppcheck /absolute/path/to/cppcheck
python3 scripts/validation_evidence.py capture /absolute/path/to/fresh-campaign/manifest.json \
  /absolute/path/to/fresh-campaign/capture
```

No Docker, Podman or PPC compiler was available. Ubuntu Unicode, Tiger cross compilation and
native Tiger runtime acceptance are unavailable; host fixtures cannot establish them. The
available CMake was 4.4.4; the resolver uses APIs available at the declared 3.20 minimum, but a
3.20 executable was not available for a separate runtime check. Existing blocking-TTY and
best-effort emergency-output limitations remain unchanged. Public interfaces, Unicode policy,
raw data, storage/journal formats, evidence v5, Tiger provenance 3 and sysroot format 2 remain
unchanged. Historical evidence is preserved, and report compaction remains deferred.
