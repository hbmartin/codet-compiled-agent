#!/usr/bin/env python3
import hashlib,json,pathlib,struct,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
PREFIX='/usr/local/bin/powerpc-apple-darwin8-'

def output(*args):return subprocess.check_output(list(args),text=True)

def runtime_libraries(stage, prefix, system_root=None):
    stage=pathlib.Path(stage).resolve()
    required=['lib/libcurl.dylib','lib/libsqlite3.dylib','lib/libz.dylib',
              'lib/libncurses.dylib','lib/libedit.dylib',
              'libexec/openssl3/lib/libssl.dylib','libexec/openssl3/lib/libcrypto.dylib']
    for relative in required:
        path=stage/prefix.lstrip('/')/relative
        if not path.is_file() or not path.resolve().is_relative_to(stage):
            raise SystemExit('Missing or invalid required runtime library: '+str(path))
    pending=list(stage.rglob('*.dylib')); seen=set(); report=[]
    while pending:
        path=pending.pop()
        if not path.is_file(): raise SystemExit('Missing runtime library: '+str(path))
        library=path.resolve()
        if not library.is_relative_to(stage): raise SystemExit('Runtime library escapes sysroot: '+str(path))
        if library in seen: continue
        seen.add(library)
        with library.open('rb') as source: header=source.read(8)
        if len(header)!=8 or struct.unpack('>II',header)!=(0xfeedface,18):
            raise SystemExit('Non-PPC runtime library: '+str(library))
        dependencies=output(PREFIX+'otool','-L',str(library))
        for line in dependencies.splitlines()[1:]:
            dep=line.strip().split(' ')[0]
            if dep.startswith(prefix+'/'):
                dependency=stage/dep.lstrip('/')
                if not dependency.is_file() or not dependency.resolve().is_relative_to(stage):
                    raise SystemExit('Unresolved target dependency: '+dep)
                pending.append(dependency)
            elif dep.startswith('/usr/lib/') or dep.startswith('/System/Library/Frameworks/'):
                if system_root is not None and not (pathlib.Path(system_root)/dep.lstrip('/')).is_file():
                    raise SystemExit('Unresolved SDK dependency: '+dep)
            else:
                raise SystemExit('Non-target transitive dependency: '+dep)
        report.append({'name':str(library.relative_to(stage)),'dependencies':dependencies,
                       'sha256':hashlib.sha256(library.read_bytes()).hexdigest()})
    return sorted(report,key=lambda item:item['name'])

def main():
    manifest=json.loads((ROOT/'build/tiger-sysroot/manifest.json').read_text());prefix=manifest['prefix']
    report={'target_runtime_verified':False,'compiler':output(PREFIX+'gcc','-dumpfullversion').strip(),'binaries':[]}
    if report['compiler']!='14.2.0':raise SystemExit('Wrong compiler')
    macros=output(PREFIX+'gcc','-m32','-mcpu=750','-mmacosx-version-min=10.4','-dM','-E','-x','c','/dev/null')
    for required in ['#define __SIZEOF_POINTER__ 4','#define __BIG_ENDIAN__ 1','#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ 1040']:
        if required not in macros.splitlines():raise SystemExit('Target macro mismatch: '+required)
    report['target_macros']=macros
    for name in ['codet','codet_tests','codet_persistence_bench']:
        p=ROOT/'build/tiger'/name;data=p.read_bytes();magic,cpu,subtype,kind=struct.unpack('>IIII',data[:16])
        if (magic,cpu,subtype,kind)!=(0xfeedface,18,9,2):raise SystemExit('Unexpected PPC750 Mach-O: '+name)
        dependencies=output(PREFIX+'otool','-L',str(p))
        for line in dependencies.splitlines()[1:]:
            dep=line.strip().split(' ')[0]
            if dep.startswith(prefix+'/') and not (ROOT/'build/tiger-sysroot'/dep.lstrip('/')).is_file():raise SystemExit('Unresolved binary dependency: '+dep)
            if not (dep.startswith('/usr/lib/') or dep.startswith(prefix+'/') or dep in ['/System/Library/Frameworks/'+f+'.framework/Versions/A/'+f for f in ['ApplicationServices','Carbon','CoreFoundation']]):raise SystemExit('Non-target dependency: '+dep)
        commands=output(PREFIX+'otool','-l',str(p))
        if 'LC_RPATH' in commands:raise SystemExit('Tiger does not support LC_RPATH')
        imports=output(PREFIX+'nm','-u',str(p))
        report['binaries'].append({'name':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'dependencies':dependencies,'imports':imports})
    report['runtime_libraries']=runtime_libraries(ROOT/'build/tiger-sysroot',prefix,'/usr/local/MacOSX10.4u.sdk')
    (ROOT/'build/tiger-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('Verified GCC 14.2, PPC32 big-endian/G3 Mach-O, target dependency paths, no LC_RPATH. PPC code was not executed.')

if __name__=='__main__':main()
