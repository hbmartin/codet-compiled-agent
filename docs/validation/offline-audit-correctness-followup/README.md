# Offline audit correctness follow-up

- Frozen implementation and tests: `90b08d30f31750c7e1d549a1a84cf6626d599371`.
- Baseline: `55bb403e1fab88b7968c64a8b06001c89433679b`, the merge of PR #75.
- Capture: `eaf52e27be994e4aadad39ccb6bc73b6`. Campaign: `62e0c930f86b4285b0b9ef05cda3db3f`.

The driver opens a regular inventory file without following symlinks, validates
fallback source membership before reading, and executes the bound entrypoint
from its verified bytes. Published replays use an independently supplied inventory
digest. Fixture cleanup propagates SIGINT recorded by the supervisor after the
owned groups and streams have been handled.

## Captured results

All 42 required checks passed on the macOS host. Both analyzer observations
returned exit 1. The baseline and changed runs each recorded 147 clang-tidy
findings, zero Cppcheck findings, and one Clang analyzer finding. The required
comparison found no additions, removals, or relocations.

| Check | Result |
| --- | --- |
| Debug and ASan/UBSan CTest | 13/13 targets passed in each build |
| ThreadSanitizer and regular focused suites | 20 repetitions of 140 cases passed in each suite |
| Apple Python 3.9.6 evidence, attempt, and Tiger recovery | 211, 7, and 54 tests passed |
| Python 3.9 CLI integration | 138 cases passed |
| Python, native and CLI, and Semgrep negative controls | 122, 63, and 2 passed |
| Pinned lint, Semgrep, Make, and host cross-selection | Passed |
| Release benchmark campaign | 45 workloads, two sets, 900 measured samples and 180 warm-ups |
| Relocated offline audit | 44 check records and 14,650 input links verified. Corruption was rejected. |

The benchmark capture records 1,080 raw samples, 180 median rows, and 90
comparisons. Some deterministic work counters differ. The raw samples retain
those differences and their measurement scope. The host campaign covers neither
Ubuntu runtime nor native Tiger execution.

The fixed driver also replayed the previously published `ad2830…` and `57253d…`
complete bundles and the `f990e2…` attempt bundle with pinned inventory hashes.
All three rejected the corruption control. The attempt preserved
`eligible_current=false` and the expected `benchmarks: failed` disposition.

## Evidence and replay

- [Lossless audit carrier](eaf52e27be994e4aadad39ccb6bc73b6/audit-bundle.tar.gz)
- [Exact manifest](eaf52e27be994e4aadad39ccb6bc73b6/manifest.json) and [measurement index](eaf52e27be994e4aadad39ccb6bc73b6/measurements.json)
- [Raw samples](eaf52e27be994e4aadad39ccb6bc73b6/samples.csv) and [medians](eaf52e27be994e4aadad39ccb6bc73b6/medians.csv)
- [Transport metadata](eaf52e27be994e4aadad39ccb6bc73b6/transport.json) and [digest-pinned audit receipt](eaf52e27be994e4aadad39ccb6bc73b6/offline-audit.json)

Transport verification compared all 479 expanded files byte for byte. The relocated
receipt binds 478 inventory entries and records the executed verifier digest. Replay
from a trusted checkout of this publication and read the expected inventory digest
from its transport record.

```sh
base=docs/validation/offline-audit-correctness-followup/eaf52e27be994e4aadad39ccb6bc73b6
unpacked=$(mktemp -d)
inventory=$(jq -er .bundles.main.inventory_sha256 "$base/transport.json")
tar -xzf "$base/audit-bundle.tar.gz" -C "$unpacked"
/usr/bin/python3 -B scripts/evidence_offline_audit.py \
  "$unpacked/eaf52e27be994e4aadad39ccb6bc73b6" "$unpacked/replayed-audit.json" "$PWD" \
  --expected-inventory-sha256 "$inventory"
```
