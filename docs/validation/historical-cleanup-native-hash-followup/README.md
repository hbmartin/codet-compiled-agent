# Preserved cleanup failures and native-hash evidence

A genuine post-publication cleanup failure now remains primary if a later signal
interrupts unwinding. Exception identity, original explicit cause, committed receipt,
and secondary cleanup observations are retained. Pure committed interruptions remain
publication observations; measurement consumers reject interrupted operations.

Fixture cleanup uses immutable original child argv instead of reading its expected
identity from the PID currently under inspection. Unknown identities never authorize
signals; vanished-child races are positively rechecked. In-process signal fixtures
own and restore SIGINT, including ignored and unittest-managed dispositions. Secondary
fixture cleanup errors retain the original assertion.

Offline guards use filesystem ancestor identities for case/firmlink aliases and
respect final-component no-follow metadata semantics. Archived verifier tooling has
explicit import priority and scoped module-cache isolation. These are selected Python
API guards, not an OS sandbox. Existing cached accessors, directory listings, direct
OS process primitives, and descriptor-only access remain outside their scope;
ambiguous relative dir_fd identity is rejected. Receipts identify the actual verifier.

## Source and acceptance

- Fresh baseline: `6aeaeba2e90eb1f2cf428c5fca349da0535458bb`.
- Frozen implementation/tests/producers: `5eee80619b86713880f58cc9e00a1e5ea7cbc794`.
- Capture: `ad2830bbbfe84e879ba5d65da1a11f2f`; campaign:
  `106b5335dde942a5b0cf98de106e4266`.
- All **42/42 required checks passed**. The two analyzer observations retain exit 1.
- The complete benchmark campaign passed strict eligibility and work verification:
  1,080 raw samples, including 900 measured and 180 warm-up samples; 45 workloads;
  180 recomputed median rows and 90 comparison rows.
- Evidence v5, selection format 3, Tiger provenance 3, sysroot 2, public runtime
  behavior, and the hashed sysroot builder are unchanged. Historical evidence and
  the original checkout remain unchanged.

## Captured validation

| Check | Actual outcome |
| --- | --- |
| Debug CTest | 13/13 passed |
| ASan/UBSan CTest | 13/13 passed |
| TSan focused suite | Twenty repetitions of 140 cases passed |
| Regular focused suite | Twenty repetitions of 140 cases passed |
| Verified Python 3.9.6 tooling | 202 tests passed |
| Configuration/provenance | 54 tests passed |
| Python 3.9 attempt audits / CLI integration | Passed / 138 cases passed |
| Make build / preparation smoke / unchanged rebuild | Passed |
| Pinned lint / Semgrep / runner checks | Passed |
| Host cross-selection configure/build/tests | Passed; host-only |
| Python/CMake negative controls | 111 passing baselines and intended assertion failures |
| Native/CLI negative controls | 63 passing baselines and intended assertion failures |
| Production Semgrep controls | Two passed |
| Immutable Release variants / identical shared harness | Built and verified |
| Full two-set measurement campaign | Passed |
| Historical carrier audit | 477 files / 383,683,534 expanded bytes verified |
| Metadata / availability | Captured |
| Relocated offline audit / corruption control | Passed / rejected changed archived input |

All 25 analyzer translation units have complete matching results: 147 Clang-Tidy
findings, zero Cppcheck findings, and one Clang analyzer finding per variant. Both
analysis commands exited 1. There are no added/removed findings or reviewed relocations.
The comparison passing does not relabel the analyzer observation exits.

Installed CMake 4.4.4, ccmake, Make and Ninja coverage ran. Minimum CMake 3.20 and
cmake-gui were not exercised. Docker, Podman, and the PPC compiler were unavailable.
Ubuntu runtime and native Tiger acceptance are unavailable locally; host fixtures and
host cross-selection do not establish native target acceptance. No old CI run is
substituted for this source.

## Reproductions and controls

Focused regressions cover cleanup-error plus interruption, preservation of the
original cause and all secondary errors, genuine cleanup failure after publication,
scope exit and handler restoration, and pre-commit unwinding. Existing published
destinations remain protected. No staging sweep was added; SIGKILL/host failure may
still leave owned staging.

Process-identity and PID-reuse cases mock every invalid-identity signal. The fallback
uses the recorded immutable argv; ambiguous inspection never supplies its own authority.
Real owned children exercise readiness, parent exit, SIGINT/SIGTERM, bounded gates,
EOF, cleanup and retry. Handler scopes restore caller dispositions on failure.

