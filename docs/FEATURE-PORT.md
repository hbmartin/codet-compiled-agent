# Feature port ledger

The modern application now uses the supplied `../tools` Codex snapshot as its core-tool contract. Earlier Claurst utility interfaces are retired from the model-facing surface. The recovered C implementation remains separate and unchanged.

| Feature | Implementation | Verification |
|---|---|---|
| Canonical tools, schemas, hooks and availability | tool_registry.cpp, tools.cpp | Frozen required-field fixture, mode and legacy-name rejection tests |
| Yielding shell, pipes and PTYs | platform.cpp, tools.cpp | Numeric sessions, incremental output, stdin, exit status, cancellation and process ownership regressions |
| Codex patches | patch.cpp, tools.cpp | No prior read, parser/reference chunks, prevalidation, rollback and permissions |
| Scoped inspection | read_file | Private context in all modes; workspace in Plan; streaming line/byte pages and fresh handle validation |
| Plans, blocking and async questions | tools.cpp, interaction.cpp, cli.cpp | Persistence, question IDs, queued replies, wake-on-input sleep, both terminal modes |
| Native image inputs | images.cpp, provider.cpp | Encoding, bounds, private checksummed artifacts, all provider wire fixtures and missing-file diagnostics |
| Native Tiger desktop | computer.cpp | Injected dispatch/permissions, framebuffer conversion and coordinate tests; PPC compile/link checks; opt-in target execution |
| Sessions and compaction | state.cpp, agent.cpp | Schema 5, durable reference registry, historical recovery/no replay, ownership, fork isolation, attachment references through compaction |
| Providers/credentials | provider.cpp, config.cpp | Existing stream/retry/refresh fixtures plus namespace and image serialization |
| Instructions/skills/memory | tools.cpp, cli.cpp | Bounded automatic previews, skill paths and explicit consolidation CLI fixture |
| Hooks/configuration | hooks.cpp, tools.cpp, config.cpp | Existing hook lifecycle/denial/recovery tests plus updated tool identities |
| Host/static/PPC verification | CMake, analysis runner, Tiger scripts | Host tests, sanitizers, analyzers, PPC750 Mach-O and narrowly allowed system frameworks |

See [the tool contracts](CODEX-TOOLS.md) for exact interfaces and limitations. Agents, MCP, JavaScript code mode, plugins, schedules, hosted web service replicas and dedicated browser automation remain deferred. Application permissions do not imply an OS sandbox. Cross-building does not establish actual G4 runtime or live-provider acceptance; see [Tiger acceptance](TIGER-ACCEPTANCE.md).

Historical review dispositions and source-hashed evidence remain in [REVIEW-FOLLOWUP.md](REVIEW-FOLLOWUP.md). The current tool contracts supersede its retired callable names.

## Small-core alignment with codex-main

The existing tool contract continues to use the supplied `tools` snapshot. Core behavior
now also follows the supplied `codex-main` snapshot, starting from `d817cf6`.
[Reference SHA-256 fingerprints](validation/core-alignment-reference.json) pin the selected
source files; the unchanged prompt-catalog source hash remains in `prompts/manifest.json`.

| Behavior | Reference | Verification |
|---|---|---|
| Ordered assistant messages, phase metadata and final-message selection | protocol models, stream_events_utils, session/turn | Fragmented SSE, stable IDs, persistence/fork, final-only CLI outputs |
| Instruction precedence | core/agents_md | Global/project ordering, override/fallback selection, existing bounded-include regressions |
| Lazy skill catalog | skills/parser | Bounded scalar subset, deterministic conflict resolution, catalog limits and scoped inspection |
| Model/effort/status commands | tui/slash_command, models-manager catalog | Both terminal modes, invalid-selection rollback, resume/fork effort migration |
| Explicit compaction | core/compact | Shared automatic/manual budgets, no-op, protected human/outcomes, failure/cancellation retention |
| Initial image attachments | exec/cli | Image-only input, multiple images, combined limit, denies and unchanged roots |

Tiger adaptations remain deliberate: sequential tools, conservative Plan permissions,
no runtime catalog service, a bounded metadata subset instead of a YAML dependency, a
nonrecursive skill search, text commands instead of new picker interfaces, and the existing
native image/desktop backend. MCP, agents, plugins, app-server, native sign-in and review
workflows remain deferred. Target runtime and 512 MB acceptance remain owner-run checks.

[Validation summary](validation/core-alignment.json) records final host, sanitizer, analyzer and
PPC verification results with implementation hashes. Live Tiger acceptance remains unverified.

## Context recovery and inspection fixes

Stable batch previews and earlier compaction retain useful recent output; oversized completed batches can become archived checkpoints. Inspection uses adaptive encoded budgets, directory paging, verbatim JSON file text and compact provider metadata trailers. SQLite keeps its transcript row and adds durable scoped result references. Permission policy is centralized while fresh-path/handle checks remain. See [validation evidence](validation/context-recovery.json) and the owner-run HFS+/G4 cases in [Tiger acceptance](TIGER-ACCEPTANCE.md). Earlier validation reports describe their original source snapshots.

The compaction follow-up separates the 80% trigger from hard-budget preview allocation and shares provider text shaping for accounting. It preserves unseen human/tool images, falls back after ordinary optional summary failures, retains bounded structured recovery records and reports only currently owned live execution handles. Schema-5 upgrades serialize before rereading the version. CLI preflight precedes session/fork publication; reasoning effort inherits configuration until explicitly selected. Item callbacks are virtual request arguments. Directory rescanning and whole-transcript SQLite writes remain deliberate Tiger adaptations. See [source-hashed follow-up evidence](validation/compaction-followup.json).
