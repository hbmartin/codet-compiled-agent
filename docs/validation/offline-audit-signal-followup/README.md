# Offline audit signal follow-up

The Darwin quiet-window inspector retries a transient `EINVAL` from an argv
read by discarding the partial process snapshot and starting a fresh sweep.
It allows up to two retries within the original five-second budget. A successful
sweep still requires a nonempty snapshot free of blocking processes.

- Frozen implementation `c3462978dde91af9f04bdda2d5416c88ebed9659`, based on the merge of PR #77 at `b768d5432977a8827b7f74e1bcfc6263832ace99`.
- Release benchmark baseline `337d4da8dfb5af8e32996f815e06873bc30dd52a`, the merge of PR #76.
- Capture `5b3145242f0b41c5a4d318c810c09014` and campaign `7b95b36520374cbcaeccd54425a88e8d`.

## Captured results

All 42 required checks passed on macOS arm64. The two analyzer observations
each returned exit 1 and recorded 147 clang-tidy findings, zero Cppcheck findings,
and one Clang analyzer finding. The required comparison found zero additions,
removals, and relocations.

| Check | Result |
| --- | --- |
| Debug and ASan/UBSan CTest | 13 targets passed in each build, run serially |
| ThreadSanitizer and regular focused suites | 20 repetitions of 140 cases passed in each suite |
| Apple Python 3.9.6 evidence, attempt, and Tiger recovery | 217, 7, and 54 tests passed |
| Python 3.9 CLI integration | 138 tests passed |
| Python, native and CLI, and Semgrep negative controls | 124, 63, and 2 passed |
| Lint, Semgrep, Make, historical audit, and host cross-selection | Passed |
| Release benchmarks | 45 workloads across two sets, with 900 measured samples and 180 warm-ups |
| Relocated offline audit | 44 check records and 14,650 input links verified. Corruption was rejected. |

The 1,080 raw samples yield 180 median rows and 90 baseline comparisons.
The benchmark record retains differences in work counters for 14 comparisons.
The manifest schedules benchmark prerequisites and measurements ahead of the
long native test suites. This capture covers macOS host execution and host
cross-selection for Tiger.

Two earlier captures remain in the local campaign scratch directory.
Capture `85deb9b29c19492aad79e901935332a1` hit the five-second inspector
timeout. Capture `32ec67466795414291fc371d617647b4` detected a concurrent
Python test run. The published evidence came from a fresh capture after the
quiet-window checks passed.

## Evidence and replay

- [Lossless audit carrier](5b3145242f0b41c5a4d318c810c09014/audit-bundle.tar.gz)
- [Exact manifest](5b3145242f0b41c5a4d318c810c09014/manifest.json) and [measurement index](5b3145242f0b41c5a4d318c810c09014/measurements.json)
- [Raw samples](5b3145242f0b41c5a4d318c810c09014/samples.csv) and [medians](5b3145242f0b41c5a4d318c810c09014/medians.csv)
- [Transport record](5b3145242f0b41c5a4d318c810c09014/transport.json) and [digest-pinned audit receipt](5b3145242f0b41c5a4d318c810c09014/offline-audit.json)

The transport verified 479 expanded files byte for byte. A fresh
extraction passed the pinned relocated audit and its corruption control.
Verify the archive and pin the inventory to the recorded digests from a trusted
checkout of this publication. If you obtain the carrier separately, authenticate
the expected digests through the publisher or a verified signed tag.

```sh
set -e
run=5b3145242f0b41c5a4d318c810c09014
base=docs/validation/offline-audit-signal-followup/$run
unpacked=$(mktemp -d)
archive=$(jq -er .bundles.main.sha256 "$base/transport.json")
inventory=$(jq -er .bundles.main.inventory_sha256 "$base/transport.json")
test "$(shasum -a 256 "$base/audit-bundle.tar.gz" | cut -d ' ' -f 1)" = "$archive"
tar -xzf "$base/audit-bundle.tar.gz" -C "$unpacked"
/usr/bin/python3 -B scripts/evidence_offline_audit.py \
  "$unpacked/$run" "$unpacked/replayed-audit.json" "$PWD" \
  --expected-inventory-sha256 "$inventory"
```
