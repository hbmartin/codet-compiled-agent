# Modern Codet

Codet is a C++17, single-agent terminal coding assistant targeting Mac OS X 10.4 PowerPC. Its interface follows a documented subset of Codex CLI conventions. It is a separate implementation; it does not read `~/.codex` or claim complete compatibility with official Codex.

The recovered C implementation remains available through `make legacy`. The modern CMake source list does not compile its RAG, SQL tools, routing, subagents, MCP, or recovery storage. Historical recovery evidence is retained.

## Build and test

Host prerequisites: CMake 3.20+, a C++17 compiler, Python 3.12+, curl 7.85+ development headers/library, OpenSSL, SQLite 3.24+, ncurses, and libedit development packages. Python is a development dependency, not a target application dependency.

```sh
make fetch-deps       # Explicit network step; SHA-256 verified
make test-foundation  # GoogleTest/GoogleMock and C text-helper regression tests
make test            # Build modern host executable and run all offline tests
make test-sanitize   # AddressSanitizer + UndefinedBehaviorSanitizer
```

The modern host executable is `build/host/codet`. Dependencies are source-pinned in `dependencies.lock.json`; CMake never downloads packages. `python3 scripts/fetch-deps.py --offline` reconstructs source trees using verified cached archives.

The Tiger build uses the existing pinned GCC 14.2/10.4u SDK image, with build utilities added in `cmake/Dockerfile.tiger`:

```sh
make fetch-tiger-deps # Explicitly fetch runtime sources and TigerPorts patches
make toolchain-image # Network-enabled build-tool installation, once
make tiger-sysroot   # Offline runtime dependency cross-build; can take several minutes
make                 # Default: modern PPC executable and GoogleTest test executable
make verify-tiger    # Static verification; never runs PPC code
make tiger-package   # build/dist/codet-tiger-ppc.tar.gz
```

Runtime libraries use the logical `/opt/tigr` prefix. Set `TIGERPORTS_PREFIX` consistently for `tiger-sysroot`, `tiger`, and `tiger-package` to use a different target installation. Header/library staging remains under `build/tiger-sysroot`; production code must never reference `/project` or compiler-container library paths. Compiler C++/GCC support is linked statically. Third-party C libraries remain dynamic TigerPorts prerequisites, including `curl-ca-bundle` for TLS trust.

Runtime sources and patches are locked to the TigerPorts revision in `tiger-dependencies.lock.json`. The sysroot manifest records versions, hashes, flags, installation prefix, and the recipe hash. OpenSSL uses statically linked GCC atomic support for 64-bit operations on PPC32. Curl uses the staged modern zlib, not Tiger's old system zlib. Builds record development-image package versions; target runtime verification is separate from reproducible cross-linking.

## Command contract

```sh
codet --ui line
codet --ui curses
codet exec 'explain this repository'
printf '%s\n' 'explain this repository' | codet exec
codet exec --json 'inspect this code'
codet exec --output-format json --output-schema answer.schema.json 'answer the question'
codet exec -o answer.txt 'inspect this code'
codet resume SESSION_ID
codet exec resume --last 'continue'
codet exec fork SESSION_ID       # Creates a branch without invoking the model
codet sessions
codet export SESSION_ID --output-format json
codet memory consolidate
codet doctor
```

No subcommand starts the interactive UI when stdin/stdout are terminals, otherwise it uses headless operation. `exec` always runs headlessly. A missing prompt or `-` reads stdin; a prompt plus piped stdin combines both. Input is bounded to 1 MiB. Headless operation never asks for approval. Background shell jobs live only for the current Codet process and are stopped when it exits.

`-p/--profile` selects a configuration profile, not a provider. Use `--provider`. `-m`, `-C`, `-c key=value`, `--add-dir`, `--allow`, `--deny`, `--max-turns`, `--ephemeral`, and `--color` are supported. `--skip-git-repo-check` is accepted because Codet does not require a Git checkout. OS sandbox flags are rejected explicitly. Actual `--help`/`-h` and `--version` flags take precedence over other invocation errors; help wins if both occur. Option values still consume flag-looking words (`--model --help` selects that model), and `--` makes later tokens literal prompt text.

