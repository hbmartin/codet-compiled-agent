"""Require trustworthy success reports and complete production/rule inventories."""

import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import check_semgrep
from modern_sources import policy_inputs


class SemgrepCoverage(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.expected = ["src/modern/example.cpp", "src/text_compat.c", "include/text_compat.h"]
        for name in self.expected:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("int example(void);\n")
        self.report = dict(version=check_semgrep.VERSION, results=[], errors=[], skipped_rules=[],
                           paths=dict(scanned=self.expected))

    def test_clean_complete_scan_passes(self):
        check_semgrep.validate_scan(self.report, self.expected, self.root)

    def test_findings_errors_skips_version_and_missing_fields_fail(self):
        for field in ("results", "errors", "skipped_rules", "version", "paths"):
            with self.subTest(field=field):
                data = copy.deepcopy(self.report)
                del data[field]
                with self.assertRaises(ValueError):
                    check_semgrep.validate_scan(data, self.expected, self.root)
        for field in ("results", "errors", "skipped_rules"):
            data = copy.deepcopy(self.report)
            data[field] = [{"message": "not a clean scan"}]
            with self.assertRaises(ValueError):
                check_semgrep.validate_scan(data, self.expected, self.root)

    def test_empty_duplicate_missing_extra_and_skipped_sources_fail(self):
        for files in ([], self.expected[:-1], self.expected + [self.expected[0]],
                      self.expected + ["unexpected.cpp"]):
            with self.subTest(files=files), self.assertRaises(ValueError):
                data = copy.deepcopy(self.report)
                data["paths"]["scanned"] = files
                check_semgrep.validate_scan(data, self.expected, self.root)
        data = copy.deepcopy(self.report)
        data["paths"]["skipped"] = [self.expected[0]]
        with self.assertRaises(ValueError):
            check_semgrep.validate_scan(data, self.expected, self.root)

    def test_new_source_and_header_automatically_enter_scope(self):
        before = set(policy_inputs(self.root))
        source = self.root / "src/modern/new.cc"
        header = self.root / "include/codet/new.hpp"
        header.parent.mkdir(parents=True)
        source.write_text("int new_source;\n")
        header.write_text("#pragma once\n")
        self.assertEqual(set(policy_inputs(self.root)) - before, {source, header})
        with self.assertRaises(ValueError):
            check_semgrep.validate_scan(self.report,
                [p.relative_to(self.root).as_posix() for p in policy_inputs(self.root)], self.root)

    def test_rule_results_require_all_ids_actual_detection_and_no_errors(self):
        result = dict(config_missing_tests=[], config_missing_fixtests=[], config_with_errors=[],
                      results={"sample.yaml": {"checks": {"sample": dict(passed=True, errors=[],
                          matches={"sample.cpp": dict(expected_lines=[2], reported_lines=[2])})}}})
        check_semgrep.validate_tests(result, {"sample"})
        for mutate in (
            lambda d: d["results"].clear(),
            lambda d: d["config_with_errors"].append("bad.yaml"),
            lambda d: d["results"]["sample.yaml"]["checks"]["sample"].update(passed=False),
            lambda d: d["results"]["sample.yaml"]["checks"]["sample"].update(matches={}),
            lambda d: d["results"]["sample.yaml"]["checks"]["sample"].update(errors=["timeout"]),
        ):
            data = copy.deepcopy(result)
            mutate(data)
            with self.assertRaises(ValueError):
                check_semgrep.validate_tests(data, {"sample"})
        with self.assertRaises(ValueError):
            check_semgrep.validate_tests(result, {"sample", "untested"})

    def test_failed_version_probe_cannot_leave_a_passing_manifest(self):
        manifest = self.root / "build/semgrep/scan.manifest.json"
        manifest.parent.mkdir(parents=True)
        manifest.write_text('{"passed":true}')
        with patch.object(check_semgrep, "rule_ids", return_value={"sample"}), \
             patch.object(check_semgrep.subprocess, "run", side_effect=FileNotFoundError("missing")):
            with self.assertRaises(FileNotFoundError):
                check_semgrep.run("scan", "missing", self.root)
        self.assertFalse(json.loads(manifest.read_text())["passed"])


if __name__ == "__main__":
    unittest.main()
