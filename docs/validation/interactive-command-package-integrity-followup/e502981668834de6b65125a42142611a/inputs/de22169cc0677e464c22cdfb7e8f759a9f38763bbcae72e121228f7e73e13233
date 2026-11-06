"""Identical synthetic frozen builds for inspecting each variant's packaging operation.

The fixture is prepared outside timing. Counters describe Python API invocations,
not kernel syscalls or allocation events. No target binary is executed.
"""
import hashlib
import json
import tempfile
from pathlib import Path


class ContextWorkload:
    def __init__(self):
        import tiger_provenance as module
        self.module=module
        self.temporary=tempfile.TemporaryDirectory(prefix="codet-context-measure-")
        self.root=Path(self.temporary.name)
        self.folder=self.root/"build/tiger-builds"/("a"*32)
        native=self.folder/"native";native.mkdir(parents=True)
        inputs=self.folder/"inputs";inputs.mkdir()
        sysroot=self.root/"build/tiger-sysroot";sysroot.mkdir()
        (sysroot/'manifest.json').write_text(json.dumps(dict(sysroot_format=2,
            recipe=dict(dependencies=[],patches=[]),prefix='/opt/tigr')))
        for n in range(128):
            (sysroot/str(n)).write_bytes(bytes([n])*4096)
        def store(data):
            digest=hashlib.sha256(data).hexdigest();(inputs/digest).write_bytes(data);return digest
        sources={"src/fixture.cpp":store(b"frozen source")}
        distribution={"notice.txt":store(b"frozen notice")}
        configuration={}
        for name in sorted(getattr(module,"CONFIGURATION",("CMakeCache.txt","compile_commands.json","Makefile"))):
            data=(name+" frozen").encode();(native/name).write_bytes(data);configuration[name]=store(data)
        outputs={}
        for name in module.BINARIES:
            (native/name).write_bytes(name.encode());outputs[name]=module.file_hash(native/name)
        before=dict(sources=sources,distribution=distribution,sysroot=module.tree_hashes(sysroot))
        tools={name:dict(path="/toolchain/"+name,sha256="c"*64,version="fixture") for name in ("gcc","g++","cmake","make")}
        record=dict(format=getattr(module,"BUILD_FORMAT",1),status="built",build_id=self.folder.name,
            before=before,after=before,distribution=distribution,source_snapshot=sources,
            configuration=configuration,outputs=outputs,build_timeout=1800,
            compilers={name:tools[name] for name in ("gcc","g++")},build_tools={name:tools[name] for name in ("cmake","make")},
            commands=[dict(argv=["cmake"],exit=0,outcome="passed",timeout=600),dict(argv=["build"],exit=0,outcome="passed",timeout=1800)],
            sysroot_manifest=store((sysroot/"manifest.json").read_bytes()),
            source_commit="d"*40,source_epoch=1600000000,toolchain_image="sha256:"+"e"*64)
        (self.folder/"provenance.json").write_text(json.dumps(record))
        (self.root/"build/tiger-current.json").write_text(json.dumps(dict(build_id=self.folder.name,sha256=module.file_hash(self.folder/"provenance.json"))))
        self.destination=self.root/"copied";self.destination.mkdir()
        self.counts=dict(context_resolutions=0,sysroot_snapshots=0,direct_file_hash_calls=0,path_stat_calls=0)
        self.stat=Path.stat;self.hash=module.file_hash;self.tree=module.tree_hashes

    def start(self):
        def stat(path,*args,**kwargs):
            self.counts["path_stat_calls"]+=1
            return self.stat(path,*args,**kwargs)
        def file_hash(*args,**kwargs):
            self.counts["direct_file_hash_calls"]+=1
            return self.hash(*args,**kwargs)
        def tree(path,*args,**kwargs):
            if Path(path)==self.root/"build/tiger-sysroot":
                self.counts["sysroot_snapshots"]+=1
            return self.tree(path,*args,**kwargs)
        Path.stat=stat;self.module.file_hash=file_hash;self.module.tree_hashes=tree
        self.root.stat()
        if self.counts['path_stat_calls'] != 1:
            raise ValueError('Path.stat observation positive control failed')
        self.counts['path_stat_positive'] = self.counts['path_stat_calls']
        self.counts['path_stat_calls'] = 0

    def perform(self):
        module=self.module
        if hasattr(module,"resolve"):
            self.counts["context_resolutions"]+=1
            context=module.resolve(self.root)
            values=module.archived_inputs(self.root,context)
            module.copy_audit_inputs(self.root,self.destination,context)
            context.verify_sysroot()
        else:
            self.counts["context_resolutions"]+=3
            module.load(self.root)
            values=module.archived_inputs(self.root)
            module.copy_audit_inputs(self.root,self.destination)
        if values!={"notice.txt":b"frozen notice"}:
            raise ValueError("Build-context work equivalence control failed")
        return values

    def close(self):
        Path.stat=self.stat;self.module.file_hash=self.hash;self.module.tree_hashes=self.tree
        self.temporary.cleanup()
