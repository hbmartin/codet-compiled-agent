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
| Sessions and compaction | state.cpp, agent.cpp | Schema 3, historical recovery/no replay, ownership, fork isolation, attachment references through compaction |
| Providers/credentials | provider.cpp, config.cpp | Existing stream/retry/refresh fixtures plus namespace and image serialization |
| Instructions/skills/memory | tools.cpp, cli.cpp | Bounded automatic previews, skill paths and explicit consolidation CLI fixture |
| Hooks/configuration | hooks.cpp, tools.cpp, config.cpp | Existing hook lifecycle/denial/recovery tests plus updated tool identities |
| Host/static/PPC verification | CMake, analysis runner, Tiger scripts | Host tests, sanitizers, analyzers, PPC750 Mach-O and narrowly allowed system frameworks |

See [the tool contracts](CODEX-TOOLS.md) for exact interfaces and limitations. Agents, MCP, JavaScript code mode, plugins, schedules, hosted web service replicas and dedicated browser automation remain deferred. Application permissions do not imply an OS sandbox. Cross-building does not establish actual G4 runtime or live-provider acceptance; see [Tiger acceptance](TIGER-ACCEPTANCE.md).

Historical review dispositions and source-hashed evidence remain in [REVIEW-FOLLOWUP.md](REVIEW-FOLLOWUP.md). The current tool contracts supersede its retired callable names.
