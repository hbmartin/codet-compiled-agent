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

    def test_each_copied_sysroot_input_is_checked_even_if_source_is_restored(self):
        original=shutil.copy2
        def copy(source,destination,*args,**kwargs):
            result=original(source,destination,*args,**kwargs)
            if Path(source)==self.root/"build/tiger-sysroot/manifest.json":
                Path(destination).write_text("transient wrong manifest")
            return result
        self.archive.write_bytes(b"previous")
        with patch.object(shutil,"copy2",side_effect=copy):
            with self.assertRaisesRegex(ValueError,"input changed while copying"):
                self.package()
        self.assertEqual(self.archive.read_bytes(),b"previous")
        tree=shutil.copytree
        def copy_tree(source,destination,*args,**kwargs):
            result=tree(source,destination,*args,**kwargs)
            if Path(source)==self.root/"build/tiger-sysroot/opt/tigr/share/terminfo":
                (Path(destination)/"d/dumb").write_text("transient wrong terminal data")
            return result
        with patch.object(shutil,"copytree",side_effect=copy_tree):
            with self.assertRaisesRegex(ValueError,"copied sysroot tree differs"):
                self.package()
        self.assertEqual(self.archive.read_bytes(),b"previous")

    def test_same_frozen_build_packages_identically(self):
        self.package();before=self.archive.read_bytes()
        self.package();self.assertEqual(self.archive.read_bytes(),before)

    def test_transient_live_prefix_cannot_select_another_verified_terminfo_tree(self):
        from test_tiger_packaging import portable
        stage=self.root/'build/tiger-sysroot';manifest=stage/'manifest.json';original=manifest.read_bytes()
        for source in list((stage/'opt/tigr/share/terminfo').rglob('*')):
            if source.is_file():
                target=stage/'alternate/share/terminfo'/source.relative_to(stage/'opt/tigr/share/terminfo')
                target.parent.mkdir(parents=True,exist_ok=True);target.write_text('alternate verified data')
        self.record['before']['sysroot']=provenance.tree_hashes(stage)
        self.record['after']=self.record['before']
        provenance.atomic_json(self.folder/'provenance.json',self.record)
        sha=provenance.file_hash(self.folder/'provenance.json')
        provenance.atomic_json(self.root/'build/tiger-current.json',dict(build_id=self.folder.name,sha256=sha))
        self.verification['build_provenance_sha256']=sha
        provenance.atomic_json(self.folder/'verification.json',self.verification)
        verified=provenance.BuildContext.verified_report;dependencies=portable.MachO.dependencies
        def tamper(context):
            value=verified(context)
            altered=json.loads(original);altered['prefix']='/alternate';manifest.write_text(json.dumps(altered))
            return value
        def restore(inspection,binary):
            manifest.write_bytes(original)
            return dependencies(inspection,binary)
        with patch.object(provenance.BuildContext,'verified_report',tamper),patch.object(portable.MachO,'dependencies',restore):
            self.package()
        import tarfile
        with tarfile.open(self.archive) as archive:
            self.assertEqual(archive.extractfile('codet-tiger/share/terminfo/d/dumb').read(),b'terminal',
                             'Transient live prefix selected a different terminfo tree')
            self.assertEqual(archive.extractfile('codet-tiger/sysroot-manifest.json').read(),original)

    def test_pinned_manifest_is_immutable_and_its_archive_digest_is_required(self):
        context=provenance.resolve(self.root)
        with self.assertRaises(TypeError):context.manifest['prefix']='/changed'
        with self.assertRaises(TypeError):context.manifest['recipe']['dependencies']=[]
        path=self.folder/'inputs'/self.record['sysroot_manifest'];path.write_text('corrupt')
        with self.assertRaisesRegex(ValueError,'input changed while copying'):
            provenance.resolve(self.root)

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

    def test_empty_root_is_unset_and_real_cmake_honors_environment_and_cache_precedence(self):
        from dependency_paths import dependency_root
        with patch.dict(os.environ,CODET_DEPENDENCY_ROOT=''):
            self.assertEqual(dependency_root(self.root),(self.root/'.deps').resolve())
        with tempfile.TemporaryDirectory(prefix='codet dependency configure ') as temporary:
            folder=Path(temporary);selected=folder/'empty dependencies';selected.mkdir()
            def configure(output,override,*extra):
                return subprocess.run(['cmake','-S',str(ROOT),'-B',str(output),'-DCODET_FOUNDATION_ONLY=ON',*extra],
                    env=dict(os.environ,CODET_DEPENDENCY_ROOT=override),capture_output=True,text=True,timeout=30)
            output=folder/'default';result=configure(output,'')
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual((output/'CMakeFiles/codet-dependency-root.txt').read_text(),str((ROOT/'.deps').resolve()))
            output=folder/'selected';result=configure(output,str(selected))
            self.assertNotEqual(result.returncode,0)
            self.assertIn('Missing pinned dependency',result.stderr)
            result=configure(output,str(selected),'-DCODET_DEPENDENCY_ROOT='+str(ROOT/'.deps'))
            self.assertEqual(result.returncode,0,result.stderr)
            result=configure(output,str(folder/'another missing location'))
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('CODET_DEPENDENCY_ROOT:PATH='+str((ROOT/'.deps').resolve()),(output/'CMakeCache.txt').read_text())

    def test_recipe_cache_has_no_distribution_metadata_and_rejects_unknown_lock_formats(self):
        from test_tiger_packaging import load_script
        module=load_script('build-tiger-sysroot')
        for name in ('build-tiger-sysroot.py','verified_sources.py','dependency_paths.py','compiled_recipe.py'):
            self.write('scripts/'+name,'stable build helper')
        lock=dict(format=1,dependencies=[],patches=[],ca_bundle=dict(version='old'),tigerports_revision='old')
        stage=self.root/'build/tiger-sysroot'
        fingerprint=module.sysroot_fingerprint(self.root,lock,module.PREFIX,module.FLAGS,'','compiler')
        recipe=dict(dependencies=[],patches=[])
        manifest=dict(sysroot_format=2,recipe=recipe,prefix=module.PREFIX,source_hash=fingerprint,
                      outputs=module.tree_hashes(stage,exclude_manifest=True))
        (stage/'manifest.json').write_text(json.dumps(manifest));before=(stage/'manifest.json').read_bytes()
        lock['ca_bundle']['version']='new';lock['tigerports_revision']='new'
        self.write('tiger-dependencies.lock.json',json.dumps(lock))
        with patch.multiple(module,ROOT=self.root,BUILD=self.root/'build/tiger-deps',STAGE=stage),patch.object(module,'file_hash',return_value='compiler'):
            module.main()
            self.assertEqual((stage/'manifest.json').read_bytes(),before)
            self.assertNotIn('ca_bundle',manifest);self.assertNotIn('tigerports_revision',manifest)
            lock['format']=2;self.write('tiger-dependencies.lock.json',json.dumps(lock))
            with self.assertRaisesRegex(ValueError,'Unsupported Tiger dependency lock format'):
                module.main()

    def test_sysroot_cache_ignores_distribution_policy_but_tracks_build_inputs(self):
        from test_tiger_packaging import load_script
        module=load_script("build-tiger-sysroot")
        for name in ("build-tiger-sysroot.py","verified_sources.py","dependency_paths.py","compiled_recipe.py"):
            self.write("scripts/"+name,"build input "+name)
        lock=dict(format=1,dependencies=[dict(name="curl",sha256="a"*64)],patches=[],ca_bundle=dict(version="old"))
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


