# Codex tool compatibility and Tiger desktop control

The supplied `../tools` source snapshot defines the reference contract. Codet implements its selected core tools in C++17, with a native PowerPC Tiger desktop backend. JavaScript code mode, MCP, agents, plugins, scheduling, hosted search and dedicated browser adapters remain outside this implementation.

## Public tool surface

| Tool | Behavior |
|---|---|
| `exec_command` | `cmd`, optional `workdir`, `shell`, `login` (true), `tty` (false), `yield_time_ms` (10000, clamped 250–30000), `max_output_tokens` (10000). Returns `output`, `wall_time_seconds`, `chunk_id`, and either numeric `session_id` or `exit_code`. Nonzero command status is a completed tool result. |
| `write_stdin` | Numeric `session_id`, optional `chars`, `yield_time_ms`, `max_output_tokens`. Nonempty writes wait 250 ms by default; empty polls wait 5000 ms by default. Returns newly collected output. Empty polls clamp to 5000–300000 ms; writes cap at 30000 ms. |
| `apply_patch` | Codex patch envelope, including Add/Delete/Update/Move. Raw custom-tool input for the subscription Responses transport; `{patch: ...}` for function-only providers. No mandatory earlier Read call. |
| `view_image` | Local `path`, `detail` high/original. Sends image pixels to the model and records a private artifact reference. |
| `update_plan` | Optional `explanation`, `plan` of `{step,status}`. Status is pending/in_progress/completed, with at most one active step. Unavailable in Plan mode. |
| `request_user_input` | Interactive Plan mode: one to three questions with stable IDs, headers, two or three described choices and free-text answers. Returns `answers[id].answers`. |
| `request_user_input_async` | Interactive mode: questions with titles and optional suggested choices. Returns `accepted: true`; submitted replies enter the conversation as new user messages. No option is selected automatically. |
| `send_message_to_user_async` | Interactive mode: a bounded message needing attention; returns immediately. |
| `clock.curr_time` | UTC `current_time`, formatted `YYYY-MM-DD HH:MM:SS UTC`. |
| `clock.sleep` | `duration_ms` from 1 to 43200000. Wakes on user input or cancellation and reports elapsed wall time. Headless operation has cancellation but no live input channel. |
| `read_file` | Codet extension available in Plan mode: `path`, one-based `offset` and `limit` (2000). Reads numbered text lines or lists a directory. Root and per-entry denials apply. |

`tool_registry.cpp` owns schemas, dispatch identities, mode availability, mutation classification, hook identities and event categories. Responses uses native `clock` and `computer` namespaces. Function-only providers reversibly encode qualified names as `clock__curr_time`, `computer__get_state`, etc.

Legacy `Read`, `Write`, `Edit`, `BatchEdit`, `Bash`, `Glob`, `Task*`, `ApplyPatch`, `TodoWrite`, `AskUserQuestion`, `EnterPlanMode`, `ExitPlanMode`, `Sleep`, `Memory`, `Skill`, `WebFetch` and `WebSearch` calls are removed. There are no callable aliases. Update existing permission rules to new public names; unknown exact names fail configuration validation. Historical completed calls still load, export and recover without re-execution.

Use shell commands for ordinary reads/searches, patches for edits, and the automatically listed skill paths to read relevant SKILL.md files. Automatic instruction loading, memory previews, sessions, export and explicit `memory consolidate` remain available. Memory consolidation adds the memory directory as an authorized root for that invocation and uses patches under the selected permission mode. Further shell reads still require shell authorization.

## Permissions and execution lifetime

Application rules do not provide OS shell confinement. `sandbox_permissions: use_default` is accepted; escalation, justification and reusable escalation prefixes are rejected explicitly. Deny rules beat allow/bypass. `accept-edits` grants patches. Plan mode rejects shell execution, patches, nonempty stdin writes and desktop mutations, while allowing observation and `read_file` inspection. Headless unresolved approvals fail as tool errors.

Shell hooks use Codex's canonical `Bash` hook identity; patches use `apply_patch`. Hook identities are separate from public permission-rule names. Shell `PreToolUse` receives `{command: ...}` and rewritten commands are reauthorized. `write_stdin` does not repeat the pre-hook; completion produces the matching post-hook.

Yielding does not terminate a process. Sessions live for the Codet process lifetime, are bounded, and support pipe or PTY input. Cancellation stops the owned process group. Output is bounded per collection and per response; `original_token_count` is an estimate when truncation occurs.

