#!/usr/bin/env python3
"""Build/verify/package the modern PPC agent without executing it."""
import argparse,json,os,pathlib,subprocess,tarfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
IMAGE=os.environ.get('CODET_TOOLCHAIN_IMAGE','codet-tiger-build:gcc14')

def docker(args):
    command=['docker','run','--rm','--pull=never','--platform','linux/amd64','--network','none','--read-only','--cap-drop','ALL','--security-opt','no-new-privileges','--tmpfs','/tmp:rw,noexec,nosuid,size=256m','--mount',f'type=bind,src={ROOT},dst=/project,readonly','--mount',f'type=bind,src={ROOT}/build,dst=/project/build','--workdir','/project','--entrypoint',args[0],IMAGE,*args[1:]]
    for key in ['TIGERPORTS_PREFIX','JOBS']:
        if key in os.environ:command[2:2]=['--env',key+'='+os.environ[key]]
    subprocess.run(command,check=True)

def main():
    parser=argparse.ArgumentParser(__doc__);parser.add_argument('action',choices=['image','sysroot','build','verify','package']);args=parser.parse_args();(ROOT/'build').mkdir(exist_ok=True)
    if args.action=='image':
        subprocess.run(['docker','build','--platform','linux/amd64','-t',IMAGE,'-f',str(ROOT/'cmake/Dockerfile.tiger'),str(ROOT/'cmake')],check=True);return
    if args.action=='sysroot':docker(['/usr/bin/python3','scripts/build-tiger-sysroot.py']);return
    if args.action=='build':
        configure=['/usr/bin/cmake','-S','.','-B','build/tiger','-DCMAKE_BUILD_TYPE=Release','-DCODET_TIGERPORTS_PREFIX='+os.environ.get('TIGERPORTS_PREFIX','/opt/tigr')]
        if not (ROOT/'build/tiger/CMakeCache.txt').exists():configure.append('-DCMAKE_TOOLCHAIN_FILE=cmake/tiger.cmake')
        docker(configure)
        docker(['/usr/bin/cmake','--build','build/tiger','-j4']);return
    if args.action=='verify':docker(['/usr/bin/python3','scripts/verify-tiger.py']);return
    docker(['/usr/bin/python3','scripts/verify-tiger.py'])
    dist=ROOT/'build/dist';dist.mkdir(exist_ok=True)
    with tarfile.open(dist/'codet-tiger-ppc.tar.gz','w:gz') as bundle:
        for source,target in [('build/tiger/codet','codet'),('build/tiger/codet_tests','codet_tests'),('scripts/tiger-acceptance.sh','tiger-acceptance.sh'),('docs/TIGER-ACCEPTANCE.md','TIGER-ACCEPTANCE.md'),('docs/MODERN-AGENT.md','MODERN-AGENT.md'),('docs/FEATURE-PORT.md','FEATURE-PORT.md'),('docs/REVIEW-FOLLOWUP.md','REVIEW-FOLLOWUP.md'),('docs/validation/followup-analysis.json','validation/followup-analysis.json'),('docs/validation/remaining-review.json','validation/remaining-review.json'),('tests/fixtures/events-v1.jsonl','tests/fixtures/events-v1.jsonl'),('dependencies.lock.json','dependencies.lock.json'),('tiger-dependencies.lock.json','tiger-dependencies.lock.json'),('build/tiger-sysroot/manifest.json','sysroot-manifest.json'),('build/tiger-verification.json','static-verification.json'),('LICENSE.md','LICENSE.md'),('licenses/codex-APACHE-2.0.txt','licenses/codex-APACHE-2.0.txt'),('licenses/codex-NOTICE.txt','licenses/codex-NOTICE.txt')]:
            bundle.add(ROOT/source,arcname='codet-tiger/'+target)
        for name,path in [('googletest','LICENSE'),('json','LICENSE.MIT'),('tomlplusplus','LICENSE'),('json-schema-validator','LICENSE')]:
            p=ROOT/'.deps'/name/path
            if p.exists():bundle.add(p,arcname='codet-tiger/licenses/'+name+'.txt')
    print(dist/'codet-tiger-ppc.tar.gz')

if __name__=='__main__':main()
