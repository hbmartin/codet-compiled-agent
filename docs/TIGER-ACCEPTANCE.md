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


The `OutputReview.*` cases cover exact original-byte omission counts, UTF-8 and malformed-byte boundaries, long capture-path notices, partial writes and sync errors, bad store entries, conservative unreadable-file accounting, legacy quota adoption, active capture protection, 64 KiB batching, unchanged observation, hook-triggered capture and completed-buffer budgets. Run the acceptance script with its default offline cases first. In both terminal modes, generate a long output, page `metadata.spill_path`, and verify the omission count describes primary output rather than annotations. Run 70 trivial commands and confirm the earlier large capture survives. Repeat with `--ephemeral`, page the capture while active, then test success, provider failure and Ctrl-C; the temporary capture directory must disappear only after SessionEnd and process cleanup. Credentials, explicit memory updates and requested workspace edits retain their existing behavior. Record capture responsiveness and disk activity on the G4; host timing samples do not establish target performance.


The `Policies.*`, `DiagnosticPolicies.*` and `PromptPolicies.*` cases add observation-independent termination, private failed foreground ownership/active limits, ordinary exception isolation, resource/cancellation propagation, canonical metacharacter aliases, wildcard-parent normalization, CRLF preflight denial before hooks, full hook channel budgets, accurate diagnostics, bounded omission notices and prompt rollback. In both terminal modes exercise successful, failed and interrupted turns with a live hook diagnostic and SessionEnd warning/error. Curses must show live messages immediately and replay them once only after restoration, before cleanup messages. Flood both diagnostic phases with a small configured limit and verify separate complete omission notices. A successful SessionEnd hook printing large ignored stdout must not report cleanup failure. Repeat a narrow Glob in a large directory and record responsiveness on the G4; host samples do not establish target timing.

## Native computer use

For prompt acceptance, use an image-capable configured model in each terminal interface. Ask it
to open a disposable TextEdit document, inspect it visually, type a short Unicode sentence,
and verify the resulting text. Confirm screenshot observation precedes pointer input and that
the reported result follows a new observation. Repeat against a local browser page; verify it
uses the native desktop tools and never assumes DOM/CDP or Playwright. Let snapshots expire,
then verify fresh observation instead of stale coordinates or node IDs. Test cropped capture
and a focus change between observations. Record the selected model and whether prompts follow
the intended screenshot-first strategy; scripted host fixtures do not establish model behavior.

Disable assistive-device access and repeat visual observation/input with the user's keyboard
idle: AX action tools should disappear while pixel tools remain available. Configure
`image_input = false` and verify screenshots/pointer tools disappear and the agent explains
its visual limitation. Restore the setting afterward. In Plan mode verify read_file/desktop
observations work and shell commands, AppleScript and input mutations remain denied. Resume
after a mode change and confirm the prompt and offered tools reflect it. Measure peak RSS and
repeated-turn memory growth with the larger embedded prompts on the 512 MB G4.

From a Terminal in the logged-in Aqua session, enable **access for assistive devices** in Universal Access and run:

```sh
sh ./tiger-acceptance.sh --desktop
```

This explicit check opens a temporary plain-text fixture in TextEdit, types Unicode using native events, reads it back through AppleScript, records desktop state and a screenshot, tests plain-text clipboard round-trip when readable, activates Finder, and opens the default browser at `http://127.0.0.1/`. No external website or model API is contacted. The fixture closes without saving its synthetic edits; existing text clipboard content is restored. Run it while you can leave the keyboard/mouse idle. Reports include the captured desktop, window/application metadata and peak RSS.

Also verify mouse clicks, a drag, scrolling, right/double click, shortcuts, cropped/window capture, AXPress/set_value on observed controls, stale screenshot/node rejection after expiry, and cancellation while input is active. Test multiple display origins and 16/32-bit display modes when available. Confirm images arrive as pixels in a live image-capable provider round trip. Repeat observation/action cycles and record memory growth, responsiveness and swapping on the 512 MB G4. Both terminal interfaces must accept asynchronous replies while the agent is sleeping or working.

See `CODEX-TOOLS.md` for capability gates and exact compatibility boundaries. Offline fixtures and successful PPC linking do not mark these desktop/runtime checks complete.

## Reviewed runtime regression acceptance

In both terminal modes on the G4:

