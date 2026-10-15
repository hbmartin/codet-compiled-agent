#!/usr/bin/env python3
"""Run the host Clang analyzer using the real CMake compilation database."""
import concurrent.futures,hashlib,json,pathlib,shlex,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]

def main():
    database=ROOT/'build/host/compile_commands.json'
    if not database.exists():raise SystemExit('Run make host first')
    out=ROOT/'build/modern-analysis';out.mkdir(exist_ok=True)
    entries=[e for e in json.loads(database.read_text()) if '/src/modern/' in e['file']]
    def analyze(entry):
        command=entry.get('arguments') or shlex.split(entry['command']);args=[];i=0
        while i<len(command):
            if command[i]=='-o':i+=2;continue
            if command[i]=='-c':i+=1;continue
            args.append(command[i]);i+=1
        name=pathlib.Path(entry['file']).stem
        args+=['--analyze','-Xanalyzer','-analyzer-output=text','-o',str(out/(name+'.plist'))]
        result=subprocess.run(args,cwd=entry['directory'],text=True,capture_output=True)
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        return dict(source=entry['file'],sha256=hashlib.sha256(pathlib.Path(entry['file']).read_bytes()).hexdigest(),command=args,exit_code=result.returncode,diagnostics=result.stdout+result.stderr)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(analyze,entries))
    (out/'manifest.json').write_text(json.dumps(results,indent=2)+'\n')
    for result in results:
        print(pathlib.Path(result['source']).name+': '+str(result['exit_code']))
        if result['diagnostics']:print(result['diagnostics'])
    raise SystemExit(int(any(r['exit_code'] or 'warning:' in r['diagnostics'] for r in results)))

if __name__=='__main__':main()
