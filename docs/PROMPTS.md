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

## Runtime composition

The assembler combines selected base instructions, Default/Plan mode, effective application
permissions, available tools, Tiger guidance and existing project/skill/memory context.
`plan` selects Plan instructions; `default`, `accept-edits` and `bypass` select Default.
Project documents and previews are loaded once per agent run and never passed through the
upstream adaptation rules. Each provider request refreshes a capability snapshot shared by
the assembler and schema selection. Runtime facts explicitly qualify general base guidance.

Desktop capability flags separately gate accessibility actions, framebuffer/image-dependent
screenshots and pointer tools, clipboard and AppleScript. Without image input or a codec,
the model receives text observations and supported native actions, without visual claims.
UI work defaults to screenshots, observed image coordinates and observation after action;
accessibility and installed app scripting remain secondary routes. The foreground desktop
is shared with the user. Aqua, Universal Access, expiry, crop and input constraints are
described in the generated prompt. This does not add modern browser or desktop adapters.

No CLI flags, provider interfaces, tool parameter schemas or session schema were added.
Unsupported operations disappear from the available tool list; their registry identities
remain valid for rules and historical transcripts. Execution still rechecks capabilities
and permissions. Headless unresolved approvals fail; question answers do not grant tool
authorization. Deny rules prevail even in bypass mode.

Context accounting retains the existing approximate three-bytes-per-token estimate and now
includes fixed prompt and tool-schema bytes. Compaction uses the imported instructions and
summary prefix, with no tools. It runs only after completed tool batches, preserves the latest
user message verbatim and deduplicated artifact references, and commits only a smaller
continuation that fits. Oversized fixed context or an ineffective summary fails with a
diagnostic recommending a larger context or smaller document/memory limits. Instructions are
never silently shortened. Image payloads retain their separate provider request budgets.

Host fixtures establish source integrity, composition, schema gates, adapter transport,
steering and compaction behavior. PPC cross-build verification establishes compile/link
compatibility. Neither establishes live model behavior or Tiger desktop runtime acceptance.
