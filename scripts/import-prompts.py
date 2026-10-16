#!/usr/bin/env python3
"""Explicit, offline import of captured Codex messages. Not needed to build or run Codet."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def digest(text):
    return hashlib.sha256(text.encode()).hexdigest()


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
        for line in list(text.splitlines()):
            if any(s in line for s in ("functions.exec", "multi_tool_use.parallel", "parallelization over", "parallelize tool calls")):
                replace(line, "- Group independent read-only work when practical. Tool calls execute "
                        "sequentially; keep dependencies, mutations and approvals ordered.",
                        "No JavaScript orchestration or parallel-tool wrapper")
            elif "Playwright screenshots" in line:
                replace(line, line.replace("Playwright screenshots and canvas-pixel checks", "available browser screenshots and visual checks"),
                        "Tiger browser verification")
            elif "image search, known relevant images, or generated bitmap images" in line:
                replace(line, line.replace("image search, known relevant images, or generated bitmap images", "available relevant local or user-supplied images"),
                        "No hosted image-search/generation tool")
            elif "skills.list" in line or "skills.read" in line:
                prefix = re.match(r"\s*(?:[-*]|\d\))?\s*", line).group()
                replace(line, prefix + "Read listed filesystem SKILL.md files completely through "
                        "exec_command, or read_file in Plan mode. Resolve supporting paths relative "
                        "to the skill directory; do not invent non-filesystem providers.",
                        "Filesystem-only skills on Tiger")
        replace("`functions.request_user_input_async`", "`request_user_input_async`", "Direct tool name")
        # Codet streams assistant text but has no separate commentary/final message fields.
        for before in ("You have two channels for staying in conversation with the user:",):
            replace(before, "You communicate with the user through streamed assistant text:", "Terminal communication surface")
        for before in ("`commentary` channel", "`commentary`", "commentary channel"):
            replace(before, "progress updates", "Terminal progress text, not a protocol channel")
        for before in ("`final` channel", "final channel"):
            replace(before, "final answer", "Terminal final answer, not a protocol channel")
        replace("- You share updates in progress updates.", "- Share concise progress updates before tool calls.", "Terminal progress wording")
        replace("- You share updates in the progress updates.", "- Share concise progress updates before tool calls.", "Terminal progress wording")
        replace("`functions.send_user_message_async` or `request_user_input_async` tool (depending on which is available)",
                "`request_user_input_async` tool when available", "Direct asynchronous question tool")
        for before, after in (("in the progress updates", "as progress updates"),
                              ("in progress updates", "as progress updates"),
                              ("since they are collapsed after the final answer is shown to users", "alongside the final answer"),
                              ("but you will see all prior user requests", "but earlier user requests may be represented in the summary"),
                              ("but you will still see all prior user requests", "but earlier user requests may be represented in the summary")):
            replace(before, after, "Terminal communication and compaction capabilities")
        # The auto-review service is not part of Codet's application authorization.
        for paragraph in list(text.split("\n\n")):
            if "auto-review" in paragraph.lower() or "automatic approval review" in paragraph.lower():
                replace(paragraph, "When an application permission rule denies an action, explain "
                        "the denial and continue unrelated authorized work. Do not retry or use "
                        "another tool to bypass the denial.", "No guardian or automatic approval reviewer")
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
                "An allow rule or application approval is required for mutations unless the selected "
                "permission mode grants them. Explicit deny rules prevail. Do not request shell "
                "escalation or use another tool to bypass a denial.\n", "Tiger application authorization replaces sandbox escalation")
    return text, edits


def import_bundle(catalog, supporting, modes, destination):
    manifest = dict(version=1, sources={}, assets={}, models={}, bundled={})
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
            target.write_text(content)
        entry = manifest["assets"].setdefault(key, dict(adapted_sha256=key, origins=[]))
        origin = next((o for o in entry["origins"] if o["upstream_sha256"] == original), None)
        if origin is None:
            origin = dict(upstream_sha256=original, edits=edits, sources=[])
            entry["origins"].append(origin)
        origin["sources"].append(source)
        return key

    for index, model in enumerate(json.loads(catalog.read_text())["models"]):
        slug = model["slug"]
        if not re.fullmatch(r"[a-zA-Z0-9._-]+", slug):
            raise ValueError("Invalid model slug")
        messages = model.get("model_messages") or {}
        collaboration = messages.get("collaboration_modes") or {}
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
        manifest["bundled"][name] = asset(path.read_text(), family, source)
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
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
