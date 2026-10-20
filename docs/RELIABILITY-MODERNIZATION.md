# Reliability, persistence and providers

Codet is an unofficial C++17 adaptation of the official Codex CLI. Historical reconstruction notes and attribution describe the separate recovered C target and remain preserved. Codex is the subscription-first default, with its existing model selection unchanged. Its imported-token transport uses private ChatGPT endpoints and Codex-compatible OAuth/header conventions; it is an unofficial compatibility integration, with no claim of guaranteed support or a proven terms violation.

## Behavior changes

Control hooks default to stopping the current turn on execution failure, timeout, invalid decisions or truncated interpreted output. Set `on_error="warn"` on an individual hook to retain diagnostic behavior. Exit 2 and valid vetoes always block, with a fallback explanation. SessionEnd and PostToolUseFailure remain diagnostic-only. A completed effect stays completed even if its PostToolUse hook fails; the result is committed before remaining calls are stopped.

Provider failures expose the entire captured HTTP body and raw streaming error payload, including JSON fields, HTML and multiline data. Transport size limits and retry rules are unchanged. Human output includes raw bytes. Structured errors contain `http_status` and `body`, with ordinary JSON escaping; invalid UTF-8 uses `body_base64` and `body_encoding="base64"`. This deliberately preserves provider diagnostic content without redaction.

Compaction receives an explicit projection of human steering, visible assistant text, calls, outcomes, recovery facts and artifact references. Encrypted reasoning, thinking signatures, image encodings and storage metadata are excluded. Required archives precede the atomic history replacement; failed publication retains the previous generation.

Configuration, credentials and memory remain in Codet home. Invocation state includes plans, hook input, locks, SQLite sidecars, captures and generated artifacts. With `--ephemeral`, all automatic invocation state lives in the temporary directory and is removed after success, error or orderly interruption. Deterministic argument/configuration/prompt/credential checks precede new persistent session rows. Accepted prompts and useful failure/recovery records remain available, and reusable lock files are retained.

A syntactically or structurally invalid saved plan warns and resumes with no active plan. The original stays in place until an update is published, when it moves to a private unique `SESSION.corrupt-UUID.json` recovery file. Security and filesystem failures remain errors.

## API selection and signed history

New API fallback defaults are `gpt-6.1-sol`, `claude-sonnet-5-5` and `gemini-3.8-flash`. Public OpenAI Responses defaults to medium effort for `gpt-6.1-sol` and `gpt-6-astra`; other OpenAI model IDs send reasoning fields only with an explicit effort. Sonnet 5.5 uses medium effort when none is selected. Saved model selections and explicit overrides take precedence.

```toml
[providers.openai]
wire_api = "auto" # auto | responses | chat_completions
```

Auto selects public Responses for the built-in OpenAI endpoint and Chat Completions for custom bases. Select Responses explicitly for compatible custom endpoints. Known Responses-only tool-calling models reject Chat Completions with an actionable preflight error. Responses shares wire encoding/decoding with Codex, while API authentication, URLs and private headers stay separate.

Sonnet 5.5 stores immutable transmitted message prefixes, including empty thinking blocks and signatures. Changing instructions append supported system messages. Account, model, tool-schema or history-projection changes start a fresh epoch at a completed tool boundary, using semantic completed context and preserving image/recovery references. Signed blocks are not replayed against edited prefixes. Text or image-budget rollover also starts a fresh epoch, keeping current image delivery within four images and 8 MiB. Provider conversation state survives resume; a fork can safely start a fresh epoch. Empty
assistant replies retain source bookkeeping but emit no empty assistant wire message. Following
a transition to another Anthropic model, later instruction changes update that model's top-level
system prompt without resetting its new tool-use/result history.