class ContextMeasurement(unittest.TestCase):
    def test_startup_failure_restores_patches_tracing_and_temporary_storage(self):
        import tracemalloc

        import evidence_tiger_context_bench
        import evidence_tool_bench
        original_check=subprocess.check_output;original_stat=Path.stat
        start=evidence_tiger_context_bench.ContextWorkload.start
        failure=ValueError('injected context startup failure');created=[]
        def fail(context):
            created.append(context);start(context);raise failure
        with patch.object(evidence_tiger_context_bench.ContextWorkload,'start',fail):
            with self.assertRaises(ValueError) as caught:
                evidence_tool_bench.measure(ROOT,'HEAD','build-context')
        self.assertIs(caught.exception,failure)
        self.assertIs(subprocess.check_output,original_check)
        self.assertIs(Path.stat,original_stat)
        self.assertFalse(tracemalloc.is_tracing())
        self.assertFalse(created[0].root.exists())

    def test_scoped_context_counters_have_an_observed_positive_control(self):
        import evidence_tool_bench
        from evidence_benchmarks import verify_tooling_metrics
        row=evidence_tool_bench.measure(ROOT,"HEAD","build-context")
        row.update(harness="tooling",workload="build-context")
        self.assertEqual(row["context_resolutions"],1)
        self.assertEqual(row["sysroot_snapshots"],2)
        self.assertEqual(row["path_stat_positive"],1)
        verify_tooling_metrics(row)
        row["path_stat_positive"]=0
        with self.assertRaisesRegex(ValueError,"scoped counters"):
            verify_tooling_metrics(row)



