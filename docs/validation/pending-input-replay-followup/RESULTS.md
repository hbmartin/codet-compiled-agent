# Captured results: pending input and campaign replay

The immutable [v5 bundle](20261104T073413Z-ab2fde3/report.json) records campaign `1bc8d7f37d7244b4a350356f19e6e090`. Baseline: fetched main `ddfbcc0bd7fa216cc12d36c1b5a1d2059b496344`. Frozen implementation/tests/producers: `ab2fde3`. All builds used fresh campaign-local directories; both Release variants used the same separately declared harness snapshot. Historical reports, samples, manifests and archived source blobs were preserved.

## Captured validation

All **37 required checks passed**. Two additional analyzer observations retain their actual exit status 1 because of existing findings. The complete command vectors, resolved environments, cwd-resolution chains, source/build inputs, executable identities, logs and outcomes are in the bundle.

- Debug CTest: 10/10 targets passed; ASan/UBSan CTest: 10/10.
- TSan and ordinary focused runs: each passed 20 repetitions of 81 tests.
- Python 3.9.6: 106 tooling tests and 103 CLI integration tests passed.
- Pinned lint, Semgrep, enforcement runners, analyzer canaries and host cross-selection configure/build/tests passed.
- Controls: 10 native, 21 Python and two production-site Semgrep controls passed their unmodified baselines, applied exact mutations and failed through the intended assertions. Compilation, discovery, launch and timeout errors did not count.
- Full 25-source analyzer inventories matched: 150 Clang-Tidy findings, zero Cppcheck findings and one Clang Analyzer finding on each variant. Five reviewed Clang-Tidy location moves passed unchanged tool/check/message/file/translation-unit identity, source context and one-to-one checks. Known finding exits remain failed observations, not clean analyzer passes.

The gated regressions reserve an admitted native diagnostic while input reaches Enter. They check complete visible `abc` in known-width mode and on a shorter second basic-mode line; a fresh basic-mode line remains a positive control. Other gates cover deferred row termination, resize, intervening output, cancellation before handoff, successful queued follow-up, provider failure, quit, Ctrl-D, EOF and signals. Frozen approval/question answers are cancelled while reserved, then checked against a fresh session request: they cannot become ordinary messages or answer the new request. The keyboard negative control must fail before the diagnostic is released, through its explicit acknowledgement assertion. Request replacement also retains the original identity in the gated lifecycle test.

The first capture attempt failed on a Python 3.9-only HTTPError fixture issue and was interrupted. Its corrected fixture passes. The second attempt was interrupted to add the explicit frozen-request expiry regression. Both attempts' original outcomes and logs are archived as historical inputs; neither supplies required results for this campaign.

## Measurements

The [raw samples](20261104T073413Z-ab2fde3/measurements/benchmarks/measure.json/samples.csv) and [medians](20261104T073413Z-ab2fde3/measurements/benchmarks/measure.json/medians.csv) cover 25 workloads: 600 samples, including 500 measured samples and 100 warm-ups. Each of two sets has one warm-up plus five alternating measured runs per variant. Source, harness, compiler/configuration, executable and static-library identities are recorded. Positive controls verify counters. Measurements ran after analyzers with the process-eligibility guard; no timing thresholds were added.

Selected workload medians in milliseconds:

| Workload | Set 1 baseline → change | Set 2 baseline → change |
| --- | ---: | ---: |
| Short submission (`pending-submission`) | 0.2904 → 0.3060 | 0.2826 → 0.2988 |
| Unchanged native prompt flush | 0.1865 → 0.2061 | 0.1860 → 0.1961 |
| Native editing | 21.1391 → 21.2786 | 19.9323 → 19.8418 |
| Amplified diagnostics | 399.2898 → 396.8649 | 399.7299 → 398.9765 |
| Small export | 55.0733 → 55.4098 | 53.9153 → 54.2570 |
| Large export | 261.5244 → 253.8611 | 258.3038 → 255.1235 |
| Snapshot hashing | 34.3765 → 31.4092 | 33.2955 → 31.0735 |
| Git inspection | 133.6042 → 89.4666 | 132.4819 → 87.8493 |

The complete change has repeatable microbenchmark costs in pending submission (+5.4%/+5.7%) and unchanged native flush (+10.5%/+5.4%). Recorded native allocation/output/metadata counters are unchanged in those workloads. All measured work is **not** identical: tooling allocation/memory metrics differ. These samples establish no broad runtime speedup.

The separate [hashing experiment](20261104T073413Z-ab2fde3/artifacts/hash-experiment/hash-profile.json) isolates correctness-era hashing at `c692848` against the retained implementation on identical current input data. It has 72 samples, complete profiles, matching digests, allocation controls and recomputed acceptance. Each timed sample performs five snapshots; allocation/profiler metrics use another snapshot.

| Role | Set 1 baseline → change ms | Set 2 baseline → change ms | Profiled explicit stat calls |
| --- | ---: | ---: | ---: |
| Production | 57.02 → 51.18 | 53.16 → 48.23 | 250 → 20 |
| Validation | 67.85 → 49.52 | 67.71 → 48.97 | 409 → 33 |
| Build | 136.55 → 112.58 | 136.59 → 112.83 | 714 → 44 |

Both sets pass the latency/work gates for all three roles. Peak traced memory and retained allocation counts decrease. The counters describe profiled Python calls, not a complete kernel syscall trace. The optimization reuses one 64 KiB read buffer within a snapshot, reduces repeated selection/traversal work, and shares no file hashes across command boundaries.

## Offline audit and limits

[Original audit](20261104T073413Z-ab2fde3-audit-original.json) and [relocated audit](20261104T073413Z-ab2fde3-audit-relocated.json) both report format v5 eligibility, 39 checks and 11,598 audit-input records. The relocated audit ran from `/private/tmp` under Python 3.9 using verifier scripts reconstructed from the bundle's archived validation inputs. It did not rerun binaries or require the original source/configuration paths for lookup. Inventory completeness, hashes, declared coverage, producer links, cwd relationships and raw-sample summaries were checked.

Docker, Podman and a PPC compiler are unavailable locally. Host cross-selection is not native Tiger/PPC runtime verification. No source-matched Ubuntu CI result is claimed: publishing/dispatch remained blocked by automatic approval review pending renewed publishing confirmation. Public CI reader tests cover credential-free requests, pagination, identity/attempt changes, unsuccessful jobs, rate limits and deadlines. Existing Apple Clang 21 builds retain the campaign's two previously documented warning accommodations, equally applied to both variants; no additional analyzer suppression was introduced.

Pending presentation remains best effort on actual delivery failure, and blocking TTY output remains outside bounded cancellation. Unicode/data policies, completed-turn persistence, diagnostic order, terminal cleanup ownership and storage formats are preserved.