Protocol references: [OpenAI model and Responses support](https://developers.openai.com/api/docs/models/gpt-6.1-sol), [Sonnet signed-history migration](https://platform.claude.com/docs/en/models/sonnet-5-5/migration-guide), and [Gemini Flash](https://ai.google.dev/gemini-api/docs/models/gemini-3.8-flash).

## Schema 7 and restore

Schema 7 stores ordered messages separately from metadata, tool journals and provider conversation parts. Per-session `provider_revisions` tokens rotate through insert, update and delete triggers on provider parts, including writes from external SQLite connections. Missing or altered revision infrastructure fails closed. Append operations serialize changed messages only. Tool completion, its transcript result and associated references commit together. Compaction replaces the history generation atomically after archives exist.

Provider snapshots describe committed rows only. A nested provider write compares its cached
prefix before invalidating the snapshot; only an owning transaction publishes a replacement
after a successful commit. Reads inside explicit transactions do not publish snapshots.
Optional cache publication cannot fail an already committed save: allocation or serialization failures invalidate cache and cursor trust. Recovery may leave the cache empty until the next owning save or load. The single-session
8 MiB cache cap and changed-suffix serialization still apply.

Before migrating an older database, stop active sessions. Codet serializes upgraders on a private, interruptible migration lock, rechecks the version, then checks session ownership and creates a private consistent SQLite backup named `state.sqlite3.pre-v7-UUID`. Valid legacy sessions migrate independently in one transaction. Malformed legacy transcript bytes remain untouched and report an error only when that session is loaded. Migration failures roll back. Older binaries reject schema 7. Already-current WAL databases open without a migration write transaction. The migration lock has no five-second SQLite busy deadline.

To restore, stop every Codet process, copy the current database and its `-wal`/`-shm` files aside, remove those current files, then copy the selected pre-v7 backup to `state.sqlite3` with mode 0600. Keep the artifact and capture directories alongside it. Run the older binary only after restoring its compatible schema. A restore discards sessions and changes newer than the chosen backup; keep the displaced database for recovery.

```toml
durability = "durable" # durable | fast
```

`--durability durable|fast` has the highest configuration precedence. `/status` and `doctor` report the effective mode. Both use [SQLite WAL](https://www.sqlite.org/wal.html). Durable uses FULL synchronization. Fast uses NORMAL for ordinary transcript/result commits, FULL for tool starts and a FULL write barrier at normal turn completion and orderly shutdown. Fast can lose recent completions on power failure. Missing completions are uncertain and never automatically re-executed.

## Capture and build accounting

Each capture store has a rebuildable SQLite catalog under the existing store lock. It records identity, accounted bytes, active/completed state and eviction order. Bytes are reserved before writes; partial or abandoned reservations reconcile conservatively. Ordinary 64 KiB flushes update the active capture without opening every retained file. Startup, namespace changes, a missing catalog, catalog damage and capacity reclamation trigger reconciliation. Corrupt databases and sidecars are validated as owned regular files before removal, with at most one recovery attempt per catalog operation. Other failures remain capture errors, and recovery never repeats output writes. Eviction revalidates identity and ownership locks. The existing 8 MiB per-capture, 64 MiB total and completed-file limits remain, including active-file protection, legacy adoption and symlink defenses.

Connected catalog identity checks use `lstat`. Raw validation descriptors close before
SQLite opens the catalog, and the path identity is checked again before schema work.
This avoids canceling process advisory locks by closing another descriptor of the same inode.

Dependencies build from freshly extracted, hash-verified archives. A recipe/lock/toolchain fingerprint selects a clean stage, and only a completed build replaces the published sysroot. Its manifest records archive, patch, source-tree, recipe and output hashes. Mach-O verification requires all expected libraries, PPC architecture and a resolvable transitive dependency closure under the stage or approved system paths.

Tests use private homes, controlled environments and explicit non-login `/bin/sh` commands; recording fakes cover shell defaults. CTest limits are 300 seconds for units, 180 for integration and 60 for smaller suites. Required provider acceptance uses fixtures, including loopback HTTP, without paid requests.

## Measurement and owner acceptance

Run `make benchmark` for growing transcript appends in both durability modes and repeated capture flushes. The executable asserts serialized append work scales with new messages and ordinary capture flushes do not scan the store. It records wall time, peak RSS and process I/O block counters. These host observations do not establish G4 performance.


The recorded macOS arm64 Debug run used 512-byte payloads:

| Results | Serialized append bytes | Durable elapsed | Fast elapsed |
| --- | ---: | ---: | ---: |
| 100 | 72,292 | 0.024 s | 0.020 s |
| 200 | 144,692 | 0.047 s | 0.037 s |
| 300 | 217,092 | 0.072 s | 0.055 s |

Both modes serialize the same changed-message bytes. One hundred 64 KiB flushes with 40 retained files perform zero store scans. [The source-hashed validation record](validation/reliability-modernization.json) includes memory, WAL size, Linux I/O counters, analyzer coverage and verification details.

The Tiger bundle includes the same benchmark executable. `tiger-acceptance.sh` records both durability runs, capture flushes, `/usr/bin/time -l`, runtime tests and machine details. G4 execution, power-loss testing and live account/provider acceptance remain owner-run checks. See [Tiger acceptance](TIGER-ACCEPTANCE.md) and the [validation record](validation/reliability-modernization.json).

## Capture accounting errors

A close-accounting failure remains visible through `capture_error`, bounded to 1,024 bytes,
without marking fully written output as truncated or overwriting a primary capture write error.
Active capture ownership and byte quotas still apply; capacity reconciliation probes catalogued
active files, preserving live owners and reclaiming abandoned flags. Existing corruption recovery
remains limited to one attempt per top-level catalog operation and never repeats output writes.

Malformed provider caches remain explicit errors during ordinary saves. Explicit `codet repair`
can back up and reset derived wire state or preserve unavailable recovery references as uncertainty.
Genuine immutable archive-identity conflicts remain errors; repair never silently replaces archives.

[PR #16 follow-up validation](validation/pr16-review-fixes.json) records transaction-cache,
terminal-event and catalog-lock regressions, host/sanitizer/Linux tests, scoped safeguards,
benchmarks and Tiger cross-build verification against their source hashes.

## Repair preparation and private restores

Repair inspects result archives and captures using their configured roots, including captures
removed by retention quotas. Inspection, provider-cache repair and recovery repair report the
same pre-repair problems. Recovery repair preserves unavailable results as partial archives and
durable uncertainty; it never rewrites arbitrary user JSON content or repeats historical actions.

A session lease covers the operation. Coherent database reads, archive decoding, private backups
and replacement serialization precede the final write transaction. Recovery preparation carries
file identities captured with the decoded facts, including cache hits; it never establishes a new
identity baseline after decoding. Final recovery publication rechecks each distinct relevant
reference using filesystem metadata and trusted-path validation under the writer lock. These
checks remain proportional to the reference count; archive reads, backup writing and serialization
stay outside that lock. Provider-cache-only repair reports the same preparation-time file issues
but does not gate its database-only mutation on file stability.

A database-wide version check rejects stale preparation in every mutation mode. Repair retries
once, for at most two attempts; overlapping unrelated commits can exhaust both attempts even when
the selected session is unchanged. A second change exits without applying repair changes and
reports all retained backups. The existing `backup` field names the applied attempt; `backups`
lists retained preparation backups when a retry occurs. Operational database read failures abort
preparation immediately, including failures that end the read transaction automatically.

Trusted configured-root aliases (including `/var` and `/private/var`) resolve to the same portable
identifiers without bypassing symlink checks below the anchor. Failed repair and checkpoint
candidates are removed from their actual creation root only while their identity/content matches
and no committed reference uses them; pre-existing archives are preserved.

Restore checkpoint archives as owned regular files with mode `0600` under their original relative
artifact identifiers. Symlinks, foreign owners and group/other permissions remain untrusted;
unavailable archives retain explicit uncertainty rather than inferred historical outcomes.
The recovery archive cache accounts for at most 8 MiB of serialized facts using the decoded archive
text length. This bounds serialized content, not the parsed JSON heap footprint.

[PR #18 follow-up validation](validation/pr18-correctness-repair.json) records committed-response
allocation failures, repair preparation retries, capture-reference recovery, tool error payloads,
the offline test matrix, benchmarks, and Tiger cross-build/static verification against source hashes.

[PR #19 follow-up validation](validation/pr19-correctness-repair.json) records consumed archive
identity checks, root-aware candidate cleanup, optional token failures, operational snapshot errors,
fragmented tool diagnostics, and the offline matrix with source hashes. Tiger verification is static;
no target hardware or live provider execution is claimed.

## PR #20 repair follow-up

Inspection checks each selected session's provider revision token inside its read snapshot, even
when no provider rows remain. Explicit provider-cache or recovery repair backs up the original
rows, clears derived provider state and rotates that token in the same transaction. Invalid token
content is repairable; missing or altered revision infrastructure remains a mandatory integrity
failure. Operational SQLite and allocation failures are not reported as message corruption.

Trusted, available absolute checkpoint archive paths normalize to relative identifiers in loaded
snapshots without writing the database. Successful saves persist the same normalized view used for
reference registration, and forks retain references to shared archives. Missing or unsafe pointers
remain available to historical-uncertainty handling and explicit recovery repair. Trust validation
continues to require owned private regular archives, including on restored or aliased storage.

Candidate cleanup treats an absent unpublished file as already removed and continues after cleanup
or diagnostic failures. Existing, committed, replaced, unsafe and changed archives retain their
protections. Backup publication uses the filesystem already configured by Tools. Known-reference
insertion skips committed paths and publishes optional cache knowledge only after owning commits.

[PR #20 follow-up validation](validation/pr20-correctness-repair.json) records the fault regressions,
portable checkpoint lifecycle, cleanup and insertion tests, offline checks, benchmarks and Tiger
cross-build/static verification against source hashes. Tiger hardware execution remains untested.


## PR #21 checkpoint persistence follow-up

Legacy checkpoint projections carry nonserialized source-row provenance. Missing message IDs are
assigned from the filesystem-independent message view before checkpoint paths are normalized.
Once validated, a mapping stays stable for reconciliation even if its archive disappears or becomes
unsafe. It never grants access to that file: recovery decoding still validates current private
ownership, containment and identity, and reports unavailable history without repeating effects.

Loads and exports remain read-only. The next successful append or revision also rewrites pending
legacy checkpoint rows and registers their portable references in the same transaction. Only those
rows are compared and rewritten; ordinary appends retain suffix-only work. Conflicting source edits
fail without overwriting history. Pending knowledge is acknowledged only after an owning commit;
rollback and externally controlled nested transactions are checked against actual rows on retry.

StateStore accepts a shared filesystem dependency, retaining its native compatibility constructor.
Checkpoint preparation, private repair backups, fork index copying and response receipts use that
dependency. Artifact anchors are acquired on demand; unavailable roots do not block database-only
commands. A successfully acquired anchor remains pinned. Validation is deduplicated during each
preparation, and pending metadata rewrites do not reopen previously observed archives under the
writer lock. New archives and subsequent reads retain strict validation.

[PR #21 follow-up validation](validation/pr21-portable-checkpoints.json) records source-hashed
regressions, the offline matrix and benchmarks. Tiger evidence is cross-build/static verification;
no Tiger hardware or live-provider execution is claimed. Schema 7, provider wire formats, tool
schemas, private archive restoration requirements and the two-attempt repair policy are unchanged.