Nine additional negative controls independently remove cleanup-failure ownership,
cause preservation, native-hash delegation, pre-lookup validation, immutable process
identity, scope SIGINT ownership, filesystem identity, no-follow semantics, and verifier
precedence. All controls require passing baselines and intended executed assertions;
skips, errors, compilation, discovery, launch and timeouts do not count.

The first development control run correctly rejected an import-guard mutation because
the synthetic fixture's missing inventory binding triggered another guard first.
The fixture now has a valid inventory so removal of the import guard reaches its
intended assertion. Development logs remain local; final captured controls all passed.

## Measurement definition and deterministic counters

Extraction workload **contract 4** delegates to each variant's native hasher and
observes actual stream reads while preserving its buffer strategy and exceptions.
The measured scope remains the complete verified operation: carrier verification,
extraction, auditing, final verification, cleanup, and instrumentation overhead.
It is an instrumented operation observation rather than an isolated hashing cost.

Contract-3 historical observations used the shared harness hash loop. Contracts 1–3
retain their original decoding only in explicit historical auditing; none satisfies
new live extraction acceptance. Historical reports/samples remain unchanged.

Every extraction sample reports three successful native archive-hash calls and
28,717,194 read bytes for the 9,572,398-byte declared archive, plus 477 expanded files
and 383,683,534 expanded bytes. The archived carrier identity is shared by both
variants. Publication interruptions are observed and zero in accepted samples.
Layout validation reports 128 checks, 640 returned build locations, and zero files.
Current changed saturated preparation samples allocate zero payload-sized buffers;
Changed CLI preparation uses one command wrapper per accepted submission; changed
preflight controls report 256 advisory budget checks. Positive controls
passed. Scoped traced blocks/bytes are retained-memory observations, not allocation
event totals or kernel operation counts.

The identical declared harness and compiler settings run one warm-up and five
alternating measurements per variant in each of two sets. Source, harness, compiler,
configuration, executable and link identities are retained in the captured envelope.
All raw samples, medians and computed differences are published. No optimization
experiment or timing threshold was added.

The verifier reports `all_measured_work_equal=false`: traced-memory counters differ
in 13 tooling comparisons. Those observations remain intact. Timings and memory
observations support no general performance or work-equivalence claim.

### Median latency observations

Values below come from the verified comparison payload. Percent is
`100 * (changed_ms / baseline_ms - 1)`; no threshold is applied.

