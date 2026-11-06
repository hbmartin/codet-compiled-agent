# Queued input and Tiger provenance follow-up

The accepted source freeze is `24af5b1d14fd67b82b46e9ca10fe07bf26a13c85`, based on freshly fetched main `2d599f8ec07f19d3abd2719be8bea5b970871ae0`. The original checkout was preserved; implementation and validation used the isolated `codex/queued-input-tiger-recovery-followup` worktree.

[Immutable audit bundle](57253daf77c94391858174663ae6d86d/report.json) · [exact manifest](57253daf77c94391858174663ae6d86d/manifest.json) · [inventory](57253daf77c94391858174663ae6d86d/inventory.json) · [captured outcomes](57253daf77c94391858174663ae6d86d/capture) · [raw benchmark samples](57253daf77c94391858174663ae6d86d/measurements/benchmarks/measure.json/samples.csv) · [medians](57253daf77c94391858174663ae6d86d/measurements/benchmarks/measure.json/medians.csv).

## Validation

All 37 required checks passed. Two additional analyzer observations retain their actual nonzero outcomes. These are not relabeled as successful analyzer runs.

| Check | Result |
| --- | --- |
| Debug CTest | 12/12 passed |
| ASan/UBSan CTest | 12/12 passed; 684 enabled unit cases, 682 passed and two explicit opt-in skips |
| Focused input/diagnostic/lifecycle and TSan | Both passed 20 repetitions of 114 cases |
| Python 3.9.6 | 130 tooling and 121 integration tests passed |
| Pinned lint, Semgrep, runner tests | Passed |
| Host cross-selection | Configure, link and tests passed; not a PPC build |
| Python mutation controls | 32 passing baselines and intended assertion failures |
| Native/integration mutation controls | 30 passing baselines and intended assertion failures |
| Analyzer comparison | Equal full finding identities/context, with 49 reviewed location-only relocations |
| Clang-Tidy / Cppcheck / Clang analyzer | Both variants: 147 known findings / 0 findings / 1 known finding |
| Measurement and positive-control checks | Passed, including quiet-window checks |

The two opt-in unit skips are `NetworkOptIn.TlsHandshake` and `Contracts.NativeTigerDesktopAcceptance`. One deliberately disabled fixture-printer test remains disabled. Real loopback TLS integration tests ran separately. Native Tiger runtime acceptance, Docker/Podman Ubuntu execution and the PPC compiler are unavailable locally. Host fixtures cannot establish native Tiger acceptance. CMake 4.4.4 and AppleClang 21 were exercised; a native Tiger runtime and a CMake 3.20 runtime were not. Existing SDK warning accommodations are recorded in both variants' identical configuration.

Seven earlier attempts retain their actual failures/interruption records in the [history artifact](57253daf77c94391858174663ae6d86d/artifacts/historical-attempt/history.json). They do not satisfy this run's required checks. Validation found and repaired local filesystem preparation errors, the curl feature-constant guard, explicit signal-test synchronization, intended-assertion fixture cleanup, and a final-output PTY drain race. The deterministic drain regression and real PTY case also passed 100 exploratory repetitions; those repetitions are not substituted for the final captured suite.

## Measurements

Two sets completed with one warm-up and five alternating measured runs per variant/workload. The bundle contains 696 samples, including 580 measured samples across 29 workloads. It records native allocation/byte/output counters and precisely named Python API-call and traced-memory observations. Python retained blocks are not allocation events; API calls are not asserted to be total kernel syscalls.

The following are observed median milliseconds, baseline / change, with all raw values retained:

| Workload | Set 1 | Set 2 |
| --- | --- | --- |
| line/native-pending-submission | 1.2948 / 1.1692 | 1.1915 / 1.1917 |
| line/native-unchanged | 0.1978 / 0.1957 | 0.1973 / 0.1951 |
| output/diagnostics-amplified | 394.5940 / 391.2982 | 391.6989 / 390.9370 |
| export/small | 54.8260 / 54.0785 | 53.3687 / 52.7837 |
| export/large | 251.5151 / 253.4811 | 257.2264 / 257.2189 |
| tooling/build-context | 75.2886 / 51.2719 | 74.5763 / 50.6273 |
| tooling/hashing | 36.0425 / 35.5034 | 34.3693 / 34.0532 |
| tooling/git-inspection | 109.0374 / 113.8744 | 97.9670 / 98.3778 |

No timing thresholds or general speed claim are made. The work-equivalence field is explicitly false: hashing/Git inspect 121 baseline versus 124 changed production files; the build-context workload performs different verified work. Its context resolutions change 3→1, sysroot snapshots 3→2 and direct file-hash calls 36→19. Peak traced memory increases despite fewer retained blocks. These are scoped synthetic host observations, not native Tiger measurements. Queue ordering/admission is covered functionally; the performance harness measures pending-line admission/presentation, not isolated Interaction queue-container latency.

## Audit and packaging scope

The bundle was audited after relocation using auditor sources reconstructed from declared digest-verified inputs, while reads/stat operations under the original checkout were blocked. It verified 39 checks and 12,711 audit-input declarations without rerunning binaries. Corrupting an archived input was rejected as an inventory mismatch. [Post-publication audit receipt](offline-audit-57253daf77c94391858174663ae6d86d.json) is an integrity observation, explicitly separate from the captured campaign checks.

To audit using the frozen checkout's supported Python 3.12+ tooling:

```sh
python3.12 scripts/validation_evidence.py verify-bundle \
  docs/validation/queued-input-tiger-recovery-followup/57253daf77c94391858174663ae6d86d
```

Tiger provenance format 2 records effective configure-time compilers and full pathful audit inputs. Repeated archives are deterministic only for one frozen verified build; no cross-checkout rebuilt-binary/archive identity is claimed. Builds remain fresh, with a 1,800-second compile/link default and explicit positive tooling override. Trees accumulate by design; manual cleanup must be performed only when Tiger operations are stopped. Automatic pruning and reuse remain deferred. Historical reports and source blobs are unchanged.