- Run screenshot-led TextEdit and local browser tasks above. On a second display, capture an observed window and a global region without `display_id`; verify the returned display/bounds and pointer placement. A window or region spanning displays must fail clearly; capture each display separately. Mixed selectors must fail. Repeat with expired snapshots and accessibility disabled.
- Start non-TTY `cat`, send Unicode text and `close_stdin: true`, and verify final output and exit status. Repeat close while a session is still alive, then confirm further writes fail. A PTY close request must fail before typing. Plan mode must reject EOF mutation.
- Approve a command with explicit cwd/shell and inspect those fields in the terminal and Bash hook input. Check outside-root cwd denial and explicit deny precedence even with root `/`. Verify stdin approval shows the original command and actual input.
- Use a PostToolUse hook that fails after a disposable patch or shell change. Resume the session and verify its completed result remains recorded and its side effect is not repeated. Check fatal/resource failures separately from ordinary blocking feedback.
- Send steering while a Stop hook is briefly waiting, then `/new`. Verify steering completes in the old session before the new session starts; cancel a turn and verify pending text is shown without automatic execution. Recover from an input-limit error in both terminal modes.
- Run image-producing tasks with `--ephemeral`; replay images during the invocation, then check that temporary images/captures disappear after success, failure and Ctrl-C and no files appear under permanent `CODET_HOME/artifacts`.
- Compare 1/2/4 MiB output capture responsiveness and repeat process starts with failed cleanup observations. Record time, peak RSS and disk behavior. Tiny split Unicode output must not create a spill notice.

Host fixtures and PPC compile/link checks do not establish these target results. Record actual execution and any unavailable multi-display setup separately.

For the follow-up runtime changes, also verify:

- A Bash PermissionRequest hook that approves an originating Python command cannot approve later stdin or EOF without `decision.tool: "write_stdin"`. Test exact operation matchers, legacy unscoped allows, explicit denies, headless operation and a valid scoped allow.
- After a post-hook fails, confirm a completed command remains journaled, uses its requested output limit, and survives invalid text in diagnostics. Resume without repeating its side effect.
- Send `/memory consolidate` while a Stop hook waits, followed by `/new`; it must remain model steering in the old session. Check interrupted approvals cannot consume the next prompt and error diagnostics appear before the next turn.
- Run enough screenshot-led TextEdit/browser interactions to compact. Verify the continuation contains no automatic images, inspect the paged index and reload a chosen image with `view_image`. Resume and fork; repeat with `--ephemeral` and confirm indexes disappear with the temporary images.
- Exercise small context settings and large first prompts; verify actionable fixed/input/summary diagnostics, current task-plan updates and Plan-mode memory inspection guidance.
- Repeat cleanup with observer/reap fault fixtures, then run new jobs; confirm capacity is recovered without per-job refresh waits. Check invalid display bounds fail before capture arithmetic where a display fixture is available.


Context/history follow-up acceptance (native Tiger execution required):

- With default context_tokens=65536, run a command producing 45 KiB and a multi-command batch; verify status survives projection, inspect metadata.result_path with headless read_file, and confirm commands are not repeated to recover omitted output.
- Exercise line pages and byte_offset/byte_limit on a long UTF-8 line, a capture and memory Markdown. Resume at next_byte_offset and verify no gaps or repeated bytes. Confirm explicit read_file denies and symlink-changing hooks block private reads in all permission modes.
- Save a long plan; verify active/pending steps and original numbers appear in its bounded view, then inspect the full file by byte pages. Confirm a >1 MiB update preserves the prior plan and a legacy oversized saved plan does not block resume.
- Capture repeated identical TextEdit/browser screenshots, force compaction, and verify every observation is in chronological sequence while the continuation has no attachments. Page the latest_line, explicitly reload with view_image, resume, fork and relocate state plus artifact storage. Legacy rows must say chronology unknown.
- Repeat index/result paging with --ephemeral and verify automatic storage is removed after hooks/process cleanup.
- With a valid and invalid display entry, explicitly capture the valid display and automatically capture a window/region on it; verify warnings for skipped invalid entries, unchanged IDs, correct pointer mapping and rejection of spans across valid displays.
- Force summary output exhaustion and a persistence failure; verify durable history/results survive and completed side effects are never replayed. Run screenshot-led TextEdit and browser workflows after explicit image reload.

## Small-core alignment acceptance

Run `SmallCore.*` on the target along with existing offline suites. In both line and curses
interfaces, run `/model`, select an available model/effort, reject an invalid known effort,
and use `/status` before and after a turn. Resume and fork and verify selection persistence.
Use `/compact` on an empty session (no provider call), then after several turns; verify the
PreCompact hook receives `manual`, latest steering/outcomes survive, and interruption keeps
history usable. Enter these commands during active work and verify they stay out of the
model prompt. Pass one and multiple local PNG/JPEG inputs with `-i`, including an image-only
prompt, and verify pixels reach an image-capable live provider. Repeat with denied paths,
invalid files and disabled image input. Observe separate progress/final messages without
duplicate terminal text and check final-only stdout/`-o` on a headless invocation. Exercise
ancestor/override guidance and skill metadata/support-file reads from both project and user
locations. Record startup latency, peak RSS, repeated-turn growth, responsiveness and any
swapping on the 512 MB G4. Cross-link and host fixtures do not establish this acceptance.