Text output sends only the final assistant answer to stdout. JSON output is one Codet result object with session ID, status, result, structured output, and token usage. JSONL is a versioned compatibility subset with `thread.started`, `turn.started`, `item.started`, `item.updated`, `item.completed`, `turn.completed`, and `turn.failed`. Items cover assistant messages, commands, file changes, web search, and `codet.tool`. Partial text uses the documented `text_delta` extension. `codet.compacted` reports context compaction. This is not the app-server JSON-RPC protocol. Progress and diagnostics go to stderr.

The v1 event fixture is `tests/fixtures/events-v1.jsonl`; integration tests compare its complete sequence and payloads after normalizing only generated IDs and the temporary workspace. `thread.started` carries `thread_id`; item events carry `item` with a stable per-item `id` and `type`. Assistant items contain `text` or `text_delta`. Tool items contain `tool`, `status` (`in_progress`, `completed`, or `failed`), then `output` and `metadata` on completion. Command items also contain `command`. `turn.completed` contains aggregate `usage` token counts; `turn.failed` contains `error.message`. Consumers must tolerate extra fields and the documented `codet.*` extensions. These event names and fields form the frozen subset; no exhaustive Codex compatibility is claimed.

Exit codes: 0 success; 1 execution failure; 2 invalid invocation/configuration/schema; 130 interruption. A shell command failure that the model handles does not itself force a failed overall run. `--output-schema` accepts object schemas using Draft 7 and local references only; external schema fetching is disabled. The model must produce a validated `StructuredOutput` call. `--output-last-message` independently writes the final answer.

## Configuration and credentials

State lives under `CODET_HOME`, otherwise `~/.codet`. Main settings are in `config.toml`; `-p work` overlays `work.config.toml`. Explicit command-line overrides win. Trusted project `.codet/config.toml` files load from the project root toward the current directory. A project must be listed in the global `trusted_projects` array before its executable hooks/configuration are loaded.

```toml
provider = "openai" # codex, anthropic, google, ollama also supported
model = "your-model-id"
permission_mode = "default" # plan, accept-edits, bypass
approval_policy = "on-request" # or never
ui = "auto" # curses or line
max_turns = 100
max_output_tokens = 8192
context_tokens = 65536
tool_output_limit = 65536
connect_timeout_seconds = 20
stream_idle_timeout_seconds = 120
request_timeout_seconds = 0 # explicit total cap; 0 disables it
project_doc_max_bytes = 32768
memory_context_max_bytes = 8900
trusted_projects = ["/path/to/trusted/project"]
allow = ["exec_command(make test*)"]
deny = ["exec_command(rm*)"]

[providers.openai]
api_key = "{env:OPENAI_API_KEY}"
base_url = "https://api.openai.com/v1"

[keybindings]
submit = 10
quit = 4
backspace = 127
```

Provider streams have no implicit total timeout. The idle timer includes waiting for the first response bytes and resets on received body bytes, including streaming keepalives. Connection and cancellation checks are independent. An explicitly configured positive `request_timeout_seconds` remains a total request limit; nonstreaming web and credential operations retain finite limits. Partial model streams are not automatically replayed after transport failure. Google parameterless tools omit optional parameters. Every adapter omits an empty optional top-level tools array, including compaction requests. This is compatibility hardening, not evidence of a verified live API rejection.

