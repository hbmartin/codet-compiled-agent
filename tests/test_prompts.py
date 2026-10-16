"""Offline source integrity, exact adaptation replay and sparse-catalog regression checks."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("import_prompts", ROOT / "scripts/import-prompts.py")
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)


class PromptBundle(unittest.TestCase):
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

    def test_utf8_bytes_and_newlines_are_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / "unicode.md"
            importer.write_utf8(target, "雪🌲é\n")
            self.assertEqual(target.read_bytes(), "雪🌲é\n".encode("utf-8"))
            self.assertEqual(importer.digest("雪🌲é\n"), hashlib.sha256(target.read_bytes()).hexdigest())

    def test_every_asset_has_complete_replayable_provenance(self):
        bundle = ROOT / "prompts"
        manifest = json.loads((bundle / "manifest.json").read_text())
        for key, asset in manifest["assets"].items():
            adapted = (bundle / "assets" / (key + ".md")).read_text()
            self.assertEqual(importer.digest(adapted), key)
            self.assertEqual(asset["adapted_sha256"], key)
            self.assertTrue(asset["origins"])
            for origin in asset["origins"]:
                text = (bundle / "upstream" / (origin["upstream_sha256"] + ".md")).read_text()
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
                {"slug": "gpt-5.5", "model_messages": {"instructions_template": "You are Codex, fixture."}},
                {"slug": "missing"},
                {"slug": "empty", "model_messages": {"instructions_template": "", "collaboration_modes": {"plan": ""}}}]}))
            supporting = root / "supporting"
            modes = root / "modes"
            for path in (supporting / "templates/compact/prompt.md", supporting / "templates/compact/summary_prefix.md",
                         supporting / "templates/permissions/approval_policy/on_request.md", supporting / "templates/permissions/approval_policy/never.md",
                         modes / "templates/default.md", modes / "templates/plan.md"):
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(path.name)
            output = root / "output"
            importer.import_bundle(catalog, supporting, modes, output)
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertIsNone(manifest["models"]["missing"]["instructions"])
            self.assertEqual(manifest["models"]["empty"]["instructions"], importer.digest(""))
            self.assertEqual(manifest["models"]["empty"]["plan"], importer.digest(""))
            self.assertEqual(manifest["sources"]["models-manager/models.json"], hashlib.sha256(catalog.read_bytes()).hexdigest())
            before = {str(p.relative_to(output)): p.read_bytes() for p in output.rglob("*") if p.is_file()}
            importer.import_bundle(catalog, supporting, modes, output)
            self.assertEqual(before, {str(p.relative_to(output)): p.read_bytes() for p in output.rglob("*") if p.is_file()})