class MeasurementExceptionOwnership(unittest.TestCase):

    def test_shared_fixture_cannot_be_shadowed_by_the_variant_support_module(self):
        with tempfile.TemporaryDirectory(prefix='codet shared fixture ') as temporary:
            root=Path(temporary);scripts=root/'scripts';scripts.mkdir()
            (scripts/'evidence_tiger_context_bench.py').write_text("class ContextWorkload:\n    counts={}\n    def start(self):pass\n    def perform(self):return {'variant-only':b'wrong fixture'}\n    def close(self):pass\n")
            marker=root/'core-selected'
            core=(ROOT/'scripts/tiger_provenance.py').read_text()
            (scripts/'tiger_provenance.py').write_text(core+'\nPath('+repr(str(marker))+').write_text("variant core")\n')
            output=root/'measurement.json'
            import evidence_tool_bench
            harness=root/'harness';harness.mkdir()
            entry=harness/'evidence_tool_bench.py';entry.write_bytes(Path(evidence_tool_bench.__file__).read_bytes())
            (harness/'evidence_tiger_context_bench.py').write_bytes((ROOT/'scripts/evidence_tiger_context_bench.py').read_bytes())
            result=subprocess.run([sys.executable,str(entry),str(root),'HEAD','build-context',str(output)],
                capture_output=True,text=True,timeout=30,env=dict(os.environ,PYTHONPATH=str(ROOT/'scripts')))
            self.assertEqual(result.returncode,0,'Shared support snapshot was not used: '+result.stderr)
            row=json.loads(output.read_text())
            self.assertEqual(row['files'],2,'Shared fixture inputs differ between variants')
            self.assertTrue(marker.exists(),'Shared fixture did not select the variant core')

    def test_constructor_failure_releases_storage_before_the_exception_leaves(self):
        import evidence_tiger_context_bench
        native=tempfile.TemporaryDirectory;created=[]
        def temporary(*args,**kwargs):
            value=native(*args,**kwargs);created.append(Path(value.name));return value
        failure=RuntimeError('constructor setup failed')
        with patch.object(evidence_tiger_context_bench.tempfile,'TemporaryDirectory',temporary),patch.object(Path,'write_text',side_effect=failure):
            try:evidence_tiger_context_bench.ContextWorkload()
            except RuntimeError as error:
                self.assertIs(error,failure)
                self.assertTrue(created)
                self.assertFalse(created[0].exists(),'Constructor failure retained owned temporary storage')
            else:self.fail('Constructor failure was not propagated')
    def test_caller_exception_does_not_hide_a_successful_measurements_cleanup_failure(self):
        import evidence_tiger_context_bench
        import evidence_tool_bench
        class Context:
            counts={}
            def start(self):pass
            def perform(self):return {'notice':'frozen'}
            def close(self):raise RuntimeError('close failed')
        with patch.object(evidence_tiger_context_bench,'ContextWorkload',Context):
            try:raise ValueError('unrelated outer handler')
            except ValueError:
                with self.assertRaisesRegex(RuntimeError,'close failed'):
                    evidence_tool_bench.measure(ROOT,'HEAD','build-context')

    def test_measurement_exception_keeps_identity_when_cleanup_also_fails(self):
        import evidence_tiger_context_bench
        import evidence_tool_bench
        failure=ValueError('primary measurement failure')
        class Context:
            counts={}
            def start(self):pass
            def perform(self):raise failure
            def close(self):raise RuntimeError('secondary close failure')
        with patch.object(evidence_tiger_context_bench,'ContextWorkload',Context):
            with self.assertRaises(ValueError) as caught:
                evidence_tool_bench.measure(ROOT,'HEAD','build-context')
        self.assertIs(caught.exception,failure)

