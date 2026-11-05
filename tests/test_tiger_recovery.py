"""Actual configure artifacts and pinned package operations; no PPC execution."""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from test_tiger_packaging import ROOT, DistributionFixture, provenance


class CompilerIdentity(unittest.TestCase):
    def test_real_configure_with_toolchain_variables_and_spaces(self):
        cmake = shutil.which("cmake")
        cc, cxx = shutil.which("cc"), shutil.which("c++")
        self.assertTrue(cmake and cc and cxx, "Host configure tests require CMake and native compilers")
        for cached in (False, True):
            with self.subTest(cached=cached), tempfile.TemporaryDirectory(prefix="codet configure ") as d:
                root = Path(d)/"source with spaces";root.mkdir();output=Path(d)/"output with spaces"
                (root/"CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(probe C CXX)\ninclude("'+str(ROOT/"cmake/compiler-identity.cmake")+'")\n')
                suffix=' CACHE FILEPATH ""' if cached else ''
                toolchain=root/"toolchain.cmake"
                toolchain.write_text('set(CMAKE_C_COMPILER "'+cc+'"'+suffix+')\nset(CMAKE_CXX_COMPILER "'+cxx+'"'+suffix+')\n')
                subprocess.run([cmake,"-S",str(root),"-B",str(output),"-G","Unix Makefiles",
                    "-DCMAKE_TOOLCHAIN_FILE="+str(toolchain),"-DCMAKE_BUILD_TYPE=Release"],
                    check=True,capture_output=True,text=True,timeout=30)
                artifact=json.loads((output/"codet-compiler-identity.json").read_text())
                tools={name:provenance.compiler(Path(artifact["compilers"][language]["path"]))
                       for language,name in (("C","gcc"),("CXX","g++"))}
                try:
                    actual=provenance.configured_compilers(root,output,tools)
                except Exception as error:
                    self.fail("Valid configure metadata was rejected: "+repr(error))
                self.assertEqual(actual,artifact)
                cache=(output/"CMakeCache.txt").read_text()
                if not cached:
                    self.assertNotIn("CMAKE_C_COMPILER:",cache)
                    self.assertNotIn("CMAKE_CXX_COMPILER:",cache)
                artifact["source"]="/wrong/source"
                (output/"codet-compiler-identity.json").write_text(json.dumps(artifact))
                with self.assertRaisesRegex(ValueError,"source/build identity"):
                    provenance.configured_compilers(root,output,tools)
                (output/"codet-compiler-identity.json").unlink()
                with self.assertRaisesRegex(ValueError,"compiler identity artifact"):
                    provenance.configured_compilers(root,output,tools)


class PinnedPackaging(DistributionFixture):
    def test_current_pointer_can_change_between_copy_stages_without_mixing_builds(self):
        original=provenance.archived_inputs
        calls=[]
        def read(root,context):
            result=original(root,context)
            # Subsequent helpers must never consult this now-invalid pointer.
            (root/"build/tiger-current.json").write_text('{"build_id":"replacement"}')
            calls.append(context.folder.name)
            return result
        with patch.object(provenance,"archived_inputs",side_effect=read), patch.object(provenance,"resolve",wraps=provenance.resolve) as resolve:
            try:
                self.package()
            except Exception as error:
                self.fail("Pinned package consulted a replacement pointer: "+repr(error))
            self.assertEqual(resolve.call_count,1,"Packaging reloaded its mutable build pointer")
        import tarfile
        with tarfile.open(self.archive) as archive:
            audit=json.load(archive.extractfile("codet-tiger/build-provenance.json"))
            self.assertEqual(audit["build_id"],self.folder.name)
            self.assertEqual(archive.extractfile("codet-tiger/SOURCE.patch").read(),b"frozen patch\n")
        self.assertEqual(calls,[self.folder.name])

    def test_post_admission_sysroot_changes_reject_before_publication(self):
        original=provenance.copy_audit_inputs
        def copy(root,destination,context):
            original(root,destination,context)
            (root/"build/tiger-sysroot/changed").write_text("changed")
        self.archive.write_bytes(b"previous")
        with patch.object(provenance,"copy_audit_inputs",side_effect=copy):
            with self.assertRaisesRegex(ValueError,"sysroot changed"):
                self.package()
        self.assertEqual(self.archive.read_bytes(),b"previous")

    def test_same_frozen_build_packages_identically(self):
        self.package();before=self.archive.read_bytes()
        self.package();self.assertEqual(self.archive.read_bytes(),before)

    def test_legacy_format_requires_new_build(self):
        record=dict(self.record,format=1)
        provenance.atomic_json(self.folder/"provenance.json",record)
        provenance.atomic_json(self.root/"build/tiger-current.json",dict(build_id=self.folder.name,sha256=provenance.file_hash(self.folder/"provenance.json")))
        with self.assertRaisesRegex(ValueError,"source-bound Tiger build"):
            provenance.resolve(self.root)

    def test_timeout_budget_and_failure_preserve_previous_build(self):
        with patch.object(provenance.subprocess,"check_output",side_effect=self.git):
            folder=provenance.prepare(self.root,self.deps,"sha256:"+"d"*64)
        previous=(self.root/"build/tiger-current.json").read_bytes()
        failure=subprocess.TimeoutExpired(["cmake","--build"],2300)
        with patch.object(provenance,"compiler",side_effect=lambda p:dict(path=str(p),sha256="b"*64,version="fixture")), patch.object(provenance,"run",side_effect=[subprocess.CompletedProcess(["cmake"],0),failure]) as run:
            with self.assertRaises(subprocess.TimeoutExpired) as caught:
                provenance.build(self.root,folder,2300)
        self.assertIs(caught.exception,failure)
        self.assertEqual([c.kwargs["timeout"] for c in run.call_args_list],[600,2300])
        record=json.loads((folder/"provenance.json").read_text())
        self.assertEqual(record["commands"][-1]["outcome"],"timeout")
        self.assertEqual(record["build_timeout"],2300)
        self.assertEqual((self.root/"build/tiger-current.json").read_bytes(),previous)
        for value in (0,-1):
            with self.assertRaisesRegex(ValueError,"positive"):
                provenance.build(self.root,folder,value)


class DependencyInputs(DistributionFixture):
    def test_offline_fetch_uses_the_configured_root_and_correct_checksum_name(self):
        from test_tiger_packaging import load_script
        fetch=load_script("fetch-deps")
        self.write("dependencies.lock.json",json.dumps(dict(dependencies=[])))
        lock=json.loads((self.root/"tiger-dependencies.lock.json").read_text());lock["dependencies"]=[]
        (self.root/"tiger-dependencies.lock.json").write_text(json.dumps(lock))
        destination=self.root/"selected/dependencies";destination.mkdir(parents=True)
        (destination/"tiger-cacert.pem").write_bytes(self.ca.read_bytes())
        with patch.object(fetch,"ROOT",self.root), patch.dict(os.environ,CODET_DEPENDENCY_ROOT=str(destination)), patch.object(sys,"argv",["fetch","--offline","--tiger"]):
            fetch.main()
        self.assertEqual((destination/"tiger-cacert.pem.sha256").read_text(),
            lock["ca_bundle"]["sha256"]+"  tiger-cacert.pem\n")
        self.assertFalse((self.deps/"tiger-cacert.pem.sha256").exists())

    def test_sysroot_cache_ignores_distribution_policy_but_tracks_build_inputs(self):
        from test_tiger_packaging import load_script
        module=load_script("build-tiger-sysroot")
        for name in ("build-tiger-sysroot.py","verified_sources.py","dependency_paths.py"):
            self.write("scripts/"+name,"build input "+name)
        lock=dict(dependencies=[dict(name="curl",sha256="a"*64)],patches=[],ca_bundle=dict(version="old"))
        def key():
            return module.sysroot_fingerprint(self.root,lock,"/opt/tigr","flags","image","compiler")
        original=key()
        self.write("scripts/tiger_distribution.py","changed documents and archive policy")
        lock["ca_bundle"]["version"]="new"
        self.assertEqual(key(),original)
        lock["dependencies"][0]["sha256"]="b"*64
        self.assertNotEqual(key(),original)
        lock["dependencies"][0]["sha256"]="a"*64
        self.write("scripts/dependency_paths.py","changed build behavior")
        self.assertNotEqual(key(),original)


if __name__ == "__main__":
    unittest.main()
