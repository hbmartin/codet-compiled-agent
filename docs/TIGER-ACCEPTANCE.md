# Tiger acceptance

Run these checks on the G4 with 512 MB RAM. The build host never executes the PPC application or test executable.

Install the TigerPorts dependencies matching `sysroot-manifest.json`: curl with OpenSSL, openssl3, zlib, sqlite3, ncurses, libedit, legacy-support, and curl-ca-bundle. The package contains no bundled runtime dylibs. The default target prefix is `/opt/tigr`; a different TigerPorts prefix requires a matching cross-build. `otool -L ./codet` lists direct runtime dependencies.

Extract `codet-tiger-ppc.tar.gz`, enter its directory, then run:

```sh
sh ./tiger-acceptance.sh
```

This runs offline GoogleTest fixtures, doctor, startup measurements, and resource reporting into a new `codet-acceptance-*` directory. Tests cover files/edits, permissions, credential import/refresh with fake HTTP, provider fixtures, sessions/recovery, PTYs/processes, and cancellation. The remaining-review suite also covers recovery ordering and no replay, path aliases, per-file Glob denies, malformed UTF-8 diagnostics, Codex-style hooks, oversized context, patch anchors/addition-only updates, startup deadlines, and completed-job eviction. The follow-up suite adds one-shot exit/cleanup failures, ownership loss, startup EINTR/cancellation, inherited SIGHUP/mask reset, 8 MiB captures, quotas/eviction/cross-process locking, paging and failed storage. No original recovered application code is executed. The optional live TLS test is skipped by default. The review regression cases also exercise system grep (including a 512 KiB line), signal-before-reap ordering, immediate child exits, descendant cleanup, inherited signal state, scoped shell rules, and bounded hook cleanup. `/usr/bin/grep` is a target prerequisite; no new regex library is required.

Explicit live checks:

```sh
sh ./tiger-acceptance.sh --tls
sh ./tiger-acceptance.sh --subscription /explicit/path/to/token-file.json
CODET_TEST_PROVIDER=openai CODET_TEST_MODEL=your-model-id sh ./tiger-acceptance.sh --api
```

The API check requires the provider's environment key. The subscription check imports a copy into temporary private state, can refresh/rotate it, and never edits the supplied source file. Temporary state is removed on exit. Retain credentials separately if you intend to use the agent afterward.

For interactive verification, set `CODET_HOME` to a separate test directory, configure/import the credential you want to use, and run both:

```sh
./codet --ui line
./codet --ui curses
```

Verify input/paste/backspace, a Unicode prompt, resize, Ctrl-C/Ctrl-D, approval acceptance/rejection, an actual provider/tool round trip, file changes, `/new`, session resume/fork, and restored terminal settings on exit. Ask the agent to save and recall a memory, inspect the resulting Markdown files, then run `./codet memory consolidate` explicitly and verify the resulting memory is readable in a fresh session. In each terminal mode, run `/permissions`, switch to `bypass`, and verify an approved shell grep round trip. Switch to `plan` and verify mutations are denied, then switch back to `default`. Verify an invalid mode leaves the current setting unchanged and that mode remains visible while model output streams. Interrupt a multi-tool turn, continue in the same REPL, then resume/fork it and verify completed edits are not repeated. Use `/new` with lifecycle hooks and verify start/end IDs still pair. Try instruction/memory files larger than 64 KiB and verify bounded context plus diagnostics. Run more than 64 sequential background jobs and confirm later Bash commands and hooks still work. If testing a local streaming endpoint, verify healthy keepalives exceed the former total cap and stalled streams time out. Verify streamed status clears after success, failure and cancellation while the permission mode remains visible; failing SessionEnd diagnostics must appear after curses closes. Generate output larger than the preview and 8 MiB, page the retained capture through Read and approved shell `dd`, and inspect retained/total/truncated metadata. Confirm private file modes and bounded store size across invocations. Run the documented find/grep exclusions against generated directories and binary fixtures. Run within a disposable workspace. Record CPU/RAM, terminal type, provider/model, startup time, peak RSS, responsiveness, and any swapping. Keep the workload comfortably within the 512 MB machine budget; report memory growth over repeated turns.

Return the acceptance report directory, terminal observations, and any relevant stderr. Reports should not include token files or real API keys. Native subscription success may remain unavailable while an explicitly selected API provider passes. Runtime acceptance remains unverified until the results are reviewed; no script marks it complete automatically.

The `FinalReview.*` and `Sanitization.*` cases add literal metacharacter workspaces, rule-cache freshness, isolated process failures/active limits, timeout during stop, independent hook channel floods, source matching, CRLF patch protocol/lone-CR content, exact session IDs, failed command expansion and pinned UTF-8 replacement equivalence. In both terminal modes, use a hook that writes stderr during a tool call and verify curses retains the diagnostic in its transcript; verify line mode uses stderr. Resume and fork with SessionStart source matchers and verify each invocation appends its newly produced context (including identical output). Repeat the failed custom-command case followed by a successful turn. These are owner-run target cases, not claims of completed Tiger acceptance.
