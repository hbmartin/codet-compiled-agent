#!/usr/bin/env python3
import hashlib,json,pathlib,struct,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
PREFIX='/usr/local/bin/powerpc-apple-darwin8-'

def output(*args):return subprocess.check_output(list(args),text=True)

def main():
    manifest=json.loads((ROOT/'build/tiger-sysroot/manifest.json').read_text());prefix=manifest['prefix']
    report={'target_runtime_verified':False,'compiler':output(PREFIX+'gcc','-dumpfullversion').strip(),'binaries':[]}
    if report['compiler']!='14.2.0':raise SystemExit('Wrong compiler')
    macros=output(PREFIX+'gcc','-m32','-mcpu=750','-mmacosx-version-min=10.4','-dM','-E','-x','c','/dev/null')
    for required in ['#define __SIZEOF_POINTER__ 4','#define __BIG_ENDIAN__ 1','#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ 1040']:
        if required not in macros.splitlines():raise SystemExit('Target macro mismatch: '+required)
    report['target_macros']=macros
    for name in ['codet','codet_tests']:
        p=ROOT/'build/tiger'/name;data=p.read_bytes();magic,cpu,subtype,kind=struct.unpack('>IIII',data[:16])
        if (magic,cpu,subtype,kind)!=(0xfeedface,18,9,2):raise SystemExit('Unexpected PPC750 Mach-O: '+name)
        dependencies=output(PREFIX+'otool','-L',str(p))
        for line in dependencies.splitlines()[1:]:
            dep=line.strip().split(' ')[0]
            if not (dep.startswith('/usr/lib/') or dep.startswith(prefix+'/') or dep in ['/System/Library/Frameworks/'+f+'.framework/Versions/A/'+f for f in ['ApplicationServices','Carbon','CoreFoundation']]):raise SystemExit('Non-target dependency: '+dep)
        commands=output(PREFIX+'otool','-l',str(p))
        if 'LC_RPATH' in commands:raise SystemExit('Tiger does not support LC_RPATH')
        imports=output(PREFIX+'nm','-u',str(p))
        report['binaries'].append({'name':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'dependencies':dependencies,'imports':imports})
    report['runtime_libraries']=[];seen=set()
    for directory in [ROOT/'build/tiger-sysroot'/prefix.lstrip('/')/'lib',ROOT/'build/tiger-sysroot'/prefix.lstrip('/')/'libexec/openssl3/lib']:
        for library in sorted(directory.glob('*.dylib')):
            if library.resolve() in seen:continue
            seen.add(library.resolve());dependencies=output(PREFIX+'otool','-L',str(library))
            if struct.unpack('>II',library.read_bytes()[:8])!=(0xfeedface,18):raise SystemExit('Non-PPC runtime library: '+str(library))
            for line in dependencies.splitlines()[1:]:
                dep=line.strip().split(' ')[0]
                if not(dep.startswith('/usr/lib/') or dep.startswith('/System/Library/Frameworks/') or dep.startswith(prefix+'/')):
                    raise SystemExit('Non-target transitive dependency: '+dep)
            report['runtime_libraries'].append({'name':library.name,'dependencies':dependencies})
    (ROOT/'build/tiger-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('Verified GCC 14.2, PPC32 big-endian/G3 Mach-O, target dependency paths, no LC_RPATH. PPC code was not executed.')

if __name__=='__main__':main()
