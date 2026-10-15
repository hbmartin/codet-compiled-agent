# Feature port ledger

The source reference is the supplied Claurst Rust tree (`claurst-main/src-rust`), not the npm launcher. "Implemented" means present in the modern source and covered by the listed host checks. Live provider and Tiger behavior remain pending target acceptance. Retired paths remain as historical C evidence and are excluded from `codet_core`.

| Feature | Rust reference | C++ implementation | Verification |
|---|---|---|---|
| GoogleTest/GoogleMock foundation | Provider fixture patterns and core tests | CMake/CTest; pinned 1.17.0 | `foundation_test.cpp`, offline fetch hashes |
| Typed service boundaries | core types; api provider; tool context | `core.hpp`: HTTP, clock, filesystem, processes, provider | MockHttp, FakeClock, MockFiles, MockProcesses, MockProvider |
| Codex-shaped CLI/headless | cli/main, cli/headless | cli.cpp | CLI contract checks; loopback integration tests |
| TOML configuration/profiles/trust | core config/import_config | config.cpp | Layering, override, invalid limit tests |
| Credential import/refresh | core oauth_config/accounts; api/providers/codex | config.cpp | Source preservation, 0600 storage, expired credentials, refresh rotation/failure fixtures |
| Codex Responses/custom patches/reasoning | api/providers/codex | provider.cpp | Request/replay and completion fixtures; live subscription pending |
| Anthropic | api/providers/anthropic | provider.cpp | Tool-input/thinking-signature fixtures; live API pending |
| OpenAI | api/providers/openai | provider.cpp | Arbitrary-byte SSE tool assembly; full curl CLI round trip |
| Google | api/providers/google | provider.cpp | Grouped function responses and thought-signature replay fixtures; live API pending |
| Ollama native chat | api/providers/openai_compat; supplied Codet provider | provider.cpp | Fragmented NDJSON, object arguments and named tool-result round trip; live LAN endpoint pending |
| Retry/cancellation/stream validation | api retry/stream_parser | platform.cpp, provider.cpp | 429 retry, malformed/truncated streams, SIGINT integration |
| File read/write/edit | tools file_read/file_write/file_edit | tools.cpp | Atomic mode preservation, read-before-write, stale/ambiguous edits, Unicode/binary/large files |
| Batch edits and Codex patches | tools batch_edit/apply_patch | tools.cpp | Prevalidation, same-file batches, injected disk failure rollback, patch changes |
| Glob/grep | tools glob_tool/grep_tool | tools.cpp | Quoted regex input; no generated shell invocation |
| PTY shell and background jobs | tools pty_bash/tasks | platform.cpp, tools.cpp | Native PTY input, bounded flood output, timeout, cancellation |
| Plans/todos/questions | tools todo_write/plan/ask_user | tools.cpp | Durable plans, read-only modes, noninteractive rejection; free-form/choice answers in both terminal modes |
| Permission rules/roots | core permissions/bash_classifier | tools.cpp | Deny precedence, sibling/symlink escapes, project trust, hook outcomes, utility-tool denials |
| Sessions/resume/fork/recovery | core session_storage/session_journal | state.cpp | Fork isolation, completed/uncertain tool recovery, migration, truncated messages, future schema rejection; CLI resume/fork |
| Context compaction | query runner/compaction | agent.cpp | Same-provider summary, retained objective, request cap |
| Instructions/includes/skills | core claudemd/skill_discovery | tools.cpp | Includes/cycles, skill loading, bounded prompt assembly |
| Durable memory/consolidation | core memdir; query session_memory/auto_dream | tools.cpp, cli.cpp | Explicit consolidation through CLI with a loopback provider; fresh-session recall; no background calls |
| Hooks/custom commands | core hooks; commands named_commands | tools.cpp | JSON stdin, block-before-write; manual target hook acceptance |
| Web fetch/search | tools web_fetch/web_search | tools.cpp | Text preservation and result URL fixtures; live search pending |
| Minimal ncurses + line REPL | tui app/input/render | cli.cpp | PTY startup/exit, resize, terminal restoration; Tiger terminal acceptance pending |
| Export/diagnostics/usage | commands export/diagnostics/usage | cli.cpp, agent.cpp | JSON/text output, doctor, usage/event fixtures |
| Modern Tiger dependencies | TigerPorts pinned recipes | build-tiger-sysroot.py + locks | SHA-256/patch records; static PPC verification |

## Supported subset and exclusions

- `ApplyPatch` uses the Codex envelope; unified diffs are rejected. Batch rollback restores file contents and reports failures; it is not a filesystem-wide atomic commit.
- Rules and hooks use Codet's documented TOML contract. No complete compatibility claim is made for every Claurst/Claude Code hook setting or Codex CLI payload.
- PTY jobs and their output are bounded and live for the current process. They do not survive restarting Codet. Task tools manage shell jobs, not model subagents.
- Context size is estimated from serialized bytes for compaction; provider-reported usage is shown separately. Unknown subscription pricing is not converted to invented dollar costs.
- The keyless DuckDuckGo adapter returns instant answers/related topics, matching the supplied Rust implementation. SearXNG/Brave/Serply provide configurable general search.
- Native interactive login, multi-account registries, old settings/conversation import, filesystem checkpoints/rewind, RAG, model-facing SQL, small-model routing, subagents/teams, goals/schedules, plugins, MCP/ACP/LSP, Git-specific helpers, media, remote control, and public sharing are outside this implementation.
- OS shell confinement is unavailable. No application rule or allowlist is represented as a kernel sandbox.

See `MODERN-AGENT.md` for exact interfaces and `TIGER-ACCEPTANCE.md` for the outstanding target checks.
