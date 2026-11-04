# Completed input and interrupt-safe cleanup

This follow-up starts from fetched main `6669219b85d74a28ba935c92d7dc039054535c83` in the existing isolated worktree. It preserves the original checkout.

A successful worker result has one publication path before pending-input waits: update usage, save the requested file, then attempt display. Late quit and EOF exit normally. Late SIGINT/SIGTERM discard follow-ups and return to a usable prompt after worker quiescence. Real formatting, file, and delivery errors remain errors. Active and completed submissions share command, request, origin, and FIFO classification.

Frozen lines retain their safe view until presentation is attempted. A presentation receipt binds final-newline admission to its destination and external boundary. Deferral retains ownership; actual delivery failure completes the best-effort attempt. A successful newline is irrevocable. Unterminated ordinary diagnostic fragments may concatenate with ordinary diagnostics but separate from presentation. No metadata lock covers callbacks or I/O.

Control-only input uses a six-byte prefix plus overflow depth. Backspace consumes overflow first. Saturation cannot be corrected into a false quit before a line boundary. Extra ordinary typing while a submission is frozen remains intentionally discarded.

Owned process cleanup records further interruptions while completing bounded escalation and reaping. It checks group inactivity at intervals of at least 50 ms, retains the leader until group signalling ends, and preserves the original exception and signal handlers.

Cppcheck relocations retain full coordinates and compare non-coordinate identity with multiplicity. Every old/new location has an explicit unchanged-context mapping. Unrelated identities, duplicate mappings, cross-file moves, and incomplete coverage fail.

New hashing payloads use metric version 2 within unchanged evidence-container format v5. `explicit_stat_calls`, `direntry_is_file_calls`, and `direntry_is_dir_calls` are individually profiled Python calls, not total kernel metadata operations. `retained_traced_blocks`, `retained_traced_bytes`, and `peak_traced_bytes` measure live/peak traced memory, not allocation events. Positive controls verify call observations separately from timing. Historical metric version 1 retains its original interpretation; its samples are not relabeled.

## Replay and audit

Provision the pinned dependencies and host tools. Every attempt needs a fresh directory. The concrete manifest captures resolved tools, environment, source identities, command vectors, build outputs, logs, and declared inputs.

```sh
build/lint-venv/bin/python scripts/evidence_campaign.py prepare \
  --root "$PWD" --folder /absolute/fresh/campaign \
  --baseline 6669219b85d74a28ba935c92d7dc039054535c83 \
  --hash-correctness-ref 6669219b85d74a28ba935c92d7dc039054535c83 \
  --check-python build/lint-venv/bin/python \
  --analysis-python build/lint-venv/bin/python \
  --clang-tidy /absolute/clang-tidy --cppcheck /absolute/cppcheck
build/lint-venv/bin/python scripts/validation_evidence.py capture \
  /absolute/fresh/campaign/manifest.json /absolute/fresh/campaign/capture
build/lint-venv/bin/python scripts/validation_evidence.py bundle \
  /absolute/fresh/campaign/manifest.json /absolute/fresh/campaign/capture \
  --output docs/validation/completed-input-cleanup-followup/UNIQUE_RUN_ID
build/lint-venv/bin/python scripts/validation_evidence.py verify-bundle \
  docs/validation/completed-input-cleanup-followup/UNIQUE_RUN_ID
```

The final concrete manifest adds focused completed-input integration repetitions and TSan coverage before capture. Both Release variants compile the same separately declared harness snapshot against their immutable production libraries/headers. Measurements include known/basic frozen submissions, fragments, typing, cleanup, diagnostics, exports, hashing, Git inspection, and supervision. Each has one warm-up and five alternating measured runs per variant in two sets. There are no timing thresholds or claims about total Python allocations/kernel syscalls.

The bundle is an offline audit archive, not a relocatable executable distribution. Known analyzer findings, unsupported platforms, and unavailable native Tiger coverage are reported separately. Existing blocking-TTY and best-effort emergency delivery limitations remain. Historical artifacts are unchanged.

On host Darwin ASan builds, fresh-process test helpers use `posix_spawn` with checked close actions. This avoids a captured Apple sanitizer/Foundation child-side `fork` lock-corruption crash before `exec`. Non-ASan and Tiger-targeted helpers retain the fork/exec implementation. Assertions and allocation traps remain enabled. The first failed capture is preserved separately from acceptance.

The shared tooling harness records `check_output_calls`, actual `git_commands`, and `process_inspection_commands` separately. Version-2 verification checks their classification and memory scopes. These are observed Python command invocations, not all kernel operations. An interrupted pre-measurement campaign is retained after correcting the previously ambiguous shared counter label.
