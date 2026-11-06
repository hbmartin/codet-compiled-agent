"""Maintain incomplete attempts without machine-local helper dependencies."""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
import evidence_attempt as attempt
import evidence_attempt_audit as audit
import validation_evidence as evidence


class AttemptPublication(unittest.TestCase):
    def setUp(self):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)/"source";self.root.mkdir()
        self.campaign=self.root/"build/campaign";self.campaign.mkdir(parents=True)
        self.output=Path(temporary.name)/"published"
        (self.root/"src").mkdir();(self.root/"src/core.cpp").write_text("original")
        scripts=self.root/"scripts";scripts.mkdir()
        for path in (Path(__file__).resolve().parents[1]/"scripts").glob("*.py"):
            shutil.copy2(path,scripts/path.name)
        blocker=dict(executable="/usr/bin/python3",argv=["python3","-m","unittest"])
        code="raise ValueError("+repr("Concurrent validation/build/analyzer processes: "+repr([blocker]))+")"
        manifest=dict(root=str(self.root),checks=[dict(id="hash-experiment",argv=[sys.executable,"-c",code]),
            dict(id="benchmarks",requires=["hash-experiment"],argv=[sys.executable,"-c","raise RuntimeError('must not launch')"])])
        (self.campaign/"manifest.json").write_text(json.dumps(manifest))
        clean=dict(os.environ);clean.pop("CODET_EVIDENCE_CONTEXT",None)
        context=patch.dict(os.environ,clean,clear=True);context.start();self.addCleanup(context.stop)
        evidence.capture(self.campaign/"manifest.json",self.campaign/"capture")

    def assemble(self):
        return attempt.assemble(self.root,self.campaign,self.output,"User allows delivery with measurements unavailable")

    def test_explicit_inputs_audit_after_relocation_without_tmp_helpers(self):
        self.assemble()
        value=audit.verify(self.output)
        self.assertTrue(value["integrity_verified"])
        self.assertFalse(value["eligible_current"])
        moved=self.output.with_name("relocated");self.output.rename(moved)
        result=subprocess.run([sys.executable,str(moved/"verify-attempt.py"),str(moved)],
            cwd=moved.parent,env={"PATH":os.defpath},capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)
        replay=subprocess.run([sys.executable,str(moved/"assemble-attempt.py"),"--help"],
            cwd=moved.parent,env={"PATH":os.defpath},capture_output=True,text=True,timeout=30)
        self.assertEqual(replay.returncode,0,replay.stderr)
        self.assertIn("--campaign",replay.stdout)
        self.assertNotIn("another Python unittest",(moved/"README.md").read_text())
        report=json.loads((moved/"report.json").read_text())
        self.assertEqual(set(report["assembly_inputs"]),{"evidence_attempt.py","evidence_attempt_audit.py"})
        self.assertNotIn('/private/tmp/codet-',(moved/"evidence_attempt.py").read_text())

    def test_missing_helpers_and_existing_outputs_never_replace_artifacts(self):
        helper=self.root/"scripts/evidence_attempt_audit.py"
        helper.unlink()
        with self.assertRaisesRegex(ValueError,"Stale attempt fingerprints"):
            self.assemble()
        self.output.mkdir();(self.output/"sentinel").write_text("previous")
        with self.assertRaisesRegex(ValueError,"destination already exists"):
            self.assemble()
        self.assertEqual((self.output/"sentinel").read_text(),"previous")

    def test_helper_and_unrelated_required_failures_are_rejected(self):
        self.assemble()
        helper=self.output/"evidence_attempt_audit.py";helper.write_text(helper.read_text()+"\n# changed\n")
        with self.assertRaisesRegex(ValueError,"inventory mismatch"):
            audit.verify(self.output)


if __name__=="__main__":
    unittest.main()
