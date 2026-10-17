# Captured Codex prompts

Codet embeds per-model instructions from the supplied Codex `models-manager/models.json`,
supporting compaction and approval text from `../prompts`, and Codex collaboration-mode
templates. The capture is pinned in `prompts/manifest.json`; normal builds and installed
binaries never access those external trees or fetch a catalog.

Exact model slugs select captured messages. Missing messages fall back to bundled values;
explicit empty strings remain overrides. Unknown models, including other providers, use the
adapted captured `gpt-5.5` instructions. Identity is Codet and does not claim a model family.
Equal adapted assets share a single embedded constant. The captured `codex-auto-review` slug
selects only its base/mode/approval text; it does not enable a guardian or review service.

## Updating and reviewing

Run the importer explicitly when a new source capture is intended:

```sh
python3 scripts/import-prompts.py \
  --catalog /path/to/codex-rs/models-manager/models.json \
  --supporting /path/to/prompts \
  --modes /path/to/codex-rs/collaboration-mode-templates
```

`prompts/upstream` retains deduplicated original UTF-8 text. `prompts/assets` contains adapted
text. The manifest records full source hashes, JSON pointers, original/adapted content hashes,
and ordered exact before/after edits with occurrence counts and reasons. Multiple originals
can map to one adapted asset. The importer prunes obsolete hash-named files from its managed
asset directories. Review the manifest and asset diffs together. `codet.prompts` verifies that
every edit log replays exactly; CMake verifies adapted checksums before embedding.

Adaptations are limited to identity, unavailable tools/workflows and terminal capabilities.
Connector/plugin, inline visualization, guardian, delegation, scheduling and realtime
instructions are not enabled. Parallel JavaScript orchestration becomes sequential direct
tool use. Skills use filesystem paths. Plan templates retain their conversational workflow
but use `read_file`: Codet denies every shell command in Plan mode, including tests and reads.
Sandbox escalation wording is replaced by Codet application-authorization wording.
Adaptations replace complete known sentences or clauses, preserving surrounding permission
explanations, distinct skill steps, alias expansion and reading through EOF. Import reads and
writes UTF-8 with deterministic LF output. Generated headers change only when their content changes.

## Runtime composition

The assembler combines selected base instructions, Default/Plan mode, effective application
permissions, available tools, Tiger guidance and existing project/skill/memory context.
`plan` selects Plan instructions; `default`, `accept-edits` and `bypass` select Default.
Project documents and previews are loaded once per agent run and never passed through the
upstream adaptation rules. Each provider request refreshes a capability snapshot shared by
the assembler and schema selection. Mutable task-plan context refreshes separately on every
request. Runtime facts explicitly qualify general base guidance.

Desktop capability flags separately gate accessibility actions, framebuffer/image-dependent
screenshots and pointer tools, clipboard and AppleScript. Without image input or a codec,
the model receives text observations and supported native actions, without visual claims.
UI work defaults to screenshots, observed image coordinates and observation after action;
accessibility and installed app scripting remain secondary routes. The foreground desktop
is shared with the user. Aqua, Universal Access, expiry, crop and input constraints are
described in the generated prompt. This does not add modern browser or desktop adapters.

The prompt assembler adds no CLI flags or provider interfaces. Optional message-origin and
artifact-index metadata remain backward compatible with existing session JSON.
Unsupported operations disappear from the available tool list; their registry identities
remain valid for rules and historical transcripts. Execution still rechecks capabilities
and permissions. Headless unresolved approvals fail; question answers do not grant tool
authorization. Deny rules prevail even in bypass mode.

Context accounting retains the existing approximate three-bytes-per-token estimate and now
includes fixed prompt, tool schemas, request overhead and reserved output. Compaction uses the imported instructions and
summary prefix, with no tools. It runs only after completed tool batches, preserves the latest
human instruction verbatim, excluding newly tagged synthetic hook feedback from that selection. Legacy origin-less messages keep their historical role-based interpretation. The newest completed assistant/tool batch and subsequent feedback are also protected, with complete call/result pairing. Image
references move to a private paged index; no images are attached to the compacted continuation.
Summary requests reserve the configured generation allowance separately from their visible prose target (at most 8 KiB). Conservative JSON escaping headroom reduces that target to fit the protected continuation; fewer than 512 usable bytes fails before requesting a summary. Empty, exhausted or oversized summaries leave durable history intact.
Existing adapter output controls apply where supported; Codex subscription requests use the
prompt bound and returned-size validation without inventing unsupported transport fields.
Oversized fixed context, human input or summaries produce distinct diagnostics with estimated
minimum context settings. The default context_tokens is now 65536; max_output_tokens=8192, tool_output_limit=65536, output reservation and the three-bytes-per-token estimate remain unchanged. Instructions and human input are never
silently shortened. Image payloads retain their separate provider request budgets.

Host fixtures establish source integrity, composition, schema gates, adapter transport,
steering and compaction behavior. PPC cross-build verification establishes compile/link
compatibility. Neither establishes live model behavior or Tiger desktop runtime acceptance.
