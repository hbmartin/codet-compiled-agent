"""Offline source integrity, exact adaptation replay and sparse-catalog regression checks."""
import hashlib
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("import_prompts", ROOT / "scripts/import-prompts.py")
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)


class PromptBundle(unittest.TestCase):
    def test_cmake_embeds_literal_bytes_and_keeps_unchanged_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            text = "Literal @VAR@ ${interpolation}\n#cmakedefine ENABLED\n雪🌲\n"
            key = hashlib.sha256(text.encode("utf-8")).hexdigest()
            (root / "prompts/assets").mkdir(parents=True)
            (root / "prompts/assets" / (key + ".md")).write_bytes(text.encode("utf-8"))
            manifest = {"assets": {key: {}}, "models": {"gpt-5.5": {name: key for name in
                        ("instructions", "default", "plan", "approval_on_request", "approval_never")}},
                        "bundled": {name: key for name in
                        ("compact", "summary_prefix", "default", "plan", "approval_on_request", "approval_never")}}
            (root / "prompts/manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
            (root / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(fixture NONE)\nset(VAR changed)\nset(ENABLED 1)\ninclude("' + str(ROOT / "cmake/prompts.cmake") + '")\n', encoding="utf-8")
            command = ["cmake", "-S", str(root), "-B", str(root / "build")]
            subprocess.run(command, check=True, capture_output=True)
            header = root / "build/generated/prompts.hpp"
            self.assertIn(text.encode("utf-8"), header.read_bytes())
            timestamp = header.stat().st_mtime_ns
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(header.stat().st_mtime_ns, timestamp)

    def test_adaptation_preserves_distinct_skill_steps_and_permission_explanations(self):
        originals = [(p, p.read_text(encoding="utf-8")) for p in (ROOT / "prompts/upstream").glob("*.md")]
        for path, original in originals:
            if not original.startswith("You are Codex,"):
                continue
            adapted, _ = importer.adapt(original, "instructions")
            with self.subTest(asset=path.name):
                for unavailable in ("functions.exec", "multi_tool_use.parallel", "skills.list", "skills.read",
                                    "auto-review rejection", "to the final answer", "the progress updates"):
                    self.assertNotIn(unavailable, adapted)
                if "If a read is truncated or paginated, continue until EOF." in original:
                    self.assertIn("If a read is truncated or paginated, continue until EOF.", adapted)
                    self.assertIn("expand the matching root alias", adapted)
                    self.assertIn("prefer running or patching provided scripts", adapted)
                if "Avoid re-reading skills when possible." in original:
                    self.assertIn("Avoid re-reading skills when possible.", adapted)
                if "approval auto-review block" in original:
                    self.assertIn("explicitly explain why you need the confirmation", adapted)
                    self.assertIn("SKILL.md, AGENTS.md, memory", adapted)
                if "The final answer must always be fully self-contained" in original:
                    self.assertIn("The final answer must always be fully self-contained", adapted)

    def test_identity_adaptation_preserves_product_references(self):
        original = ("You are Codex, fixture.\nAs Codex, help the user.\n"
                    "You have a vivid inner life as Codex: curious and present.\n"
                    "Use the Codex source snapshot as a reference.\n")
        adapted, edits = importer.adapt(original, "instructions")
        self.assertTrue(adapted.startswith("You are Codet,"))
        self.assertIn("As Codet, help the user.", adapted)
        self.assertIn("You have a vivid inner life as Codet: curious and present.", adapted)
        self.assertIn("Use the Codex source snapshot as a reference.", adapted)
        replayed = original
        for edit in edits:
            self.assertEqual(replayed.count(edit["before"]), edit["count"])
            replayed = replayed.replace(edit["before"], edit["after"])
        self.assertEqual(replayed, adapted)

    def test_offline_catalog_records_only_selection_metadata(self):
        manifest = json.loads((ROOT / "prompts/manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(set(manifest["catalog"]), set(manifest["models"]))
        for model in manifest["catalog"].values():
            self.assertEqual(set(model), {"default_effort", "efforts"})
            if model["default_effort"]:
                self.assertIn(model["default_effort"], model["efforts"])

    def test_utf8_bytes_and_newlines_are_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "unicode.md"
            text = "雪🌲é ’ — …\n"
            importer.write_utf8(target, text)
            self.assertEqual(target.read_bytes(), text.encode("utf-8"))
            self.assertEqual(importer.digest(text), hashlib.sha256(target.read_bytes()).hexdigest())

    def test_every_asset_has_complete_replayable_provenance(self):
        bundle = ROOT / "prompts"
        manifest = json.loads((bundle / "manifest.json").read_text(encoding="utf-8"))
        for key, asset in manifest["assets"].items():
            path = bundle / "assets" / (key + ".md")
            adapted = path.read_text(encoding="utf-8")
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), key)
            self.assertEqual(importer.digest(adapted), key)
            self.assertEqual(asset["adapted_sha256"], key)
            self.assertTrue(asset["origins"])
            for origin in asset["origins"]:
                path = bundle / "upstream" / (origin["upstream_sha256"] + ".md")
                text = path.read_text(encoding="utf-8")
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), origin["upstream_sha256"])
                self.assertEqual(importer.digest(text), origin["upstream_sha256"])
                for edit in origin["edits"]:
                    self.assertEqual(text.count(edit["before"]), edit["count"])
                    self.assertTrue(edit["reason"])
                    text = text.replace(edit["before"], edit["after"])
                self.assertEqual(text, adapted)
                self.assertTrue(origin["sources"])
        refs = [v for m in manifest["models"].values() for v in m.values()]
        refs += list(manifest["bundled"].values())
        self.assertTrue(all(r is None or r in manifest["assets"] for r in refs))
        self.assertLess(len(manifest["assets"]), len([r for r in refs if r]))
        for key in ("compact", "summary_prefix"):
            asset = manifest["assets"][manifest["bundled"][key]]
            self.assertTrue(all(not o["edits"] for o in asset["origins"]))

    def test_import_preserves_null_empty_and_records_full_source_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            catalog = root / "models.json"
            catalog.write_text(json.dumps({"models": [
                {"slug": "gpt-5.5", "model_messages": {"instructions_template": "You are Codex, 雪 ’ — …."}},
                {"slug": "missing"},
                {"slug": "empty", "model_messages": {"instructions_template": "", "collaboration_modes": {"plan": ""}}}]}, ensure_ascii=False), encoding="utf-8", newline="\n")
            supporting = root / "supporting"
            modes = root / "modes"
            for path in (supporting / "templates/compact/prompt.md", supporting / "templates/compact/summary_prefix.md",
                         supporting / "templates/permissions/approval_policy/on_request.md", supporting / "templates/permissions/approval_policy/never.md",
                         modes / "templates/default.md", modes / "templates/plan.md"):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(path.name + " 雪 ’ — …\n", encoding="utf-8", newline="\n")
            output = root / "output"
            importer.import_bundle(catalog, supporting, modes, output)
            manifest = json.loads((output / "manifest.json").read_text(encoding="utf-8"))
            self.assertIsNone(manifest["models"]["missing"]["instructions"])
            self.assertEqual(manifest["models"]["empty"]["instructions"], importer.digest(""))
            self.assertEqual(manifest["models"]["empty"]["plan"], importer.digest(""))
            self.assertEqual(manifest["sources"]["models-manager/models.json"], hashlib.sha256(catalog.read_bytes()).hexdigest())
            before = {str(p.relative_to(output)): p.read_bytes() for p in output.rglob("*") if p.is_file()}
            importer.import_bundle(catalog, supporting, modes, output)
            self.assertEqual(before, {str(p.relative_to(output)): p.read_bytes() for p in output.rglob("*") if p.is_file()})