Credentials resolve from provider configuration, environment, then the private Codet key store. Supported variables are `OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `GOOGLE_API_KEY`/`GOOGLE_GENERATIVE_AI_API_KEY`, and optional `OLLAMA_API_KEY`. Ollama defaults to its native `/api/chat` endpoint. URLs are provider API bases; Codex's override is the complete Responses endpoint. Reasoning effort is passed to Codex/OpenAI and Anthropic; unsupported Google effort overrides fail explicitly. Model defaults are fallbacks, not account-availability guarantees; use `--model` for the model available to your account.

```sh
codet login --import /explicit/path/to/auth.json
codet login --import /explicit/path/to/claurst-codex-tokens.json
printf '%s' "$OPENAI_API_KEY" | codet login --with-api-key --provider openai
codet login status
```

Import supports the `tokens` object in a Codex auth file or Claurst's flat Codex-token object. Source files are never modified. Native sign-in and account switching are deferred. Refresh is serialized with a cross-process lock and atomically persists rotated credentials. Failed refresh asks for fresh import; it does not downgrade to an API key silently. Use `--provider openai` or another retained API-key provider explicitly for fallback. Subscription endpoint compatibility is not a public API guarantee and requires target acceptance.

Session IDs are UUIDs. SQLite schema versions migrate forward; newer unknown versions are rejected. Completed tool results are recorded before the next model request. Recovery places exactly one result immediately after each assistant tool-call batch, preserving completed journal results and distinguishing never-started calls from uncertain effects. It repairs live interrupted turns, older misplaced outcomes, resume and fork without re-executing tools. Snapshot loads/exports and fork parents are repaired in memory without modifying live journals. Persisted recovery requires a private per-session advisory ownership lock; concurrent resume is rejected, and the owning invocation keeps its lock through cleanup. Runs reload current state after acquiring ownership. Forks freeze a repaired snapshot into a new session and do not replay parent calls. Repairs are transactional and idempotent; ambiguous IDs and corrupt completed records fail explicitly. Invalid UTF-8 in diagnostic/result text is replaced at serialization boundaries, and unexpected turn errors do not terminate the REPL. For fresh, resumed and forked sessions, one resolver applies provider/model selection uses explicit CLI fields, then fields present in the selected profile, then the stored session selection, then configured defaults. Unrelated settings do not change selection. A model-only override retains the saved provider. Changing provider without an explicit model selects its configured model or built-in fallback. A matching top-level configured provider/model pair takes precedence over its nested provider model; selecting the same provider retains its saved model. `model_provider` is normalized once as an alias for `provider`; `provider` is authoritative when both appear in the same layer. `--ephemeral` uses a private invocation directory for the session database, all automatic output captures, and automatic image artifacts. Configuration, credentials and readable memory still use `CODET_HOME`; requested file changes, memory updates and credential operations remain persistent. Private read_file paging works in every mode for referenced captures inside the active temporary capture root. Image replay validates the active artifact root. Cleanup runs SessionEnd, stops owned processes, closes captures, then removes the temporary directory, reporting cleanup errors without changing the invocation result.

## Tools, permissions, and customization

See [Codex tool contracts and native Tiger computer use](CODEX-TOOLS.md) for the complete interface, permission behavior, image budgets, asynchronous interaction, compatibility break and native APIs. The model-facing surface uses exec_command/write_stdin, apply_patch, view_image, plans/questions and clock tools, plus a capability-gated computer namespace. read_file is available in every mode for narrowly scoped private context; ordinary workspace reads remain Plan-only. Legacy utility/edit/task names and aliases are removed.

The agent uses embedded per-model Codex instructions with Tiger-specific capabilities and
authorization facts. See [captured prompt provenance, adaptations and compaction](PROMPTS.md).
Small context settings must accommodate the full fixed prompt and tools; insufficient space
produces a diagnostic instead of silently truncating instructions or repeatedly compacting.
Admission includes request overhead and the configured output reserve. Diagnostics distinguish
fixed context, oversized human input and oversized summaries and show the estimated minimum
`context_tokens`. The default is now 65536; max_output_tokens remains 8192 and tool_output_limit remains 65536. Task-plan context refreshes each request without reloading project documents. The view is at most 8 KiB, prioritizes active then pending then completed steps, retains original step numbers and names the full saved file. New serialized plans above 1 MiB are rejected before state changes; larger legacy plans remain available through paging.
Tool previews are bounded once per batch and persisted; later requests keep already-sent previews unchanged until compaction. Full outcomes remain in the tool journal, with complete private archives under artifacts/results/SESSION/MESSAGE.json before reduction. File reads can be projected like other results. Projections retain direct capture references, action/recovery status, paging controls and bounded capture/post-action diagnostics.

Automatic compaction starts at 80% of the usable history budget after fixed instructions, tools and output reserves. It retires older history when possible; if the newest completed batch alone cannot fit, the whole call/result group and associated replay are replaced with a bounded checkpoint. Latest human steering, unresolved recovery outcomes and execution-session IDs remain explicit. Full records are archived and indexed before publication; cancellation, provider errors and failed publication retain durable history. Already-compacted continuations may use the remaining hard budget without repeated compaction.

Summary generation retains the configured output allowance and an 8 KiB visible-prose maximum. The conservative escaping-derived target is advisory: summaries are accepted using actual serialized continuation size. A genuinely oversized summary gets one smaller-target retry, counted in usage and request limits, with one PreCompact hook invocation. Empty or exhausted responses fail without replacing history. Manual compaction shares these rules and does nothing when no history can be reduced.

SQLite schema 5 keeps the existing transcript row and adds a per-session registry of portable capture/result references. Legacy structured transcripts and journal results backfill it; summary prose never grants access. Forks inherit references, and active storage roots resolve them after relocation. A paged indexes/SESSION.results.jsonl view is rebuilt from SQLite and exposed through read_file in every mode. Normal capture eviction remains effective. Ordinary saves no longer copy and normalize the entire history; whole-transcript serialization and SQLite writes remain, so cumulative save work is not claimed to be linear. The optional codet_persistence_bench target measures 100/200/300-result histories with configurable payload size.

Every image occurrence is archived in a versioned JSONL observation ledger before history replacement; compacted continuations are text-only, including the newest screenshot. Subsequent image input requires explicit reload.

Active-turn steering retains its classification across completion, so `/memory consolidate`
accepted as steering remains model text in its follow-up turn. Explicit active `/quit`, `/new`
and `/permissions` controls retain their behavior. Worker events and approval/question requests
have turn ownership; error cleanup drains diagnostics and outcomes and expires unanswered
requests before another turn. An expired request cannot consume the next line of input.

Instructions select AGENTS.override.md, AGENTS.md, or fallback CLAUDE.md per directory, retaining bounded includes. Skills list names, descriptions and exact SKILL.md paths for lazy inspection. Markdown memory previews are bounded separately; explicit memory consolidation uses patches. Session/credential/permission controls remain application controls without an OS shell sandbox.

Path rules are tool-specific and match normalized lexical/canonical aliases and rule-authored globs, resolving relative spellings and fixed symlink prefixes. Workspace and canonical filesystem prefixes are literal, including `[`, `*`, `?` and backslashes in workspace names. Parsed rule syntax is cached until allow/deny configuration changes; filesystem resolution is shared within one decision and refreshed for every operation. Leading wildcard rules such as `read_file(*secret*)` also cover matching paths outside the working directory, including added roots and bypass mode. Directory listings check each returned entry; a read_file rule does not restrict an approved exec_command shell command. Denials are checked before hooks and after input rewriting.

Instructions load Codet-home guidance first, then repository root to working directory. Each directory selects one file in order: `AGENTS.override.md`, `AGENTS.md`, `CLAUDE.md`. This replaces combined AGENTS/CLAUDE loading. Documents retain with bounded/cycle-safe `@include path` and `@path`. Includes stay inside the repository or Codet home. The complete Codet instruction chain (global/project/includes) uses `project_doc_max_bytes`, default 32 KiB, with UTF-8-safe truncation and stderr diagnostics rather than failing on oversized files. Automatic memory previews use a separate 8,900-byte default budget, deterministic file order and at most 200 files; source paths allow explicit retrieval through the active inspection tools. Both budgets allow 0 to disable loading and values up to 1 MiB. read_file streams bounded pages; byte_offset/byte_limit support long lines without loading the whole file. Source files are never truncated or changed. Skills are discovered in `.codet/skills`, `.agents/skills`, and `.claude/skills` from the working directory back to the repository root, then Codet home's `skills` and user `~/.agents/skills`. Nearest project names win; within a directory Codet precedes agents, then Claude. Entries are sorted, canonical paths deduplicated, and symlink roots/entries/files excluded. There is no recursive crawl. Discovery examines at most 100 skill documents with an 8 KiB metadata-read limit each and a separate 16 KiB catalog budget. Plain, quoted and literal/folded scalar frontmatter names/descriptions are supported; unsupported metadata falls back to the directory name with a diagnostic. Only the catalog is automatically included; bodies and support files load lazily. Advertised skills and their support files, memory Markdown, the active saved plan, the active image index and referenced captures/results can be inspected with read_file in every mode, including headless runs without shell approval. Other workspace read_file operations remain Plan-only, even in bypass mode. The private exceptions never override explicit denies. Fresh path classification follows hooks and callbacks; canonical containment and symlink checks are repeated immediately before reading through the validated open handle. Durable Markdown memory uses read_file and patches; consolidation runs only through an explicit request.

Custom prompt commands live in global `commands/NAME.md` or trusted project `.codet/commands/NAME.md`; `$ARGUMENTS` is textual substitution, never shell evaluation. Interactive built-ins include `/help`, `/quit`, `/sessions`, `/new`, `/name`, `/export`, `/permissions [MODE]`, and `/memory consolidate`. `/permissions` shows the active mode; `/permissions default|plan|accept-edits|bypass` changes it immediately for the current invocation. It does not write configuration. Both terminal modes keep the permission mode visible separately from progress. Streaming progress clears on success, failure and interruption.

`request_user_input` provides synchronous Plan-mode questions. Both terminal interfaces also accept asynchronous questions and incoming messages while the agent works; headless tools requiring answers are unavailable. See `CODEX-TOOLS.md` for the question IDs, answer structure and delivery boundaries.

Hooks are trusted executable configuration. Supported events: `SessionStart`, `SessionEnd`, `UserPromptSubmit`, `PreToolUse`, `PostToolUse`, `PostToolUseFailure`, `PermissionRequest`, `PreCompact`, and `Stop`. Each event remains an array of entries with `command`, optional glob `matcher`, and `timeout_ms`. Input is JSON on stdin. Hook capture gives stdout and stderr each `tool_output_limit` bytes (64 KiB each by default, 128 KiB combined retained prefixes). Both channels are fully drained and keep independent truncation flags. Successful stdout truncation is ignored for SessionEnd and PostToolUseFailure because those events do not interpret stdout; stderr truncation remains a warning. Truncated JSON stdout is a hook failure diagnostic and cannot supply a decision. Retained nonempty stderr from exit 2 can still block supported events. Ordinary shell and PTY output remains combined.

Hook interpretation follows the supplied Codex CLI behavior within this event subset:

| Event | Successful output and explicit control |
|---|---|
| SessionStart | Plain stdout or `hookSpecificOutput.additionalContext` becomes persisted context; JSON `continue=false` stops startup. |
| UserPromptSubmit | Plain stdout/context becomes persisted context; `decision="block"` with a reason or `continue=false` stops the prompt. |
| PreToolUse | Plain stdout is ignored. A reasoned deny blocks; an allow may supply object `updatedInput`, which is reauthorized. |
| PermissionRequest | A valid `hookSpecificOutput.decision` with `behavior="allow"` or `"deny"` answers approval; Codet’s previous `permissionDecision` spelling remains an alias. Failures never grant approval. |
| PostToolUse | Context and explicit blocking feedback are returned with the completed action; `action_completed=true` prevents confusing hook feedback with an unexecuted action. `continue=false` journals that outcome, halts the turn and reconciles remaining calls. |
| Stop | Reasoned blocking feedback returns to the agent loop, with the existing three-block guard; `continue=false` ends processing without another model request. |
| PreCompact | JSON `continue=false` stops the turn; ordinary exit errors are diagnostics. |
| SessionEnd / PostToolUseFailure | Diagnostic-only; no control effects. |

Exit 2 with nonempty stderr blocks only PreToolUse, PermissionRequest, PostToolUse, UserPromptSubmit and Stop. Other failures, timeouts, and malformed JSON-looking stdout produce bounded stderr diagnostics and no implicit blocking decision. SessionStart/UserPromptSubmit plaintext is accepted; malformed JSON-looking text is not injected as instructions. Cancellation still interrupts. Denials and plan/root restrictions cannot be overridden.

SessionStart input includes `source`: `startup`, `resume` or `fork`. Only SessionStart matches a nonempty glob `matcher` against that source; absent/empty matchers select every start. Tool-event matchers still select tool names. Each invocation appends its newly accepted context in order, even when identical to earlier context; resume/fork inherit history without redispatching its original startup hook.

Invocation lifecycle hooks run for headless and interactive agent use, including promptless fork. Once SessionStart is attempted, SessionEnd is attempted exactly once with the same captured session ID, including after failure or interruption. `/new` does not introduce another lifecycle pair. End-hook cleanup uses a separate cancellation context and a ten-second total budget; failures include their diagnostics on stderr without replacing the run result. Curses is restored before buffered cleanup diagnostics are printed. During the invocation, hook, instruction, recovery and process diagnostics use the terminal transcript in curses and stderr in line/headless operation. Curses also retains a bounded live-diagnostic replay buffer. After restoration, it prints live diagnostics followed by SessionEnd diagnostics exactly once, on success, failure and interruption. Each buffer has its own `tool_output_limit` budget, including labels, newlines and an explicit UTF-8-safe omission notice. SessionEnd delegates its diagnostic budget to the shared cleanup collector instead of independently capping messages twice. Successful stderr produces a warning; only actual hook failure is labeled "cleanup failed". No persistent diagnostic log or raw-export interface is added.

## Verification boundary

Host tests use fixtures, temporary files, controlled subprocesses, PTYs, and a loopback HTTP server. They do not use real credentials or paid requests. The PPC test executable uses the same GoogleTest/GoogleMock sources and compiler as the application. See `TIGER-ACCEPTANCE.md` for the owner-run target procedure. Passing a cross-build is not evidence that Tiger, subscription access, terminal behavior, or live provider APIs work at runtime.

`make analyze-modern` runs Clang-Tidy, Cppcheck and Clang Static Analyzer using a separate `build/modern-checks` compilation database, with reports under `build/modern-checks/analysis`. It requires Clang (`MODERN_CC` and `MODERN_CXX` select its drivers; `CODET_CLANGXX` remains an alias for the C++ driver) and does not reuse or change the host GCC build cache. See [modern analysis setup and enforcement](MODERN-ANALYSIS.md). The complete host developer image includes Clang/Tidy 18, pinned Cppcheck 2.22.0 and Python prerequisites; build it from the repository root with `docker build -f cmake/Dockerfile.host-tests -t codet-host-tests:local .`. Analysis targets configure only; application/test builds remain explicit.

Process jobs establish a private process group before execution. Darwin latches its one-shot kqueue exit observation independently of cleanup; Linux uses non-reaping waitid observation. Descendant cleanup occurs before reaping the leader, and repeated stop/destruction of completed jobs sends no signals. Startup uses an absolute command deadline and a five-second setup safety bound; expiration never releases the child to execute. Command timeout remains tool/process status 124, including timeout during startup, while genuine setup failures are separate errors. Interrupted handshake operations retry, cancellation retains status 130, and child commands reset inherited signal dispositions (including SIGHUP) and clear the signal mask. Confirmed ownership loss immediately invalidates signaling. Completion is observed before forced termination, preserving already-exited status; expiry before stop or during its grace period records 124. Job-specific observation/cleanup failures are reported with the handle and do not prevent unrelated starts while capacity remains. Failed cleanup retains ownership and counts toward the 64-active-job limit until reaping or confirmed ownership loss. Shell sessions use numeric model-facing IDs and explicit unknown/completed-session errors. Failed internal process cleanup stays private and continues to count against active capacity until reaping succeeds; completed retained processes are bounded to 64 records and 4 MiB. Successful foreground commands and hooks remove their records after collection. Stop and shutdown attempt termination even if observation or capture fails while ownership remains valid. Already-observed completion wins; timeout status 124 and interruption 130 remain unchanged. Job-local standard exceptions are isolated with their original causes; cancellation, allocation and typed native process resource failures propagate during normal operations. Cleanup signal/reap failures retain ownership for retry; shutdown attempts every owned job and reports failures. Managed jobs are intended to end with Codet; these process-group controls do not confine commands that deliberately escape their group.

### Retained command output and portable search

Shell, PTY and non-shell automatic output captures share a private managed store under `CODET_HOME/tool-output`, or the temporary invocation directory under `--ephemeral`. Capture begins only when output would be omitted from the final preview, including UTF-8 replacement expansion or hook feedback. Small complete results create no capture file and cannot evict older captures. The initial prefix is transferred exactly once; files preserve original bytes. New captures retain the first 8 MiB each. The coordinated store permits 64 MiB total and at most 64 completed files; active captures are locked against eviction and oldest completed files are evicted first. Existing regular `capture-*.txt` and legacy UUID `.txt` files count toward the same limits without renaming or rewriting their contents. Legacy per-file sizes are grandfathered, with their full size charged to the store. Unreadable regular files are conservatively counted and skipped for eviction. Symlinks and unexpected directories are diagnosed and skipped without following or recursively removing them. Store-wide failures remain capture failures.

Writes stage in bounded 64 KiB batches, coordinating quota checks and writes under the cross-process lock per flush. Explicit write_stdin publication and completion flush partial batches; internal process observation does not rebuild previews or flush unchanged output. The exec wait loop uses status-only observations and publishes one raw snapshot for final tool rendering. Incomplete live UTF-8 remains deferred without false omission counts or capture notices; accumulation uses incremental display accounting. Created captures receive one completion sync. Completed jobs retain only bounded raw windows, with actual retained buffer capacities charged against the 4 MiB/64-record history budget. Hook stdout/stderr capture stays separate and bounded.

Previews select UTF-8-safe head/tail windows from original bytes after hooks, budget complete notices before choosing windows, and state the number of **original output bytes** omitted. Markers, notices and hook feedback are excluded from that count. Invalid source bytes are repaired for display while captures keep the original bytes. A long path uses a complete `metadata.spill_path` reference notice rather than a clipped path. Metadata meanings:

| Field | Meaning |
|---|---|
| `total_output_bytes` | Original command output bytes, or the non-shell tool's primary result bytes before display repair/annotations. |
| `omitted_output_bytes` | Original primary output bytes omitted from this displayed result. |
| `truncated` | True exactly when `omitted_output_bytes` is positive; capture warnings alone do not set it. |
| `spill_path` | Published only after capture creation and retention of data; may later expire through normal eviction. |
| `spill_bytes` | Retained prefix bytes, bounded by this snapshot's total. |
| `spill_truncated` | Capture failed to retain all bytes in this snapshot. Independent of preview truncation. |
| `capture_error` | Storage diagnostic; preserves actual command status and otherwise complete output. |
| `hook_feedback_truncated` | Hook annotation exceeded its separate display allowance. |

If capacity or storage fails, Codet keeps draining and preserves the real command status. Warnings use metadata and the diagnostic callback, without shortening a complete primary result or encouraging command replay. An omitted result includes a complete capture notice or an unavailable-capture notice. Compaction uses the same bounded UTF-8-safe selection, including its omission marker in the request budget; malformed provider JSON is still rejected.

Use read_file in every mode for referenced retained text. File contents stay verbatim through save/resume/fork even when they contain complete JSON objects. Provider adapters render page text followed by a compact <codet_result> metadata trailer instead of JSON-escaping the full page. Line offset/limit are one-based; optional byte_offset/byte_limit are zero-based and mutually exclusive with line paging. Responses include next_byte_offset for the original bytes actually returned, total_bytes and eof. Resume at that offset for partial long lines; incomplete UTF-8 sequences remain for the next page, while malformed bytes retain existing display repair. Reads stream through bounded buffers and never spill a duplicate of the source. File pages adapt to actual encoded output/context allowance; byte_limit requests default to 64 KiB with a 1 MiB maximum and may return less. Directories use one-based offset/limit entry pages, sorted names, next_entry_offset/eof and a 10,000-entry scan ceiling. File storage does not grant shell authorization.

Search through exec_command requires its usual approval even for reads. Neither plan mode nor headless operation gains a search exemption. Use explicit exclusions with portable `find` and `grep`, rather than relying on GNU-specific recursive flags:

```sh
find . -type d \( -name .git -o -name build -o -name node_modules \) -prune -o \
  -type f -exec grep -I -n -E -e 'pattern with spaces|alternative' /dev/null {} +
```

`/dev/null` makes grep include file names even in a single-file batch; `-I` skips binary files on the supported host/Tiger grep subset. `-e` protects leading-hyphen patterns. For a literal pattern use `-F` instead of `-E`. Quote patterns and file names; never remove necessary quoting to bypass approval. Host integration checks exercise exclusions, binary handling and Unicode paths; Tiger verification of the system tools remains part of target acceptance. Without exclusions, shell grep requires explicit directory filtering.

apply_patch accepts LF/CRLF protocol lines throughout directives and hunks. Target files recognize LF and CRLF only: lone carriage returns remain content. Insertions use the first recognized newline, otherwise LF; untouched endings remain intact. Sequential first-match/fuzzy behavior remains intentional. User-facing session selection prefers an exact ID, then the newest matching name (row insertion order breaks timestamp ties). Ownership reloads, stored references and `--last` use exact IDs. Ambiguous/corrupt completed-tool records still fail load/export/resume/fork; raw diagnostic export is deferred.


Permission rules normalize lexical path components, including `..` after authored wildcard components, without expanding globs. Configured roots are literal across equivalent absolute aliases (including `/tmp` and `/private/tmp` on macOS), even when their names contain glob characters. Patch preflight and execution use the same LF/CRLF protocol parser, so denied patch paths are rejected before PreToolUse dispatch. In-memory rule sets are immutable snapshots replaced through configuration methods; parsed-rule caching uses snapshot identity. Directory inspection shares root/prefix canonicalization only within one listing and checks each returned entry. Root authorization and symlink checks remain in force; separate operations refresh filesystem-dependent checks.

Prompt expansion and hook/user additions are prepared before mutation. A bounded append transaction reserves space and moves only new messages, rolling back appended messages without allocation if insertion or persistence fails. Recovery therefore sees the original transcript on failed admission; existing conversation history is not duplicated for every prompt. apply_patch creates new files with LF; exact new-file CRLF bytes require an authorized shell command. Updates retain the target's existing recognized ending style.

Reviewed runtime corrections and contract changes are described in [CODEX-TOOLS.md](CODEX-TOOLS.md). They preserve captured prompt provenance and existing per-tool permissions. Host regressions include root `/` aliases, informed command/stdin approvals, durable completed-action failures, explicit pipe EOF, bounded cleanup refresh, provider schemas/images, late steering and display selection.

## Small-core Codex alignment

`/model` shows the current selection and, for the Codex provider, the bundled reference
catalog. `/model MODEL [EFFORT]` changes the current provider's selection between turns.
Omitted effort selects the known Codex model's captured default, or leaves effort unspecified
for other/unknown models. Known Codex effort combinations are validated before saving;
unknown model IDs remain valid. Catalog entries do not establish account availability.
No runtime catalog request is made. Provider defaults and the 65,536-token context default
remain unchanged.

`/status` shows session identity, provider/model/effort, permissions and context limit.
`last_turn_usage_this_invocation` is null until a turn completes; it is not lifetime usage.
`/compact` uses the same budgeted compaction implementation as automatic compaction and
runs `PreCompact` with `trigger=manual`. It preserves the latest human message, its following
answer, protected tool outcomes and artifact references. A conversation with no older
history is a no-op. Cancellation, provider failure and failed persistence retain the prior
history. These commands report that they require an idle turn when entered during work;
they are never sent to the model as prompts.

Repeat `-i/--image PATH` for up to four initial attachments on fresh/resume/fork invocations.
Image-only input is valid. Relative paths are resolved from the invoking directory.
Attachments are consumed by the first submitted turn, use the existing high-detail encoder,
private artifact storage and combined 8 MiB request limit, and never authorize other files.
Explicit view_image deny rules remain effective. Missing/invalid/disabled images fail before
a model call. Non-Apple builds retain their unavailable native-codec diagnostic.

Codex Responses preserves ordered assistant text items and optional `commentary`,
`partial_answer`, and `final_answer` phases. Streaming and completed item events share IDs.
Headless text/JSON results and `-o` contain the last nonempty assistant message rather than
concatenated progress. Progress appears once in the terminal or on headless stderr.
JSONL adds optional `phase` and `final_result` fields without changing existing required
fields; `final_result` prevents a terminal answer from also being printed as progress.
Other providers retain the single untagged-message path. Phases alone never force another
request. Tool batches, pending user input and response completion retain their existing
follow-up behavior. Full provider replay remains intact.

SQLite schema 5 includes nullable per-session reasoning effort and retained-result references; schema 1–4 migrate forward.
Resume/fork keep saved effort for the same provider/model. Explicit CLI/profile effort wins;
a changed provider/model falls back to configured effort. Older messages without
`assistant_items` remain readable. Earlier binaries reject schema 5, so use the new binary
after migration. No credentials or configuration locations change.

See [reference fingerprints](validation/core-alignment-reference.json) and the
[compatibility ledger](FEATURE-PORT.md) for the selected upstream source snapshot.
