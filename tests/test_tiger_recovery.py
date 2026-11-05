"""Actual configure artifacts and pinned package operations; no PPC execution."""
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_tiger_packaging import ROOT, provenance


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


if __name__ == "__main__":
    unittest.main()