The terminal owns input and rendering on the main thread. The agent worker owns tool execution and session writes. Submitted user messages are injected between complete tool batches. `/quit` also works during an active turn. Permission mode changes are made between turns. JSONL retains the existing thread/turn/item envelopes and adds `codet.user_input.requested`, `codet.user_input.received`, `codet.user_message` and `codet.artifact` events.

## Image storage and provider replay

Image input defaults on for Codex/OpenAI/Anthropic/Google and off for Ollama. Override with `[providers.NAME] image_input = true/false`; a transport capability does not guarantee that every selected model accepts images. `doctor` reports image, desktop and interaction capabilities. Non-Apple host builds report ImageIO unavailable, and desktop control requires a PowerPC Tiger console.

ImageIO handles image decoding and PNG encoding. High detail previews fit a 1280-pixel longest edge; original detail preserves dimensions within limits. Individual encoded images are limited to 8 MiB and decoded buffers to 32 MiB. Requests include at most four recent images and 8 MiB of image data. Omitted images remain reloadable through `view_image`.

Images live as immutable, checksummed files under `CODET_HOME/artifacts`, with private directory/file permissions. Messages and journal results contain artifact references, MIME types, dimensions and detail hints; provider requests encode the bytes as the provider's native image parts. Base64 data is not written into JSONL events or the session journal. Missing or changed files produce diagnostics; screenshot actions are never repeated to repair history. Session schema 3 preserves references through recovery/fork and carries references into compaction summaries. Artifact files remain until explicitly removed with their state directory.

## Native Tiger computer tools

| Functions in `computer` | Interface |
|---|---|
| `get_state` | Optional `pid`; returns snapshot ID, applications, focused PID, displays, selected application's windows and accessibility tree. Default application is focused. |
| `screenshot` | Optional `display_id` (1-based active display index, main display first), `window_id`, or global desktop `x/y/width/height` region; optional detail. Returns pixels, bounds, dimensions and screenshot ID. Window capture is a visible crop. |
| `move`, `click`, `drag`, `scroll` | Require `screenshot_id`, `x`, `y` in returned-image coordinates. Drag adds `to_x/to_y`; click supports left/right/middle and count 1–2; scroll uses line deltas, positive up/left. |
| `type_text`, `key` | Unicode `text`, or a named key with command/shift/option/control modifiers. Key chords are atomic. |
| `perform_action`, `set_value` | Require snapshot/node IDs and an advertised action or settable AXValue. Secure text values are not exposed. |
| `activate_app`, `open_app`, `open_url` | Observed PID, absolute .app bundle path, or HTTP(S) URL. |
| `clipboard_read`, `clipboard_write` | Bounded plain text clipboard access. |
| `applescript` | `script`, optional timeout_ms (30000; maximum 60000), through `/usr/bin/osascript` without shell interpolation. |

Tiger uses Process Manager/LaunchServices, AXUIElement, CGEvent, CGPostScrollWheelEvent, Carbon Pasteboard and ImageIO. Display capture reads the Tiger framebuffer without capturing/exclusively owning the display, handling row stride and 16/32-bit PPC pixel layouts. It does not depend on modern capture APIs, Swift, Node or a companion machine.

Accessibility observation has a two-second traversal budget, 50 ms messaging timeouts, depth 12 and 256 nodes. Snapshots expire after 60 seconds. At most eight screenshot mappings are retained, each expiring after 60 seconds; cropping and resizing map image coordinates back into global display coordinates. Refresh observations after app/layout changes. Desktop mutations use a private per-user interprocess lock and best-effort cleanup of synthesized pressed keys/buttons. Mutation results report dispatch, and the agent must observe application success separately.

A logged-in Aqua console is required. Enable **access for assistive devices** in Tiger's Universal Access preferences for accessibility operations. Pixel input/capture and other capabilities are reported separately when accessibility is unavailable. Safari and other browsers use these same desktop tools and their installed AppleScript support; modern CDP/Playwright behavior is not claimed.

Schema selection and prompt composition share a capability snapshot for each model request.
Accessibility actions are omitted when accessibility is disabled. Screenshots require framebuffer,
image-input and codec support; pointer tools are omitted when screenshots cannot be supplied.
Clipboard and AppleScript tools follow their respective backend flags. Remaining observations
and native actions stay available under the existing application permissions. Prompts prefer
screenshots for UI work and describe accessibility/AppleScript as secondary routes. See
[prompt sources and adaptations](PROMPTS.md) for provenance and context accounting.