| Harness / workload | Set 1 baseline ms | Set 1 changed ms | Difference % | Set 2 baseline ms | Set 2 changed ms | Difference % |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| export / large | 261.787 | 260.123 | -0.635 | 261.929 | 259.271 | -1.015 |
| export / small | 59.4169 | 59.021 | -0.666 | 56.5973 | 56.6257 | +0.050 |
| line / allocation-positive | 0.002125 | 0.002083 | -1.976 | 0.00225 | 0.002292 | +1.867 |
| line / bounded-expansion | 44.4069 | 44.5822 | +0.395 | 49.0663 | 47.7812 | -2.619 |
| line / busy | 0.135541 | 0.134542 | -0.737 | 0.139917 | 0.143875 | +2.829 |
| line / deferred-tail | 0.386625 | 0.401167 | +3.761 | 0.391583 | 0.390667 | -0.234 |
| line / deletion | 0.097417 | 0.101416 | +4.105 | 0.110417 | 0.101958 | -7.661 |
| line / editing | 2.7635 | 2.76662 | +0.113 | 2.87483 | 2.92362 | +1.697 |
| line / editing-basic | 2.60288 | 2.59146 | -0.439 | 2.69404 | 2.72183 | +1.032 |
| line / geometry-positive | 0.202584 | 0.203959 | +0.679 | 0.204584 | 0.21025 | +2.770 |
| line / native-boundary | 0.822 | 0.821333 | -0.081 | 0.871375 | 0.873917 | +0.292 |
| line / native-busy | 0.858291 | 0.857875 | -0.048 | 0.809375 | 0.817542 | +1.009 |
| line / native-editing | 20.8968 | 19.9997 | -4.293 | 21.0848 | 21.3058 | +1.048 |
| line / native-flush-boundary | 0.855584 | 0.85275 | -0.331 | 0.859417 | 0.868209 | +1.023 |
| line / native-pending-fragment | 2.11067 | 2.12113 | +0.495 | 2.07529 | 2.13512 | +2.883 |
| line / native-pending-submission | 1.17392 | 1.16567 | -0.703 | 1.18758 | 1.21329 | +2.165 |
| line / native-unchanged | 0.239333 | 0.224208 | -6.320 | 0.230083 | 0.222875 | -3.133 |
| line / pending-submission | 0.39475 | 0.39675 | +0.507 | 0.4085 | 0.395917 | -3.080 |
| line / preparation-queue | 5.50571 | 5.46533 | -0.733 | 5.36475 | 5.48238 | +2.193 |
| line / unchanged | 0.148375 | 0.147333 | -0.702 | 0.146625 | 0.150666 | +2.756 |
| output / bounds-positive | 0.030834 | 0.030583 | -0.814 | 0.031584 | 0.031417 | -0.529 |
| output / diagnostics-amplified | 407.149 | 409.802 | +0.651 | 405.889 | 402.8 | -0.761 |
| output / diagnostics-pipe-amplified | 409.342 | 412.915 | +0.873 | 409.579 | 409.292 | -0.070 |
| output / diagnostics-standalone-amplified | 417.634 | 416.309 | -0.317 | 415.533 | 406.509 | -2.172 |
| output / flags-positive | 0.017125 | 0.016959 | -0.969 | 0.017292 | 0.017083 | -1.209 |
| output / stat-positive | 0.094208 | 0.096 | +1.902 | 0.095833 | 0.095959 | +0.131 |
| output / write-positive | 0.121208 | 0.124 | +2.303 | 0.130458 | 0.129167 | -0.990 |
| preparation / allocation-positive | 0.000417 | 0.0005 | +19.904 | 0.000584 | 0.000458 | -21.575 |
| preparation / cli-preparation | 6.00258 | 6.2325 | +3.830 | 6.11412 | 6.11617 | +0.033 |
| preparation / compatibility-preparation | 132.834 | 131.407 | -1.075 | 131.967 | 132.547 | +0.439 |
| preparation / normalization | 129.006 | 131.614 | +2.022 | 131.217 | 131.468 | +0.191 |
| preparation / preflight-checks | 0.028417 | 0.027916 | -1.763 | 0.029291 | 0.0295 | +0.714 |
| preparation / saturated-count | 1.19233 | 1.21317 | +1.747 | 1.202 | 1.17863 | -1.945 |
| preparation / saturated-effective | 21.2137 | 21.0864 | -0.600 | 22.2367 | 22.0889 | -0.665 |
| preparation / saturated-retained | 22.2429 | 22.251 | +0.037 | 21.9896 | 22.0744 | +0.385 |
| tooling / build-context | 54.3983 | 52.6817 | -3.156 | 53.7449 | 52.4549 | -2.400 |
| tooling / campaign-layout | 970.631 | 986.123 | +1.596 | 995.612 | 998.613 | +0.301 |
| tooling / configuration-selection | 356.733 | 354.586 | -0.602 | 378.44 | 374.971 | -0.917 |
| tooling / dependency-resolution | 320.442 | 198.518 | -38.049 | 327.65 | 203.858 | -37.782 |
| tooling / git-inspection | 110.631 | 107.895 | -2.472 | 122.422 | 112.265 | -8.297 |
| tooling / hashing | 38.9308 | 38.061 | -2.234 | 39.3208 | 38.2574 | -2.704 |
| tooling / historical-extraction | 9529.63 | 9162.11 | -3.857 | 8262.13 | 8206.81 | -0.670 |
| tooling / historical-samples | 35.2775 | 34.9987 | -0.790 | 34.9018 | 34.6469 | -0.730 |
| tooling / measurement-cleanup | 33.8908 | 35.737 | +5.448 | 34.2957 | 37.249 | +8.611 |
| tooling / supervision | 195.261 | 193.388 | -0.959 | 196.098 | 197.07 | +0.496 |

### Retained traced-memory differences

All reported work differences are retained below; values are baseline → changed.
The complete samples retain allocations, output operations, bytes, scoped metadata
counters, native-hash reads, and memory observations for their respective workloads.

