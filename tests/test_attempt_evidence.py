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

    def refresh_inventory(self):
        evidence.atomic_json(self.output/'inventory.json',{
            str(p.relative_to(self.output)):audit.digest(p) for p in self.output.rglob('*')
            if p.is_file() and p.name!='inventory.json'})

    def test_successful_check_audits_on_the_running_supported_interpreter(self):
        manifest_path=self.campaign/'manifest.json'
        manifest=json.loads(manifest_path.read_text())
        manifest['checks'].insert(0,dict(id='successful',argv=[sys.executable,'-c','print("success")']))
        manifest_path.write_text(json.dumps(manifest));shutil.rmtree(self.campaign/'capture')
        evidence.capture(manifest_path,self.campaign/'capture')
        self.assemble()
        result=subprocess.run([sys.executable,str(self.output/'verify-attempt.py'),str(self.output)],
            capture_output=True,text=True,timeout=30,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['checks'],3)

    def test_measurements_and_reported_counts_must_both_be_empty(self):
        self.assemble();report_path=self.output/'report.json';original=json.loads(report_path.read_text())
        for samples,count in (([{'unbound':'sample'}],0),([],17),([],False),({},0)):
            with self.subTest(samples=samples,count=count):
                (self.output/'measurements.json').write_text(json.dumps(samples))
                evidence.atomic_json(report_path,dict(original,measurement_samples=count));self.refresh_inventory()
                with self.assertRaisesRegex(ValueError,'empty measurement list and zero samples'):
                    audit.verify(self.output)
        (self.output/'measurements.json').write_text('broken JSON');self.refresh_inventory()
        with self.assertRaisesRegex(ValueError,'malformed attempt measurements'):
            audit.verify(self.output)

    def test_failed_process_inspection_remains_ineligible_and_source_bound(self):
        manifest_path=self.campaign/'manifest.json';manifest=json.loads(manifest_path.read_text())
        code=('import sys,subprocess;sys.path.insert(0,"scripts");from unittest.mock import patch;'
              'from evidence_processes import require_quiet_processes\n'
              'with patch("subprocess.check_output",side_effect=subprocess.CalledProcessError(1,"inspection",stderr="native inspection failed")):require_quiet_processes()')
        manifest['checks'][0]['argv']=[sys.executable,'-c',code]
        manifest_path.write_text(json.dumps(manifest));shutil.rmtree(self.campaign/'capture')
        evidence.capture(manifest_path,self.campaign/'capture');self.assemble()
        value=audit.verify(self.output)
        self.assertFalse(value['eligible_current']);self.assertEqual(value['measurement_samples'],0)
        self.assertEqual(value['unmet_required'],{'hash-experiment':'failed','benchmarks':'blocked'})
        from evidence_processes import validate_inspection_failure
        sha=evidence.validation_hashes(self.root)['scripts/evidence_processes.py']
        for observation in (dict(inspector_sha256='0'*64,deadline_seconds=5,reason='CalledProcessError',returncode=1,stderr='failed'),
                            dict(inspector_sha256=sha,deadline_seconds=5,reason='CalledProcessError',returncode=0,stderr='')):
            with self.assertRaisesRegex(ValueError,'process inspection'):
                validate_inspection_failure(observation,sha)

    def test_unexpected_files_are_rejected_with_identifying_details(self):
        self.assemble();(self.output/'.DS_Store').write_text('Finder metadata')
        with self.assertRaisesRegex(ValueError,r'extra=.*\.DS_Store'):
            audit.verify(self.output)

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
