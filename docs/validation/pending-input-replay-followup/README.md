# Pending input and campaign replay

This follow-up starts at fetched main `ddfbcc0bd7fa216cc12d36c1b5a1d2059b496344` in an isolated worktree. A frozen submission owns its raw bytes, request identity, originating session/turn and bounded safe view. Output reservations retain that submission until presentation can be attempted. Diagnostic completion or geometry changes invalidate its prior display progress. Successful turns transfer ordinary held messages once; cancellation, failure and quitting discard them before cancellation resets. Keyboard controls remain serviced while output is reserved.

New evidence uses format v5. It records the relationship between the configured root, cwd specification and bounded symlink-resolution chains before and after each command. Publication rechecks current resolution; offline auditing uses recorded relationships without looking up original checkout paths. Historical bundles retain their original format limitations and cannot establish v5 acceptance.

Analyzer relocations may change locations only: tool/check identity, message, root-relative file and translation unit must remain the same. Compile consumers declare their pinned dependency roots and configuration files; binary-only consumers verify the producing build's executable/library outputs. No dependency hashes are reused across command boundaries.

Public CI is read without credentials through GitHub REST, with run/head/attempt identity checks, paginated job coverage, bounded requests and polling. A historical run can exercise the reader but cannot validate changed source. Python 3.9 is discovered by a version probe unless explicitly overridden. Platform trust defaults apply unless an existing CA bundle is explicitly selected and hashed. Owned subprocess groups receive bounded termination, escalation and reaping on timeouts or interruption.

The hashing experiment compares the mandatory-correctness implementation at `c692848` with the candidate on the same data. It retains one warm-up and five alternating measurements in each of two sets, allocation controls, profiler records, hashes and recomputed summaries. The candidate reuses a 64 KiB read buffer only within a logical snapshot and reduces repeated role/metadata work. Each final bundle contains a fresh experiment, not just the development samples. There are no CI timing thresholds.

## Replay

Provision the repository's pinned dependencies and host tools first. Choose a new campaign directory for every attempt; interrupted attempts are historical and are never reused. An external campaign directory is supported when local disk space is limited.

```sh
build/lint-venv/bin/python scripts/evidence_campaign.py prepare \
  --root "$PWD" --folder /absolute/fresh/campaign \
  --baseline ddfbcc0bd7fa216cc12d36c1b5a1d2059b496344 \
  --hash-correctness-ref c692848 \
  --check-python build/lint-venv/bin/python \
  --analysis-python build/lint-venv/bin/python \
  --clang-tidy /absolute/clang-tidy --cppcheck /absolute/cppcheck
build/lint-venv/bin/python scripts/validation_evidence.py capture \
  /absolute/fresh/campaign/manifest.json /absolute/fresh/campaign/capture
build/lint-venv/bin/python scripts/validation_evidence.py bundle \
  /absolute/fresh/campaign/manifest.json /absolute/fresh/campaign/capture \
  --output docs/validation/pending-input-replay-followup/UNIQUE_RUN_ID
build/lint-venv/bin/python scripts/validation_evidence.py verify-bundle \
  docs/validation/pending-input-replay-followup/UNIQUE_RUN_ID
```

Optional `--python39 /absolute/python3.9` takes precedence over discovery. `--ca-bundle /absolute/certificates.pem` requires a real file and records its digest. `--ci-run RUN_ID` accepts only public CI matching the frozen source commit. The concrete manifest records resolved tools, environment, commands and paths; ambient credentials and compiler settings are not inherited.

The bundle is an audit archive, not a relocatable executable distribution. Verification checks archived inputs, producer links, outcomes, inventories and measurements without rerunning binaries. Native Tiger coverage, unavailable container runtimes and known analyzer findings must be reported separately from passing checks. Blocking TTY delivery and best-effort emergency-output limitations remain unchanged. Historical reports, archived sources and samples are preserved.