| Set | Tooling workload | Reported differences |
| --- | --- | --- |
| 1 | campaign-layout | retained_traced_blocks: 9165 → 9168; retained_traced_bytes: 1463538 → 1463479; peak_traced_bytes: 1491161 → 1491155 |
| 1 | configuration-selection | retained_traced_blocks: 107 → 106; retained_traced_bytes: 11307 → 11262; peak_traced_bytes: 143413 → 143368 |
| 1 | dependency-resolution | retained_traced_bytes: 569430 → 569382; peak_traced_bytes: 573947 → 572857 |
| 1 | git-inspection | retained_traced_blocks: 414 → 409; retained_traced_bytes: 36535 → 36243 |
| 1 | hashing | retained_traced_bytes: 33683 → 33657; peak_traced_bytes: 320176 → 306398 |
| 1 | historical-extraction | retained_traced_blocks: 9625 → 9637; retained_traced_bytes: 1264617 → 1266488; peak_traced_bytes: 615831446 → 615834296 |
| 1 | historical-samples | retained_traced_bytes: 37403 → 37375; peak_traced_bytes: 44968 → 44940 |
| 2 | campaign-layout | retained_traced_blocks: 9164 → 9170; retained_traced_bytes: 1463493 → 1463571; peak_traced_bytes: 1491116 → 1491247 |
| 2 | dependency-resolution | retained_traced_bytes: 569430 → 569382; peak_traced_bytes: 573947 → 572857 |
| 2 | git-inspection | retained_traced_blocks: 414 → 409; retained_traced_bytes: 36535 → 36243; peak_traced_bytes: 6415322 → 6420501 |
| 2 | hashing | retained_traced_bytes: 33683 → 33657; peak_traced_bytes: 320172 → 306390 |
| 2 | historical-extraction | retained_traced_blocks: 9512 → 9437; retained_traced_bytes: 1258740 → 1257102; peak_traced_bytes: 615825175 → 615823317 |
| 2 | historical-samples | retained_traced_bytes: 37403 → 37375; peak_traced_bytes: 44968 → 44940 |

## Bundle, offline audit and replay

The new immutable carrier is [ad2830bbbfe84e879ba5d65da1a11f2f/audit-bundle.tar.gz](ad2830bbbfe84e879ba5d65da1a11f2f/audit-bundle.tar.gz).
It preserves all 483 expanded files / 398,186,298
bytes, including source snapshots, configs, captures, logs, build metadata, outcomes,
raw samples, sidecars and archived tooling. Every expanded member was round-tripped
and digest-compared before exclusive publication. Existing destinations are refused.

- Carrier bytes: 19,176,619; SHA-256: `518ba23675b3e608c2d77282f6b5c0dc677bff7d3fb797a6844e4267f48680d0`.
- Inventory SHA-256: `ed193f291b440e02a05add71e75f898bd671c2de8622b1ed52005e67913bafe1`.
- [Exact manifest](ad2830bbbfe84e879ba5d65da1a11f2f/manifest.json).
- [All raw samples](ad2830bbbfe84e879ba5d65da1a11f2f/samples.csv) and [all median rows](ad2830bbbfe84e879ba5d65da1a11f2f/medians.csv).
- [Relocated audit receipt](ad2830bbbfe84e879ba5d65da1a11f2f/offline-audit.json) and
  [lossless transport provenance](ad2830bbbfe84e879ba5d65da1a11f2f/transport.json).
- Tables derive from producing check `benchmarks`, captured envelope SHA-256
  `559b24a14e459986d4e96add4b9481c3216838d03ad5dfcb319523500aaa3418`.

The relocated Python 3.9.6 audit verifies 44 checks and 14,650 input links without
rerunning binaries. Its actual selected archived verifier SHA-256 is
`4ac7a730fe07b7a53c03f89661f1c4fa0ee55596bc8f9f2134931ab54cb30de3`. The corruption control rejected a changed archived
input with `Audit inventory mismatch`. The receipt enumerates the guarded APIs,
interpreter, canonical original-root paths, guard phase and exclusions. It establishes
recorded integrity and current acceptance for this declared host campaign, not
rebuilding reproducibility or native coverage on unavailable platforms.

Replay from a fresh directory using the frozen producer checkout and the bundle's
own archived tooling. Original checkout paths in provenance are opaque records.

```sh
mkdir /tmp/codet-native-hash-audit
base=docs/validation/historical-cleanup-native-hash-followup/ad2830bbbfe84e879ba5d65da1a11f2f
tar -xzf "$base/audit-bundle.tar.gz" -C /tmp/codet-native-hash-audit
bundle=/tmp/codet-native-hash-audit/ad2830bbbfe84e879ba5d65da1a11f2f
python3 -B scripts/evidence_offline_audit.py "$bundle" /tmp/codet-native-hash-receipt.json "$PWD"
```

The driver reconstructs this bundle's archived verifier from digest-keyed inputs,
uses that tooling ahead of root copies and checkout caches, and restores import/guard
state. The carrier includes the full expanded report; no report compaction, missing
outcome reconstruction, or historical artifact rewriting was performed.