class RecipeAdmission(DistributionFixture):
    def test_current_recipe_must_match_before_any_build_directory_is_created(self):
        lockfile=self.root/'tiger-dependencies.lock.json';original=json.loads(lockfile.read_text())
        for field,entry in (('dependencies',dict(name='openssl',version='changed',sha256='a'*64)),
                            ('patches',dict(name='curl',path='new.patch',sha256='b'*64))):
            with self.subTest(field=field):
                lock=json.loads(json.dumps(original));lock[field].append(entry);lockfile.write_text(json.dumps(lock))
                before=set((self.root/'build/tiger-builds').iterdir())
                with patch.object(provenance.subprocess,'check_output',side_effect=self.git):
                    with self.assertRaisesRegex(ValueError,'recipe does not match.*tiger.py sysroot'):
                        provenance.prepare(self.root,self.deps,'image')
                self.assertEqual(set((self.root/'build/tiger-builds').iterdir()),before,
                                 'Recipe preflight allocated a build directory')
        lock=dict(original,format=9);lockfile.write_text(json.dumps(lock))
        with self.assertRaisesRegex(ValueError,'Unsupported Tiger dependency lock format'):
            provenance.prepare(self.root,self.deps,'image')

    def test_distribution_changes_do_not_invalidate_a_compiled_recipe(self):
        lockfile=self.root/'tiger-dependencies.lock.json';lock=json.loads(lockfile.read_text())
        lock['ca_bundle']['version']='new distribution';lock['tigerports_revision']='new provenance'
        lockfile.write_text(json.dumps(lock))
        with patch.object(provenance.subprocess,'check_output',side_effect=self.git):
            folder=provenance.prepare(self.root,self.deps,'image')
        self.assertTrue((folder/'provenance.json').is_file())

    def test_archived_lock_is_checked_instead_of_the_live_checkout(self):
        lockfile=self.root/'tiger-dependencies.lock.json';lock=json.loads(lockfile.read_text())
        lock['dependencies']=[dict(name='curl',version='new')];lockfile.write_text(json.dumps(lock))
        self.assertEqual(provenance.resolve(self.root).manifest['recipe']['dependencies'],())
        record=json.loads((self.folder/'provenance.json').read_text())
        digest=provenance.store(self.folder,lockfile.read_bytes())
        record['distribution']['tiger-dependencies.lock.json']=digest
        record['before']['distribution']['tiger-dependencies.lock.json']=digest
        record['after']=record['before']
        provenance.atomic_json(self.folder/'provenance.json',record)
        provenance.atomic_json(self.root/'build/tiger-current.json',dict(build_id=self.folder.name,
            sha256=provenance.file_hash(self.folder/'provenance.json')))
        with self.assertRaisesRegex(ValueError,'recipe does not match'):
            provenance.resolve(self.root)

    def test_missing_and_legacy_manifests_have_recovery_guidance_without_directories(self):
        manifest=self.root/'build/tiger-sysroot/manifest.json'
        for value in (None,b'{"lock":{},"prefix":"/opt/tigr"}'):
            with self.subTest(value=value):
                if value is None:manifest.unlink()
                else:manifest.write_bytes(value)
                before=set((self.root/'build/tiger-builds').iterdir())
                with self.assertRaisesRegex(ValueError,'Tiger sysroot.*tiger.py sysroot'):
                    provenance.prepare(self.root,self.deps,'image')
                self.assertEqual(set((self.root/'build/tiger-builds').iterdir()),before)

