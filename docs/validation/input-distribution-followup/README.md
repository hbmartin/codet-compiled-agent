# Input ordering and Tiger distribution follow-up

Baseline: `a6b4164f84de4c8a3826cc77d3cd4e56613bc4f8`. Frozen implementation, tests, and producers: `5b2962be909fbaa88f533d838d802c6ab150356b`.

The [published attempt](20261105-9c5b92655b36/README.md) is **incomplete**. Its 39 original check records contain 35 passes, two completed analyzer observations with known findings, a failed hashing quiet-window check, and a blocked Release measurement check. Another live Python unittest process was detected. The user authorized delivery with benchmarks explicitly unavailable. There are **zero measurement samples** and no performance claim. The required outcomes and policies are preserved; [strict publication rejected this run](strict-publication-rejection.log). This is not a passing full campaign.

Runtime changes use one ordered admission queue for unread steering, reserved frozen lines and follow-ups, retaining the 100-entry and 65,536-byte limits. Built-in local commands receive idle-use guidance during a turn, `/new` remains deferred, and custom or unknown commands reach idle expansion/rejection. Bound question/approval answers retain non-quit slash bytes. Extra typing while a line is frozen is deliberately discarded with a notice and retains ownership through its next line boundary. A partial quit prefix does not gate independent frozen-line handoff. Empty frozen lines refresh receipts after destination replacement. Empty successful diagnostics do not open a fragment.

TLS trust selection checks all four origin/proxy CA settings and disables their default directory paths. Permanent certificate and CA-file failures retain their original error and skip transport retries. TLS fixture handshakes run on connection threads and cleanup preserves the original failure.

Tiger packaging requires a new successful source-bound build and its static verification. Both variants consume frozen documentation, notices, configuration and source snapshots. Required GPL, GCC Runtime Library Exception, MPL and dependency notices fail closed. Distribution CA data must match the pinned Mozilla digest and every certificate must parse; runtime custom CA selection is retained. The launcher preserves explicit terminal/trust settings and follows symlinks. Archives normalize timestamps/ownership and preserve previous archive/sidecar outputs on publication failure. Host fixtures do not establish target runtime acceptance.

Captured validation includes:

- Full Debug and ASan/UBSan CTest. Sanitizer isolation records all 677 enabled cases, zero failures and two opt-in skips; the disabled failure-printer probe remains disabled.
- TSan and Debug focuses: 107 cases per iteration, twenty iterations each.
- Verified Python 3.9.6 tooling and integration, pinned lint, Semgrep and runner tests.
- Host cross-selection compilation/linking and selected tests, explicitly separate from PPC execution.
- Complete baseline/change analyzer coverage over 25 sources: 147 Clang-Tidy findings, zero Cppcheck findings and one Clang analyzer finding on each variant. The comparison gate passed with 44 source-bound coordinate relocations. Analyzer nonzero outcomes remain observations.
- 28 Python, 23 native/integration and two production-site Semgrep negative controls, with passing baselines and intended assertion failures.
- Identical Release harness compilation against both immutable production variants. Quiet-window eligibility prevented measurement.

[The report](20261105-9c5b92655b36/report.json), [manifest](20261105-9c5b92655b36/manifest.json), [capture records](20261105-9c5b92655b36/capture), [archived input index](20261105-9c5b92655b36/inputs.json) and [bound artifacts](20261105-9c5b92655b36/artifacts) retain the exact commands, environments, actual outcomes, configurations, source identities, executable references and logs. External executables/dependency installations are identified rather than bundled. Original checkout paths are opaque provenance fields.

Audit with Python 3.12 or later, including after moving the directory:

```sh
python3.12 docs/validation/input-distribution-followup/20261105-9c5b92655b36/verify-attempt.py \
  docs/validation/input-distribution-followup/20261105-9c5b92655b36
```

Exit zero verifies this attempt's inventory, archived content, source/producer links and the narrowly identified measurement exception. It never grants campaign acceptance: `eligible_current` and `complete` remain false. [Post-publication audits](offline-audits.json) also exercise corrupted inventory/content, forged required outcomes, changed source identities and immutable destination rejection. These assembly/audit records are explicitly post-capture integrity evidence, not replacement capture checks.

Replay from a fresh checkout with provisioned pinned tools and a fresh folder:

```sh
python3.12 scripts/evidence_campaign.py prepare --root "$PWD" \
  --folder build/input-distribution-replay-NEW \
  --baseline a6b4164f84de4c8a3826cc77d3cd4e56613bc4f8 \
  --check-python /absolute/path/to/python3.12 \
  --analysis-python /absolute/path/to/python3.12 \
  --python39 /absolute/path/to/verified/python3.9 \
  --clang-tidy /absolute/path/to/clang-tidy
python3.12 scripts/validation_evidence.py capture \
  build/input-distribution-replay-NEW/manifest.json \
  build/input-distribution-replay-NEW/capture
```

Docker, Podman, the PPC compiler, native Tiger and local Ubuntu runtime coverage were unavailable. No Tiger package was produced or claimed verified. CMake's explicit isolation command avoids the 3.29-only launcher property; only installed CMake 4.4.4 was exercised. Existing Apple SDK compiler-warning accommodations remain recorded. The system Python 3.9.6 has an existing dependency-extraction API limitation; supported Python 3.12+ provisioning/full tests passed, and the specified 3.9 tooling/integration checks passed.

An earlier worktree disappeared during capture. Its commits survived, but the deleted captures were not reconstructed or used for acceptance; [the incident note](incident.txt) identifies that gap. Persistent recovery attempts with setup failures/interruption remain explanatory history inside the bound history artifact. Historical tracked reports, samples, manifests and archived blobs remain unchanged. The original checkout's tracked files were left untouched.
