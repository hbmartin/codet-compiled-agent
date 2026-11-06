# Interactive command and package integrity follow-up

Baseline: `4d787a890c7ab06d6073d7ca024a38aaf60281ff` (merged PR #60).
Frozen implementation, tests and producer commit: `bb7560f`.
The isolated worktree preserves the original checkout's unrelated state.

Interactive commands resolve once at Enter before `UserPromptSubmit`. Unknown
slash names and paths remain prose; existing unsafe/unreadable commands fail
locally without a prompt hook, provider turn, or loss of later queue entries.
Hooks retain original input while persistence/provider input uses the frozen
expansion. Initial, idle, active and completed paths share preparation; headless
behavior remains strict. Inactive compatibility submissions become follow-ups.

TLS recognizes actual Rustls and BearSSL spellings with delimiter-aware matching.
Dependency roots use nonempty explicit/cache paths, then nonempty environment,
then `.deps`. Recipe-manifest format 2 contains compiled dependency metadata;
distribution-only metadata is archived separately. New Tiger build provenance
format 3 pins immutable manifest bytes and requires a fresh compatible build.
Both package variants preserve their admitted build, audit bytes and copied-input
checks. No build reuse or pruning is added.

Benchmark startup cleanup and incomplete-attempt auditing are exception safe and
Python 3.9 compatible. Incomplete attempts require an empty measurement list and
integer zero count; they remain ineligible for campaign acceptance. Inventory
errors identify missing, extra and mismatched files. Unexpected Finder/bytecode
files are rejected. The optional full-record parsing optimization is unretained.

## Captured validation

The [immutable v5 audit bundle](e502981668834de6b65125a42142611a/report.json) retains 41 actual command outcomes:
39 passed and two analyzer invocations exited 1 with known findings under the
explicit observation policy. Strict source-bound comparison passed for all 25
sources: Clang-Tidy 147/147, Clang analyzer 1/1, Cppcheck 0/0; 12 reviewed
location-only mappings, zero added findings. These nonzero analyzer observations
are not relabeled as passing analyzer checks.

- Fresh Debug CTest: 12/12; fresh ASan/UBSan CTest: 12/12.
- TSan and unsanitized focused input/diagnostic/lifecycle coverage: 20 repetitions
  of 116 tests each, all passed.
- Verified `/usr/bin/python3` 3.9.6: tooling 131 tests, attempt-auditor compatibility,
  Tiger recovery 15 tests, and integration 126 tests passed.
- Pinned lint, Semgrep, runner tests, and host cross-selection build/tests passed.
- 35 Python, 32 native/integration, and two production-site Semgrep negative
  controls passed; mutations failed through intended assertions after passing baselines.
- Prior failed/interrupted capture and analyzer preflights remain explanatory
  history with original outcomes, digests and derived artifacts.

The triage and gated regressions cover unknown paths/names at initial/idle/active
input, original hook input, command creation/removal/modification after Enter,
frozen expansion, unsafe-command isolation, inactive submissions, dependency-root
precedence through real CMake, recipe cache identity, transient package prefixes,
immutable nested metadata, startup failure cleanup, and incomplete-audit tampering.

## Measurements and scope

The fresh baseline/change Release campaign completed with one warm-up and five
alternating measured runs per variant, repeated in two sets after source-bound
quiet checks. It contains 696 samples (580 measured, 116 warm-up), 29 workloads,
identical declared harness snapshots, compiler/configuration identities, link
commands, executable/library digests, raw samples, medians, and positive controls.
[Raw samples](e502981668834de6b65125a42142611a/measurements/benchmarks/measure.json/samples.csv) and
[all medians](e502981668834de6b65125a42142611a/measurements/benchmarks/measure.json/medians.csv) are preserved.

Dedicated interactive resolution/queue admission, TLS-classification and attempt-
auditor latency are not isolated in these shared harnesses; their correctness is
covered by the captured tests. No performance claim is made for those paths.
The measured presentation harness includes the pending-submission path; tooling
includes large build-context inspection. Allocation metrics for Python remain
retained traced memory/block observations rather than allocation-event totals.
Native operation counters and metadata counters keep their declared scopes.

`all_measured_work_equal` is false: build-context inspection performs 20 versus
19 direct file-hash calls and 518 versus 514 profiled `Path.stat` calls, while
traced-memory observations also differ in Git/hashing work. Do not infer global
work equivalence or total kernel syscall/allocation counts. Some presentation
medians rise in both sets; all differences are published below without a timing
threshold, speed claim, or a retained performance optimization. The separate
hash experiment's latency gate is false. Historical measurements are unchanged.

| Workload | Set 1 baseline / changed ms | Set 1 change | Set 2 baseline / changed ms | Set 2 change |
| --- | ---: | ---: | ---: | ---: |
| export/large | 296.405833 / 286.812667 | -3.24% | 329.809541 / 332.954791 | +0.95% |
| export/small | 60.212792 / 59.949625 | -0.44% | 73.158834 / 65.742583 | -10.14% |
| line/allocation-positive | 0.002375 / 0.002375 | +0.00% | 0.002333 / 0.002292 | -1.76% |
| line/busy | 0.138541 / 0.142875 | +3.13% | 0.146541 / 0.139083 | -5.09% |
| line/deferred-tail | 0.331417 / 0.376458 | +13.59% | 0.321167 / 0.374958 | +16.75% |
| line/deletion | 0.100125 / 0.105541 | +5.41% | 0.098250 / 0.097792 | -0.47% |
| line/editing | 2.907250 / 2.993709 | +2.97% | 2.824291 / 2.826958 | +0.09% |
| line/editing-basic | 2.709208 / 2.857334 | +5.47% | 2.595208 / 2.743958 | +5.73% |
| line/geometry-positive | 0.212000 / 0.209291 | -1.28% | 0.211875 / 0.209667 | -1.04% |
| line/native-boundary | 0.801250 / 0.796208 | -0.63% | 1.040792 / 0.971750 | -6.63% |
| line/native-busy | 0.775042 / 0.885917 | +14.31% | 0.937958 / 0.969708 | +3.39% |
| line/native-editing | 22.609542 / 22.594167 | -0.07% | 22.934208 / 22.281083 | -2.85% |
| line/native-flush-boundary | 0.798208 / 0.776166 | -2.76% | 0.894584 / 0.829458 | -7.28% |
| line/native-pending-fragment | 1.973708 / 2.072208 | +4.99% | 2.273667 / 2.219959 | -2.36% |
| line/native-pending-submission | 1.151042 / 1.163375 | +1.07% | 1.880375 / 2.190041 | +16.47% |
| line/native-unchanged | 0.201416 / 0.196875 | -2.25% | 0.193917 / 0.190917 | -1.55% |
| line/pending-submission | 0.350500 / 0.408791 | +16.63% | 0.383917 / 0.404291 | +5.31% |
| line/unchanged | 0.124958 / 0.119125 | -4.67% | 0.114708 / 0.114916 | +0.18% |
| output/bounds-positive | 0.031917 / 0.032083 | +0.52% | 0.031917 / 0.032208 | +0.91% |
| output/diagnostics-amplified | 451.918125 / 511.722625 | +13.23% | 407.045958 / 415.248792 | +2.02% |
| output/diagnostics-pipe-amplified | 466.924750 / 446.270542 | -4.42% | 487.279250 / 485.453458 | -0.37% |
| output/diagnostics-standalone-amplified | 480.134292 / 445.044458 | -7.31% | 440.774958 / 434.097667 | -1.51% |
| output/flags-positive | 0.017708 / 0.018833 | +6.35% | 0.017292 / 0.018208 | +5.30% |
| output/stat-positive | 0.110209 / 0.100500 | -8.81% | 0.098541 / 0.103041 | +4.57% |
| output/write-positive | 0.134958 / 0.135208 | +0.19% | 0.128083 / 0.131708 | +2.83% |
| tooling/build-context | 60.134917 / 58.769458 | -2.27% | 55.455042 / 63.720500 | +14.90% |
| tooling/git-inspection | 120.313500 / 129.350084 | +7.51% | 109.844458 / 113.209375 | +3.06% |
| tooling/hashing | 42.451709 / 37.955291 | -10.59% | 42.605083 / 49.847000 | +17.00% |
| tooling/supervision | 256.585583 / 338.508750 | +31.93% | 222.347292 / 221.189959 | -0.52% |

## Audit and limitations

The [postpublication relocated audit receipt](offline-audit.json) records successful
verification using tooling restored from archived source blobs, with original
checkout/campaign I/O and process launches blocked. A corrupted relocated archive
was rejected. This audit verifies inventory, producer/input links and recomputed
sample summaries; it does not rebuild or rerun binaries. The receipt is an
explicitly separate postpublication observation, not a fabricated captured check.

Docker, Podman and the PPC compiler were unavailable. Ubuntu Unicode and native
PowerPC/Tiger acceptance were not established. Host cross-selection fixtures do
not replace them. Existing AppleClang/SDK warning accommodations are recorded in
both build configurations; checks were not weakened. CMake runtime coverage used
the installed version rather than every supported historical CMake release.
Earlier Darwin argv-inspection failures remain source-bound unavailable attempts;
the fresh final campaign's quiet checks succeeded.

Recheck the immutable bundle with the archived-compatible tooling:

```sh
python3 scripts/validation_evidence.py verify-bundle \
  docs/validation/interactive-command-package-integrity-followup/e502981668834de6b65125a42142611a
```

Campaign acceptance requires all required checks; known analyzer observations and
unavailable platform coverage remain distinct. No provenance was backfilled and
no historical report, sample, manifest or archived blob was rewritten.