class DependencyReconfiguration(unittest.TestCase):
    def test_nonempty_environment_root_has_exact_precedence(self):
        from dependency_paths import dependency_root
        with tempfile.TemporaryDirectory(prefix='codet root precedence ') as temporary:
            selected=Path(temporary)/'selected'
            with patch.dict(os.environ,CODET_DEPENDENCY_ROOT=str(selected)):
                self.assertEqual(dependency_root(ROOT),selected.resolve(),'Nonempty environment override was ignored')
            with patch.dict(os.environ,CODET_DEPENDENCY_ROOT=''):
                self.assertEqual(dependency_root(ROOT),(ROOT/'.deps').resolve())

    def test_environment_reconfiguration_and_relative_cwd_resolution(self):
        with tempfile.TemporaryDirectory(prefix='codet root reconfigure ') as temporary:
            folder=Path(temporary);caller=folder/'caller';caller.mkdir();output=folder/'build'
            alias=caller/'deps alias';alias.symlink_to((ROOT/'.deps').resolve(),target_is_directory=True)
            def configure(value,*options):
                return subprocess.run(['cmake','-S',str(ROOT),'-B',str(output),'-DCODET_FOUNDATION_ONLY=ON',*options],
                    cwd=caller,env=dict(os.environ,CODET_DEPENDENCY_ROOT=value,PWD='/misleading'),
                    capture_output=True,text=True,timeout=30)
            result=configure('missing')
            self.assertNotEqual(result.returncode,0)
            self.assertFalse((output/'CMakeFiles/codet-dependency-selection.json').exists(),
                             'Failed configuration committed a derived choice')
            result=configure('deps alias');self.assertEqual(result.returncode,0,result.stderr)
            expected=str((ROOT/'.deps').resolve())
            self.assertEqual((output/'CMakeFiles/codet-dependency-root.txt').read_text(),expected,
                             'CMake did not resolve the root from invocation cwd')
            code='import sys;sys.path.insert(0,'+repr(str(ROOT/'scripts'))+');from dependency_paths import dependency_root;from pathlib import Path;print(dependency_root(Path('+repr(str(ROOT))+')))'
            actual=subprocess.check_output([sys.executable,'-c',code],cwd=caller,
                env=dict(os.environ,CODET_DEPENDENCY_ROOT='deps alias',PWD='/misleading'),text=True,timeout=30).strip()
            self.assertEqual(actual,expected)
            result=configure('missing','-DCODET_DEPENDENCY_ROOT=deps alias')
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn('CODET_DEPENDENCY_ROOT:PATH='+expected,(output/'CMakeCache.txt').read_text())
            result=configure('still missing');self.assertEqual(result.returncode,0,result.stderr)
            # Old caches cannot reveal whether their nonempty value was explicit or derived.
            result=configure('missing','-UCODET_DEPENDENCY_ROOT');self.assertNotEqual(result.returncode,0)
            result=configure('deps alias');self.assertEqual(result.returncode,0,result.stderr)

    def test_later_generator_failure_keeps_a_validated_selection_until_reset(self):
        for generator in ('Ninja','Unix Makefiles'):
            with self.subTest(generator=generator),tempfile.TemporaryDirectory(prefix='codet generation error ') as temporary:
                folder=Path(temporary);source=folder/'source';source.mkdir();output=folder/'build'
                text=('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES NONE)\n'
                    +'include("'+str(ROOT/'cmake/compiler-identity.cmake')+'")\n'
                    +'include("'+str(ROOT/'cmake/dependency-root.cmake')+'")\n')
                (source/'broken.c').write_text('int main(void){return 0;}')
                (source/'CMakeLists.txt').write_text(text+'add_executable(broken broken.c)\n')
                result=subprocess.run(['cmake','-S',str(source),'-B',str(output),'-G',generator],
                    env=dict(os.environ,CODET_DEPENDENCY_ROOT=str(ROOT/'.deps')),capture_output=True,text=True,timeout=30)
                self.assertNotEqual(result.returncode,0)
                self.assertRegex(result.stderr.lower(),r'link(?:er)? language')
                pin=output/'CMakeFiles/codet-dependency-selection.json'
                self.assertEqual(json.loads(pin.read_text())['root'],str((ROOT/'.deps').resolve()))
                (source/'CMakeLists.txt').write_text(text+'add_custom_target(ok ALL)\n')
                result=subprocess.run(['cmake','-S',str(source),'-B',str(output)],
                    env=dict(os.environ,CODET_DEPENDENCY_ROOT='missing'),capture_output=True,text=True,timeout=30)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(json.loads(pin.read_text())['root'],str((ROOT/'.deps').resolve()))

    def test_successful_selection_survives_ninja_and_make_regeneration_until_reset(self):
        for generator in ('Ninja','Unix Makefiles'):
            with self.subTest(generator=generator), tempfile.TemporaryDirectory(prefix='codet pinned root ') as temporary:
                folder=Path(temporary);source=folder/'source';source.mkdir();caller=folder/'caller';caller.mkdir()
                selected=caller/'relative deps';selected.symlink_to((ROOT/'.deps').resolve(),target_is_directory=True)
                output=folder/'output'
                text=('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES NONE)\n'
                    +'include("'+str(ROOT/'cmake/compiler-identity.cmake')+'")\n'
                    +'include("'+str(ROOT/'cmake/dependency-root.cmake')+'")\n'
                    +'if(NOT EXISTS "${CODET_DEPENDENCY_ROOT}/json/CMakeLists.txt")\nmessage(FATAL_ERROR "missing selected dependencies")\nendif()\n'
                    +'add_custom_target(probe ALL COMMAND "${CMAKE_COMMAND}" -E echo ok)\n')
                cmake=source/'CMakeLists.txt';cmake.write_text(text)
                def configure(value,*extra,source=source,output=output,generator=generator,caller=caller):
                    return subprocess.run(['cmake','-S',str(source),'-B',str(output),'-G',generator,*extra],cwd=caller,
                        env=dict(os.environ,CODET_DEPENDENCY_ROOT=value,PWD='/misleading'),capture_output=True,text=True,timeout=30)
                result=configure('missing');self.assertNotEqual(result.returncode,0)
                self.assertFalse((output/'CMakeFiles/codet-dependency-selection.json').exists())
                result=configure('relative deps');self.assertEqual(result.returncode,0,result.stderr)
                expected=str((ROOT/'.deps').resolve())
                for value in (None,'different relative root'):
                    cmake.write_text(cmake.read_text()+'\n# regeneration\n')
                    env=dict(os.environ);env.pop('CODET_DEPENDENCY_ROOT',None)
                    if value is not None:env['CODET_DEPENDENCY_ROOT']=value
                    result=subprocess.run(['cmake','--build',str(output)],cwd=caller,env=env,capture_output=True,text=True,timeout=30)
                    self.assertEqual(result.returncode,0,'Automatic regeneration changed dependency availability: '+result.stderr+result.stdout)
                    actual=json.loads((output/'CMakeFiles/codet-dependency-selection.json').read_text())
                    self.assertEqual(actual['root'],expected,'Automatic regeneration changed the selected dependency tree')
                    self.assertEqual(actual['origin'],'environment')
                result=configure('missing');self.assertEqual(result.returncode,0,result.stderr)
                result=configure('missing','-UCODET_DEPENDENCY_ROOT');self.assertNotEqual(result.returncode,0)
                self.assertFalse((output/'CMakeFiles/codet-dependency-selection.json').exists())
                result=configure('relative deps');self.assertEqual(result.returncode,0,result.stderr)
                result=configure('missing','-DCODET_DEPENDENCY_ROOT=relative deps');self.assertEqual(result.returncode,0,result.stderr)
                self.assertIn('CODET_DEPENDENCY_ROOT:PATH='+expected,(output/'CMakeCache.txt').read_text())
                result=configure('missing');self.assertEqual(result.returncode,0,result.stderr)

    @staticmethod
    def dependency_probe(folder, generator):
        source=folder/'source';source.mkdir();output=folder/'output'
        text=('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES NONE)\n'
              +'include("'+str(ROOT/'cmake/dependency-root.cmake')+'")\n'
              +'add_custom_target(ok ALL)\n')
        (source/'CMakeLists.txt').write_text(text)
        def configure(value,*extra):
            return subprocess.run(['cmake','-S',str(source),'-B',str(output),'-G',generator,*extra],
                env=dict(os.environ,CODET_DEPENDENCY_ROOT=str(value)),capture_output=True,text=True,timeout=30)
        return source,output,text,configure

    def test_send_error_never_commits_a_first_selection(self):
        for generator in ('Ninja','Unix Makefiles'):
            for error in ('SEND_ERROR','FATAL_ERROR'):
                with self.subTest(generator=generator,error=error),tempfile.TemporaryDirectory() as d:
                    source,output,text,configure=self.dependency_probe(Path(d),generator)
                    (source/'CMakeLists.txt').write_text(text+'message('+error+' "intentional configuration failure")\n')
                    result=configure('/root-A');self.assertNotEqual(result.returncode,0)
                    self.assertFalse((output/'CMakeFiles/codet-dependency-selection.json').exists(),
                        'Failed configuration committed a derived choice')
                    (source/'CMakeLists.txt').write_text(text)
                    result=configure('/root-B');self.assertEqual(result.returncode,0,result.stderr)
                    selection=json.loads((output/'CMakeFiles/codet-dependency-selection.json').read_text())
                    self.assertEqual(selection['root'],'/root-B','Failed provisional selection became explicit')

    def test_empty_and_unset_overrides_reset_explicit_and_derived_selections(self):
        for generator in ('Ninja','Unix Makefiles'):
            for reset in ('-DCODET_DEPENDENCY_ROOT=','-UCODET_DEPENDENCY_ROOT'):
                with self.subTest(generator=generator,reset=reset),tempfile.TemporaryDirectory() as d:
                    _,output,_,configure=self.dependency_probe(Path(d),generator)
                    for explicit in (True,False):
                        args=['-DCODET_DEPENDENCY_ROOT=/root-A'] if explicit else [reset]
                        result=configure('/root-A',*args);self.assertEqual(result.returncode,0,result.stderr)
                        result=configure('/root-B',reset);self.assertEqual(result.returncode,0,
                            'Empty or unset override did not reset selection: '+result.stderr)
                        self.assertEqual(json.loads((output/'CMakeFiles/codet-dependency-selection.json').read_text())['root'],'/root-B')

    def test_normal_variables_are_scoped_and_do_not_replace_a_derived_pin(self):
        for generator in ('Ninja','Unix Makefiles'):
            with self.subTest(generator=generator),tempfile.TemporaryDirectory() as d:
                source,output,text,configure=self.dependency_probe(Path(d),generator)
                normal='set(CODET_DEPENDENCY_ROOT "/normal-root")\n'
                (source/'CMakeLists.txt').write_text(text.replace('include(',normal+'include(',1))
                result=configure('/environment-A');self.assertEqual(result.returncode,0,result.stderr)
                cache=(output/'CMakeCache.txt').read_text()
                self.assertNotIn('CODET_DEPENDENCY_ROOT:PATH=',cache,'Normal variable was promoted into cache')
                (source/'CMakeLists.txt').write_text(text)
                result=configure('/environment-A');self.assertEqual(result.returncode,0,result.stderr)
                pin=output/'CMakeFiles/codet-dependency-selection.json';prior=pin.read_bytes()
                (source/'CMakeLists.txt').write_text(text.replace('include(',normal+'include(',1))
                result=configure('/environment-B');self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual((output/'CMakeFiles/codet-dependency-root.txt').read_text(),'/normal-root')
                self.assertEqual(pin.read_bytes(),prior,'Scoped value replaced the remembered pin')
                (source/'CMakeLists.txt').write_text(text)
                result=configure('/environment-B');self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual((output/'CMakeFiles/codet-dependency-root.txt').read_text(),'/environment-A')
                # A real cache override remains authoritative even with a normal variable.
                (source/'CMakeLists.txt').write_text(text.replace('include(',normal+'include(',1))
                result=configure('/environment-B','-DCODET_DEPENDENCY_ROOT=/explicit')
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual((output/'CMakeFiles/codet-dependency-root.txt').read_text(),'/explicit')

    def test_invalid_records_have_one_actionable_error_and_reset_recovers(self):
        with tempfile.TemporaryDirectory() as d:
            _,output,_,configure=self.dependency_probe(Path(d),'Ninja')
            result=configure('/root-A');self.assertEqual(result.returncode,0,result.stderr)
            pin=output/'CMakeFiles/codet-dependency-selection.json';valid=json.loads(pin.read_text())
            for invalid in ('{bad',json.dumps(dict(valid,root=3)),json.dumps(dict(valid,format=99)),json.dumps({'root':'/old','origin':'environment','source':valid['source']})):
                with self.subTest(invalid=invalid):
                    pin.write_text(invalid)
                    result=configure('/root-B');self.assertNotEqual(result.returncode,0)
                    self.assertIn('-UCODET_DEPENDENCY_ROOT',result.stderr)
                    self.assertIn('codet-dependency-selection.json',result.stderr)
                    self.assertNotIn('string sub-command JSON',result.stderr,'Pin parsing leaked raw errors')
                    pin.write_text(json.dumps(valid))
            pin.write_text('{bad')
            result=configure('/root-B','-DCODET_DEPENDENCY_ROOT=');self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(json.loads(pin.read_text())['root'],'/root-B')

    def test_missing_dependency_message_identifies_the_pinned_repair_destination(self):
        with tempfile.TemporaryDirectory(prefix='codet missing pinned ') as d:
            folder=Path(d);source,output,_,configure=self.dependency_probe(folder,'Ninja')
            selected=folder/'selected deps'
            result=configure(selected);self.assertEqual(result.returncode,0,result.stderr)
            actual=(ROOT/'CMakeLists.txt').read_text();begin=actual.index('foreach(dep googletest')
            validation=actual[begin:actual.index('endforeach()',begin)+len('endforeach()')]
            (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES NONE)\ninclude("'+str(ROOT/'cmake/dependency-root.cmake')+'")\n'+validation+'\n')
            result=configure('/wrong-environment');self.assertNotEqual(result.returncode,0)
            self.assertIn(' '.join(str(selected).split()),' '.join(result.stderr.split()))
            self.assertIn('-UCODET_DEPENDENCY_ROOT',result.stderr)
            self.assertIn('CODET_DEPENDENCY_ROOT=',result.stderr)

if __name__ == "__main__":
    unittest.main()
