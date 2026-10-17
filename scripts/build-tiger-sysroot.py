#!/usr/bin/env python3
"""Offline cross build. Run inside the pinned toolchain image; never executes PPC code."""
import hashlib,json,os,pathlib,shutil,subprocess,sys,fcntl
from verified_sources import extract_verified, file_hash, tree_hashes, remove_tree
ROOT=pathlib.Path(__file__).resolve().parents[1]
BUILD=ROOT/'build/tiger-deps'
STAGE=ROOT/'build/tiger-sysroot'
PREFIX=os.environ.get('TIGERPORTS_PREFIX','/opt/tigr')
CC='/usr/local/bin/powerpc-apple-darwin8-gcc'
SDK='/usr/local/MacOSX10.4u.sdk'
FLAGS=f'-O2 -m32 -mcpu=750 -mmacosx-version-min=10.4 -isysroot {SDK}'
JOBS=os.environ.get('JOBS','4')
LOG=BUILD/'build.log'

def run(args,cwd,env=None):
    print(' '.join(map(str,args)),flush=True)
    with LOG.open('a') as out:
      result=subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=out,stderr=subprocess.STDOUT)
    if result.returncode:
      print('\n'.join(LOG.read_text(errors='replace').splitlines()[-65:]),flush=True)
      raise SystemExit(result.returncode)

