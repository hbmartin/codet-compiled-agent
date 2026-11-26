# Offline audit signal follow-up

The Darwin quiet-window inspector gives an exiting process time to clear before
repeating the full process sweep. It allows two retries, with delays of 75 ms
and 150 ms when the PID remains signalable. When the probe reports that the PID has
disappeared, the next sweep can start immediately. A denied probe identifies the
process in the error. The parent sets the worker's deadline before launch and
reserves time to report diagnostics within its five-second timeout.

- Frozen implementation `0006eaf1062cc9a8589644493fa0d9380317dd2f`, based on the merge of PR #78 at `9edb71c6cf45575aa2852066111558787044d2cf`.
- Release benchmark baseline `337d4da8dfb5af8e32996f815e06873bc30dd52a`, the merge of PR #76.
- Capture `76e82620674f4847af4290c572823a27` and campaign `b8d0792262cb4f258f62f5d514e56ee6`.

## Captured results

All 42 required checks passed on macOS arm64. The two analyzer observations
each returned exit 1 and recorded 147 clang-tidy findings, zero Cppcheck
findings, and one Clang Analyzer finding. The required comparison found zero
additions, removals, and relocations.

| Check | Result |
| --- | --- |
| Debug and ASan/UBSan CTest | 13 targets passed in each build, run serially |
| ThreadSanitizer and regular focused suites | 20 repetitions of 140 cases passed in each suite |
| Apple Python 3.9.6 evidence, attempt, and Tiger recovery | 223, 7, and 54 tests passed |
| Python 3.9 CLI integration | 138 tests passed |
| Python, native and CLI, and Semgrep negative controls | 129, 63, and 2 passed |
| Lint, Semgrep, Make, historical audit, and Tiger host cross-selection | Passed |
| Release benchmarks | 45 workloads across two sets, with 900 measured samples and 180 warm-ups |
| Relocated offline audit | 44 check records and 14,650 input links verified. Corruption was rejected. |

The 1,080 raw samples yield 180 median rows and 90 baseline comparisons.
The benchmark record retains differences in work counters for 17 comparisons.
The manifest schedules benchmark prerequisites and measurements ahead of the
long native test suites. The evidence covers modern macOS host execution and
Tiger packaging, recovery, and host cross-selection checks.

Two incomplete captures remain in the local campaign scratch directory. Capture
`745e88e1c34a45efa44e4dc93f607d93` stopped after a native CLI mutant missed its
intended assertion. Capture `cf7bde71f9ca47d3ad5a820fc07b581e` stopped when the
quiet-window gate detected a concurrent Python test run during measurement.
The published evidence came from the third capture of the same frozen revision.

## Evidence and replay

- [Lossless audit carrier](76e82620674f4847af4290c572823a27/audit-bundle.tar.gz)
- [Exact manifest](76e82620674f4847af4290c572823a27/manifest.json) and [measurement index](76e82620674f4847af4290c572823a27/measurements.json)
- [Raw samples](76e82620674f4847af4290c572823a27/samples.csv) and [medians](76e82620674f4847af4290c572823a27/medians.csv)
- [Transport record](76e82620674f4847af4290c572823a27/transport.json) and [digest-pinned audit receipt](76e82620674f4847af4290c572823a27/offline-audit.json)

The transport verified 479 expanded files byte for byte. A fresh
extraction passed the pinned relocated audit and its corruption control.
Verify the archive and pin the inventory to the recorded digests from a trusted
checkout of this publication. If you obtain the carrier separately, authenticate
the expected digests through the publisher or a verified signed tag.

The replay prints the receipt to stdout and removes its extraction directory
when the subshell exits.

```sh
(
  set -e
  run=76e82620674f4847af4290c572823a27
  base=docs/validation/offline-audit-signal-followup/$run
  unpacked=$(mktemp -d "${TMPDIR:-/tmp}/codet-offline-audit.XXXXXXXX")
  trap 'rm -rf -- "$unpacked"' EXIT
  archive=$(jq -er .bundles.main.sha256 "$base/transport.json")
  inventory=$(jq -er .bundles.main.inventory_sha256 "$base/transport.json")
  test "$(shasum -a 256 "$base/audit-bundle.tar.gz" | cut -d ' ' -f 1)" = "$archive"
  tar -xzf "$base/audit-bundle.tar.gz" -C "$unpacked"
  /usr/bin/python3 -B scripts/evidence_offline_audit.py \
    "$unpacked/$run" "$unpacked/replayed-audit.json" "$PWD" \
    --expected-inventory-sha256 "$inventory"
)
```

## Earlier capture

PR #78 froze the initial retry implementation at
`c3462978dde91af9f04bdda2d5416c88ebed9659`. Capture
`5b3145242f0b41c5a4d318c810c09014` records that revision.
[Earlier carrier](5b3145242f0b41c5a4d318c810c09014/audit-bundle.tar.gz),
[manifest](5b3145242f0b41c5a4d318c810c09014/manifest.json),
[measurement index](5b3145242f0b41c5a4d318c810c09014/measurements.json),
[raw samples](5b3145242f0b41c5a4d318c810c09014/samples.csv),
[medians](5b3145242f0b41c5a4d318c810c09014/medians.csv),
[transport record](5b3145242f0b41c5a4d318c810c09014/transport.json),
and [offline audit receipt](5b3145242f0b41c5a4d318c810c09014/offline-audit.json).

The two incomplete captures that preceded it remain in the earlier campaign's
local scratch directory. Capture `85deb9b29c19492aad79e901935332a1` hit the
five-second inspector timeout. Capture `32ec67466795414291fc371d617647b4`
detected a concurrent Python test run.