## Context recovery and inspection acceptance

In both terminal interfaces, execute a roughly 130 KiB patch once, then continue with a smaller context; inspect its archived arguments/outcome and confirm no repeated edit. In Plan mode request four large directory listings in one batch, page by next_entry_offset, and verify progress without a context dead end. Exercise early compaction, stable prior previews, recent build/test output, a summary larger than its advisory target, one retry for genuine overflow, and cancellation during summarization. Resume/fork after compaction and inspect retained captures, including after relocating state/artifact/capture roots; expired captures should fail only their read, and explicit denies must still prevail.

On Tiger's HFS+ volume, inspect a file through differently cased names and a hard link with read_file and view_image. Verify normal aliases work, replaced/symlink targets and new denies remain blocked, and record filesystem details. Host APFS results do not establish HFS+ behavior. Read JSON byte pages, malformed/valid UTF-8, a past-EOF line page and long lines; verify contiguous offsets and unchanged file formatting through resume/fork. Test a 100-step plan near 8 KiB and confirm one omission notice.

Build/run codet_persistence_bench with payload arguments 512 and 65536; record 100/200/300-result timing, peak RSS, swapping and responsiveness. These synthetic measurements retain whole-transcript writes; compare them with real stable-preview/compaction sessions on the 512 MB G4. Target runtime acceptance remains owner-run.


## PR #13 recovery and discovery follow-up

On a copy of schema-4 state, mix valid sessions/references with malformed transcript and
journal rows. Confirm migration completes, valid sessions reopen, corrupt rows retain their
bytes and still fail their own load/recovery, and genuine SQLite failures roll back for retry.
In both terminal interfaces, inspect directories just below, at and above 10,000 entries.
Nonterminal pages advance; `scan_limited` marks incomplete scans and `eof` ends the scanned
subset even when `truncated` remains true. Partial scans have no complete alphabetical
coverage or snapshot guarantee. Include explicit per-entry denies and an empty filtered page.

Use unusable memory/skill roots alongside readable project guidance and other skills. Confirm
local diagnostics preserve other context. On HFS+, inspect a user skill through `/var` or
another stable ancestor alias outside Codet home/cwd; advertised canonical paths must work,
while explicit denies and a hook replacing a descendant with a symlink must still block access.
Check progress/final display and unchanged item IDs. Record startup/turn responsiveness,
peak RSS and swapping on the 512 MB G4. Host/sanitizer/static/PPC verification does not complete
these owner-run target checks. Semgrep runs only in developer/CI environments, never on Tiger.

## Compaction and startup follow-up

- In both terminal modes, trigger automatic compaction with a fresh `-i` attachment, including image-only resume/fork. Confirm the main request receives it. Interrupt before a response and confirm it remains available. Previously presented images remain inspectable in the ledger.
- Inject a failing optional summary request when history fits the hard budget; confirm a diagnostic and normal continuation. Explicit PreCompact stop and Ctrl-C must still stop. Verify a large contiguous file page, including Unicode, can be continued without missing middle bytes.
- Poll a live execution repeatedly, let it exit, and trigger a checkpoint. Its ID must disappear from live state; after resume no historical process ID is offered as live. Inspect overflow recovery records through the retained index.
- Start two binaries concurrently against copied schema-3/4 state. Both must open successfully after serialized migration. Invalid images/effort must not create a session/fork or change `resume --last`.
- Change global reasoning effort between resumed turns. Unpinned sessions follow it; explicit selections survive resume/fork for the same provider/model. Test `/model MODEL` clearing a pin in each terminal.
- Run `codet_persistence_bench 512`, `codet_persistence_bench 4096`, and `codet_persistence_bench inspection` when available. Record listing latency, archive publication counts, save time, peak RSS and swapping. Directory pages still rescan their bounded subset; whole-transcript writes still grow cumulatively. Host measurements do not establish the 512 MB G4 target.
- Check HFS+ case aliases and hard links with opened-handle validation unchanged. The host APFS handle can resolve a hard link to another name; mismatched opened names must retain the normal target-changed error. Record actual Tiger behavior.

The reliability bundle also includes `codet_persistence_bench`. The acceptance script records growing transcript append work in durable and fast modes and repeated 64 KiB capture flushes with 40 retained files. Keep its JSONL, timing, peak-memory and I/O counters with the machine report. Host benchmarks do not establish G4 performance. Modernization tests cover hook turn cancellation, raw errors, signed epochs, private plan recovery, schema migration and abrupt process exits; actual power-loss durability remains an owner exercise.
