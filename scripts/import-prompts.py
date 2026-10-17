#!/usr/bin/env python3
"""Explicit, offline import of captured Codex messages. Not needed to build or run Codet."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def write_utf8(path, text):
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(text)


def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def adapt(text, family):
    edits = []

    def replace(before, after, reason):
        nonlocal text
        if before != after and before in text:
            count = text.count(before)
            text = text.replace(before, after)
            edits.append(dict(reason=reason, before=before, after=after, count=count))

    if family == "instructions":
        opening = text.splitlines()[0] if text else ""
        if opening.startswith("You are Codex,"):
            replace(opening, "You are Codet, a terminal coding assistant. You and the user share "
                    "one workspace, and your job is to collaborate with them until their intended "
                    "goal is completely handled.", "Codet identity without a model-family claim")
        replace("As Codex,", "As Codet,", "Codet identity")
        for heading in ("# Apps (Connectors)", "# Plugins", "### Visualizations"):
            pattern = re.escape(heading) + r"\n.*?(?=\n#{1," + str(len(heading.split()[0])) + r"} |\Z)"
            for match in list(re.finditer(pattern, text, re.S)):
                replace(match.group(), "", "Unavailable connector/plugin/inline visualization workflow")
        # Adapt complete known sentences/clauses rather than discarding mixed lines.
        replace("Batch independent searches and reads in one functions.exec using await Promise.allSettled([...]); inspect every result.",
                "Batch related searches and reads when practical; execute direct tools sequentially and inspect every result.",
                "Sequential direct inspection tools")
        replace("When calling `functions.exec`, parallelize independent tool calls by awaiting Promises. Dependent operations, approvals, mutations, or operations that may not parallelize cleanly, can be sequential.",
                "Group independent read-only work when practical. Keep dependent operations, approvals and mutations ordered; Codet executes direct tool calls sequentially.",
                "No JavaScript orchestration")
        replace("When possible, prefer parallelization over sequential tool calls, as this will help with round-trip latency and let you get work done faster.",
                "Group related reads when practical; direct tool calls execute sequentially.",
                "Sequential tool execution")
        replace("You parallelize tool calls whenever you can, especially file reads such as `cat`, `rg`, `sed`, `ls`, `git show`, `nl`, and `wc`. You use `multi_tool_use.parallel` for that parallelism, and only that.",
                "Group related file inspections such as `cat`, `rg`, `sed`, `ls`, `git show`, `nl`, and `wc` in direct tool calls; those calls execute sequentially.",
                "No parallel-tool wrapper; preserve shell-output guidance")
        replace("Playwright screenshots and canvas-pixel checks", "available browser screenshots and visual checks", "Tiger browser verification")
        replace("image search, known relevant images, or generated bitmap images", "available relevant local or user-supplied images", "No hosted image-search/generation tool")
        replace('Open and read the skill according to its location: filesystem skills should be read from the filesystem, environment-owned skills should be access via the corresponding environment, and orchestrator skills should be discovered by calling `skills.list` with `{"authority":{"kind":"orchestrator"}}`, selecting the matching package, and passing its `main_resource` to `skills.read`.',
                "Open and read listed filesystem SKILL.md files through exec_command, or read_file in Plan mode. Non-filesystem skill providers are unavailable; do not invent local paths for them.",
                "Filesystem-only skills; retain reread guidance")
        replace('For a filesystem path, open the file. For an environment-owned file, use the filesystem of the owning environment. For an orchestrator reference, call `skills.list` with `{"authority":{"kind":"orchestrator"}}`, select the matching package, and pass its `main_resource` to `skills.read`. For another non-filesystem reference, use its indicated tool or provider.',
                "For a filesystem path, read it through exec_command, or read_file in Plan mode. Non-filesystem skill providers are unavailable; do not invent local paths for them.",
                "Filesystem skills; preserve alias expansion and reading through EOF")
        replace(" For orchestrator skills, pass the exact referenced resource identifier with the same authority and package to `skills.read`; do not treat `skill://` identifiers as filesystem paths.",
                " Non-filesystem references cannot be read by Codet; do not treat skill:// identifiers as filesystem paths.",
                "Preserve relative supporting-resource resolution")
        replace(" For orchestrator skills, use `skills.read` and the available tools; do not invent a local path.",
                " Non-filesystem skill readers are unavailable; do not invent a local path.",
                "Retain preference for provided scripts")
        replace("`functions.request_user_input_async`", "`request_user_input_async`", "Direct tool name")
        replace("`functions.send_user_message_async` or `request_user_input_async` tool (depending on which is available)",
                "`request_user_input_async` tool when available", "Direct asynchronous question tool")
        # Remove only guardian-specific clauses; retain ordinary permission explanations.
        replace(", or approval auto-review block", "", "No automatic approval reviewer")
        for sentence in list(re.findall(r"If you receive an auto-review rejection[^\n]*?summarize the stated reason\.", text)):
            replace(sentence, "When an application permission rule denies an action, explain the action and reason, continue unrelated authorized work, and do not retry or use another tool to bypass the denial.",
                    "Application authorization replaces guardian rejection")
        replace("Put this explanation in a short, separate paragraph at the end of both commentary and final, after any permission question.",
                "Put this explanation in a short, separate paragraph after any permission question.", "Terminal permission explanation")
        replace("You have two channels for staying in conversation with the user:",
                "You communicate through streamed assistant text and a final answer:", "Terminal communication surface")
        for old in ("Do NOT put a final response in the commentary channel that should be asked in the final channel.",
                    "Do NOT put a final response (e.g. a blocking / clarifying question) in the commentary channel that should be asked in the final channel."):
            replace(old, "Use progress updates for ongoing work and put the final response in a self-contained final answer.", "Complete terminal communication sentence")
        for before, after in (("in the `commentary` channel", "through progress updates"),
                              ("in `commentary` channel", "through progress updates"),
                              ("to the `commentary` channel", "as progress updates"),
                              ("use the `commentary` channel", "provide progress updates"),
                              ("in the commentary channel", "through progress updates"),
                              ("to the `final` channel", "as your final answer"),
                              ("in the final channel", "in the final answer"),
                              ("in the commentary channel", "through progress updates"),
                              (", since they are collapsed after the final answer is shown to users", ""),
                              ("but you will see all prior user requests", "but earlier user requests may be represented in the summary"),
                              ("but you will still see all prior user requests", "but earlier user requests may be represented in the summary")):
            replace(before, after, "Terminal communication and compaction capabilities")
    if family == "mode":
        for line in list(text.splitlines()):
            if "active mode changes only" in line:
                replace(line, "The mode is selected by Codet's permission_mode setting. Only a "
                        "runtime mode change changes it; user requests do not.", "Codet mode selection")
        replace("You are in **Plan Mode** until a developer message explicitly ends it.",
                "You are in **Plan Mode** while Codet's permission_mode is plan.", "Codet mode selection")
        replace("Plan Mode is not changed by user intent, tone, or imperative language.",
                "Plan Mode is not changed by user intent, tone, or imperative language.", "Mode semantics")
        replace("* Tests, builds, or checks that may write to caches or build artifacts (for example, `target/`, `.cache/`, or snapshots) so long as they do not edit repo-tracked files",
                "* Inspect existing test/build results through read_file. Shell execution, including tests and builds, is unavailable in Codet Plan mode.",
                "Codet Plan mode denies all shell execution")
        replace("* Static analysis, inspection, and repo exploration", "* Inspection and repo exploration through read_file and available observation tools", "No shell in Plan mode")
        replace("* Dry-run style commands when they do not edit repo-tracked files", "* Available desktop observation tools that do not change application state", "No shell in Plan mode")
        replace("for example: search relevant files, inspect likely entrypoints/configs, confirm current implementation shape",
                "for example: inspect relevant directories, entrypoints and configs with read_file", "Plan inspection tool")
        replace("for example: `search relevant files`", "for example: inspect files with read_file", "Plan inspection tool")
    if family == "approval" and text:
        replace(text, "Tool authorization is enforced by Codet's application rules, not an OS sandbox. "
                "An allow rule or application approval is required for all shell execution and other mutations unless the selected "
                "permission mode grants them. Explicit deny rules prevail. Do not request shell "
                "escalation or use another tool to bypass a denial.\n", "Tiger application authorization replaces sandbox escalation")
    for line in list(text.splitlines()):
        if line.startswith("Open and read listed filesystem SKILL.md") and line != line.rstrip():
            replace(line, line.rstrip(), "Normalize adapted skill paragraph whitespace")
    return text, edits


def import_bundle(catalog, supporting, modes, destination):
    manifest = dict(version=1, sources={}, assets={}, models={}, bundled={}, catalog={})
    for path in (catalog,):
        manifest["sources"]["models-manager/models.json"] = hashlib.sha256(path.read_bytes()).hexdigest()

    def asset(text, family, source):
        if text is None:
            return None
        original = digest(text)
        adapted, edits = adapt(text, family)
        key = digest(adapted)
        for sub, name, content in (("upstream", original, text), ("assets", key, adapted)):
            target = destination / sub / (name + ".md")
            target.parent.mkdir(parents=True, exist_ok=True)
            write_utf8(target, content)
        entry = manifest["assets"].setdefault(key, dict(adapted_sha256=key, origins=[]))
        origin = next((o for o in entry["origins"] if o["upstream_sha256"] == original), None)
        if origin is None:
            origin = dict(upstream_sha256=original, edits=edits, sources=[])
            entry["origins"].append(origin)
        origin["sources"].append(source)
        return key

    for index, model in enumerate(json.loads(catalog.read_text(encoding="utf-8"))["models"]):
        slug = model["slug"]
        if not re.fullmatch(r"[a-zA-Z0-9._-]+", slug):
            raise ValueError("Invalid model slug")
        messages = model.get("model_messages") or {}
        collaboration = messages.get("collaboration_modes") or {}
        manifest["catalog"][slug] = {
            "default_effort": model.get("default_reasoning_level") or "",
            "efforts": [entry["effort"] for entry in model.get("supported_reasoning_levels", [])]}
        manifest["models"][slug] = {
            "instructions": asset(messages.get("instructions_template"), "instructions", f"models-manager/models.json#/models/{index}/model_messages/instructions_template"),
            **{mode: asset(collaboration.get(mode), "mode", f"models-manager/models.json#/models/{index}/model_messages/collaboration_modes/{mode}") for mode in ("default", "plan")},
            **{f"approval_{policy}": asset((messages.get("approvals") or {}).get(policy), "approval", f"models-manager/models.json#/models/{index}/model_messages/approvals/{policy}") for policy in ("on_request", "never")}}
    sources = {"compact": supporting / "templates/compact/prompt.md",
               "summary_prefix": supporting / "templates/compact/summary_prefix.md",
               "default": modes / "templates/default.md", "plan": modes / "templates/plan.md",
               "approval_on_request": supporting / "templates/permissions/approval_policy/on_request.md",
               "approval_never": supporting / "templates/permissions/approval_policy/never.md"}
    for name, path in sources.items():
        source = ("collaboration-mode-templates/" + str(path.relative_to(modes))
                  if name in ("default", "plan") else "prompts/" + str(path.relative_to(supporting)))
        manifest["sources"][source] = hashlib.sha256(path.read_bytes()).hexdigest()
        family = "mode" if name in ("default", "plan") else "approval" if name.startswith("approval_") else name
        manifest["bundled"][name] = asset(path.read_text(encoding="utf-8"), family, source)
    destination.mkdir(parents=True, exist_ok=True)
    write_utf8(destination / "manifest.json", json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    retained = {"assets": set(manifest["assets"]), "upstream": {
        o["upstream_sha256"] for a in manifest["assets"].values() for o in a["origins"]}}
    for directory, keys in retained.items():
        for path in (destination / directory).glob("*.md"):
            if re.fullmatch(r"[0-9a-f]{64}", path.stem) and path.stem not in keys:
                path.unlink()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--supporting", type=Path, required=True)
    parser.add_argument("--modes", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "prompts")
    args = parser.parse_args()
    import_bundle(args.catalog, args.supporting, args.modes, args.output)