def main():
    global STAGE
    if not PREFIX.startswith('/') or '..' in pathlib.Path(PREFIX).parts:
      raise SystemExit('TIGERPORTS_PREFIX must be an absolute logical install prefix')
    BUILD.mkdir(parents=True,exist_ok=True)
    build_lock = (BUILD / "sysroot.lock").open("a")
    fcntl.flock(build_lock, fcntl.LOCK_EX)
    lock=json.loads((ROOT/'tiger-dependencies.lock.json').read_text())
    for dep in lock['dependencies']:
      archive=ROOT/'.deps'/('tiger-'+dep['name']+'.tar')
      if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest()!=dep['sha256']:
        raise SystemExit('Fetch/verify dependencies first: '+dep['name'])
    fingerprint = hashlib.sha256((ROOT/'tiger-dependencies.lock.json').read_bytes() +
        pathlib.Path(__file__).read_bytes() + (ROOT/'scripts/verified_sources.py').read_bytes() +
        PREFIX.encode() + FLAGS.encode() + os.environ.get('CODET_TOOLCHAIN_ID', '').encode() +
        file_hash(CC).encode()).hexdigest()
    final = STAGE
    if (final/'manifest.json').is_file():
        prior = json.loads((final/'manifest.json').read_text())
        if prior.get('source_hash') == fingerprint and prior.get('outputs') == tree_hashes(final, exclude_manifest=True):
            print('Verified sysroot cache: ' + str(final), flush=True)
            return
    STAGE = final.with_name('.tiger-sysroot-staging')
    if STAGE.exists(): remove_tree(STAGE)
    STAGE.mkdir()
    installed=STAGE/PREFIX.lstrip('/');installed.mkdir(parents=True,exist_ok=True)
    include=installed/'include';lib=installed/'lib'
    environment=os.environ.copy();environment.update(CC=CC,CXX=CC.replace('gcc','g++'),AR=CC.replace('gcc','ar'),RANLIB=CC.replace('gcc','ranlib'),NM=CC.replace('gcc','nm'),STRIP=CC.replace('gcc','strip'),CFLAGS=FLAGS,CXXFLAGS=FLAGS,CPPFLAGS=f'-I{include}/LegacySupport -I{include}',LDFLAGS=f'{FLAGS} -L{lib}',PKG_CONFIG_LIBDIR=str(lib/'pkgconfig'),ZERO_AR_DATE='1')
    source_hash=fingerprint
    source_trees={}
    for dep in lock['dependencies']:
      name=dep['name'];source=BUILD/name
      extract_verified(ROOT/'.deps'/('tiger-'+name+'.tar'), dep['sha256'], source)
      for patch in lock['patches']:
        if patch['name']!=name:continue
        p=ROOT/'.deps/tiger-patches'/name/patch['path']
        if file_hash(p)!=patch['sha256']:raise SystemExit('Patch checksum mismatch')
        if patch.get('destination'):
          target=source/patch['destination'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target);continue
        data=p.read_text();strip=1 if '--- a/' in data else 0
        run(['patch','--batch','--forward',f'-p{strip}','-i',p],source)
      source_trees[name]=hashlib.sha256(json.dumps(tree_hashes(source),sort_keys=True).encode()).hexdigest()
      e=environment.copy()
      if name=='legacy-support':
        run(['make','-j'+JOBS,'lib/libMacportsLegacySupport.a',f'CC={CC}',f'ARX={e["AR"]}',f'CFLAGS={FLAGS}','ARCHFLAGS='],source,e)
        run(['make','install-headers','install-slib',f'PREFIX={PREFIX}',f'DESTDIR={STAGE}',f'CC={CC}',f'ARX={e["AR"]}','ARCHFLAGS='],source,e)
      elif name=='zlib':
        e['CHOST']='powerpc-apple-darwin8'
        run(['./configure',f'--prefix={PREFIX}','--shared'],source,e)
        run(['make','-j'+JOBS],source,e);run(['make','install',f'DESTDIR={STAGE}'],source,e)
      elif name=='openssl':
        e['CFLAGS']=FLAGS+' -I'+str(include/'LegacySupport')
        run(['perl','Configure','darwin-ppc-cc','no-async','no-tests','no-asm','shared',f'--prefix={PREFIX}/libexec/openssl3',f'--openssldir={PREFIX}/libexec/openssl3/etc/openssl','-Wl,-headerpad_max_install_names'],source,e)
        atomic=subprocess.check_output([CC,'-print-file-name=libatomic.a'],text=True).strip()
        run(['make','-j'+JOBS,'EX_LIBS='+atomic],source,e);run(['make','install_sw','EX_LIBS='+atomic,f'DESTDIR={STAGE}'],source,e)
      elif name=='sqlite':
        (lib).mkdir(exist_ok=True);include.mkdir(exist_ok=True)
        run([CC,*FLAGS.split(),'-DSQLITE_THREADSAFE=1','-DSQLITE_WITHOUT_ZONEMALLOC','-DSQLITE_DISABLE_INTRINSIC','-dynamiclib','sqlite3.c','-o',lib/'libsqlite3.0.dylib',f'-Wl,-install_name,{PREFIX}/lib/libsqlite3.0.dylib','-Wl,-compatibility_version,9.0.0','-Wl,-current_version,9.54.0','-lpthread'],source,e)
        (lib/'libsqlite3.dylib').unlink(missing_ok=True);(lib/'libsqlite3.dylib').symlink_to('libsqlite3.0.dylib');shutil.copy2(source/'sqlite3.h',include/'sqlite3.h')
      else:
        args=['./configure','--host=powerpc-apple-darwin8','--build=x86_64-pc-linux-gnu',f'--prefix={PREFIX}','--enable-shared','--disable-static']
        if name=='curl':
          e['CPPFLAGS']+=f' -I{installed}/libexec/openssl3/include';e['LDFLAGS']+=f' -L{installed}/libexec/openssl3/lib';e['LIBS']='-lMacportsLegacySupport'
          args+=['--with-zlib='+str(installed),'--with-openssl='+str(installed/'libexec/openssl3'),'--with-ca-bundle='+PREFIX+'/share/curl/curl-ca-bundle.crt','--disable-ldap','--disable-ldaps','--without-libpsl','--without-brotli','--without-zstd','--without-libidn2','--without-nghttp2','--without-nghttp3','--without-ngtcp2','--without-libssh2','--without-gssapi','--disable-ares','--disable-rtsp','--disable-dict','--disable-telnet','--disable-tftp','--disable-smb','--disable-pop3','--disable-imap','--disable-smtp']
        if name=='ncurses':args+=['--with-shared','--without-debug','--without-ada','--without-cxx','--without-cxx-binding','--enable-widec','--disable-lib-suffixes','--enable-overwrite','--without-progs','--without-tests','--disable-stripping','--disable-mixed-case']
        if name=='libedit':args+=['--disable-examples'];e['LIBS']='-lncurses -lMacportsLegacySupport'
        run(args,source,e);run(['make','-j'+JOBS],source,e);run(['make','install',f'DESTDIR={STAGE}'],source,e)
    manifest=dict(lock,toolchain=CC,toolchain_image=os.environ.get("CODET_TOOLCHAIN_ID", ""),flags=FLAGS,prefix=PREFIX,source_hash=source_hash,source_trees=source_trees,outputs=tree_hashes(STAGE, exclude_manifest=True))
    (STAGE/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    previous = final.with_name('.tiger-sysroot-previous')
    if previous.exists(): remove_tree(previous)
    if final.exists(): final.rename(previous)
    try: STAGE.rename(final)
    except BaseException:
        if previous.exists(): previous.rename(final)
        raise
    if previous.exists(): remove_tree(previous)
    print('Cross sysroot ready: '+str(final),flush=True)

if __name__=='__main__':main()
