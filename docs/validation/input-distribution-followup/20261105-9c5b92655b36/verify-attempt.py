#!/usr/bin/env python3
"""Audit an explicitly incomplete campaign; exit 0 verifies integrity, never acceptance."""
import ast
import hashlib
import json
import re
import sys
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent/'verifier_tooling'))
from evidence_bundle import safe_member, valid_digest
from evidence_inputs import environment, file_specs, role_selected
from evidence_paths import verify_working_provenance
from validation_evidence import load_manifest


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def verify(folder):
    folder=folder.resolve()
    inventory=json.loads(safe_member(folder,'inventory.json').read_text())
    if not isinstance(inventory,dict):raise ValueError('Invalid inventory')
    for name,value in inventory.items():
        valid_digest(value);safe_member(folder,name)
    if any(p.is_symlink() for p in folder.rglob('*')):raise ValueError('Symlink in attempt bundle')
    actual={str(p.relative_to(folder)):digest(p) for p in folder.rglob('*') if p.is_file() and p!=folder/'inventory.json'}
    if actual!=inventory:raise ValueError('Attempt inventory mismatch')
    manifest=load_manifest(folder/'manifest.json')
    run=json.loads((folder/'capture/run.json').read_text())
    report=json.loads((folder/'report.json').read_text())
    if report.get('attempt_bundle_version')!=1 or report.get('eligible_current') is not False or report.get('complete') is not False:
        raise ValueError('Incomplete attempt must not claim acceptance')
    if run.get('schema_version')!=5 or report.get('schema_version')!=5:raise ValueError('Unexpected capture format')
    if run['manifest_sha256']!=digest(folder/'manifest.json'):raise ValueError('Manifest binding mismatch')
    if report['campaign_id']!=run['campaign_id'] or report['capture_run_id']!=run['id']:raise ValueError('Run identity mismatch')
    for key in ('id','campaign_id'):
        if re.fullmatch('[A-Za-z0-9_-]{1,128}',run[key]) is None:raise ValueError('Invalid run identity')
    index=json.loads((folder/'inputs.json').read_text());actual_index={}
    for entry in index:
        key=(entry['check_id'],entry['role'],entry['path'])
        if key in actual_index:raise ValueError('Duplicate archive declaration')
        valid_digest(entry['sha256'])
        if entry['storage'] is not None:
            if entry['storage']!='inputs/'+entry['sha256'] or digest(safe_member(folder,entry['storage']))!=entry['sha256']:
                raise ValueError('Archived input mismatch')
        actual_index[key]=(entry['sha256'],entry['storage'] is not None)
    expected={}
    def need(check,role,name,sha,retained=True):
        valid_digest(sha);expected[(check,role,name)]=(sha,retained)
    for role in ('production','validation'):
        for name,sha in report[role+'_hashes'].items():
            if not role_selected(name,role) or name.startswith('/') or '..' in name.split('/'):raise ValueError('Invalid fingerprint path')
            need('run','fingerprint:'+role,name,sha)
        for name,sha in report['validation_hashes'].items():
            if name.startswith('scripts/') and '/' not in name[len('scripts/'):]:
                copied=folder/'verifier_tooling'/Path(name).name
                if not copied.is_file() or digest(copied)!=sha:raise ValueError('Verifier tooling source mismatch')
    expected_files={'run.json'};derived=[];failed={};observations=[]
    if len(report['validation'])!=len(manifest['checks']):raise ValueError('Missing check coverage')
    for position,check in enumerate(manifest['checks']):
        name=check['id'];expected_files.update((name+'.json',name+'.log'))
        record=json.loads(safe_member(folder,'capture/'+name+'.json').read_text())
        if record!=report['validation'][position]:raise ValueError('Report differs from original capture')
        if (record['id']!=name or record['run_id']!=run['id'] or record['campaign_id']!=run['campaign_id'] or
            record['manifest_sha256']!=run['manifest_sha256'] or record['argv']!=check['argv'] or record.get('env')!=check.get('env',{})):
            raise ValueError('Check identity/command mismatch: '+name)
        for role,key in (('production','source'),('validation','validation')):
            if record[key+'_before']!=report[role+'_hashes'] or record[key+'_after']!=record[key+'_before']:
                raise ValueError('Changed check fingerprints: '+name)
        verify_working_provenance(manifest,run,record)
        if record['log']!=name+'.log' or record['log_sha256']!=digest(folder/'capture'/record['log']):raise ValueError('Log binding mismatch')
        completed=record['outcome'] in ('passed','failed') and type(record['exit']) is int
        acceptable=record['outcome']=='passed' and record['exit']==0
        if check.get('policy')=='observation' and completed:
            observations.append(name);acceptable=True
        if not acceptable:
            failed[name]=record['outcome']
            if name=='hash-experiment':
                if record['outcome']!='failed' or record['exit']!=1:raise ValueError('Unexpected measurement failure')
                log=(folder/'capture'/record['log']).read_text()
                prefix='ValueError: Concurrent validation/build/analyzer processes: '
                lines=[line[len(prefix):] for line in log.splitlines() if line.startswith(prefix)]
                processes=ast.literal_eval(lines[-1]) if lines else []
                if not processes or not any(p.get('argv',[])[-4:-2]==['-m','unittest'] or ['-m','unittest'] in [p.get('argv',[])[i:i+2] for i in range(len(p.get('argv',[])))] for p in processes):
                    raise ValueError('Unavailable benchmark reason not evidenced')
            elif name=='benchmarks':
                if record['outcome']!='blocked' or record['exit'] is not None or record['artifacts']:raise ValueError('Blocked benchmark was represented as executed')
            else:raise ValueError('Unexpected unmet required check: '+name)
        if completed:
            env=environment(manifest,check)
            context=dict(schema_version=5,campaign_id=run['campaign_id'],capture_run_id=run['id'],check_id=name,
                         root=manifest['root'],production_hashes=report['production_hashes'],validation_hashes=report['validation_hashes'],declared_artifacts=check.get('artifacts',[]))
            env['CODET_EVIDENCE_CONTEXT']=json.dumps(context)
            if env!=record['effective_environment']:raise ValueError('Effective environment mismatch')
            executable=record['executable'];valid_digest(executable['sha256'])
            if record['executed_argv']!=[executable['path']]+check['argv'][1:]:raise ValueError('Resolved executable mismatch')
            for key in ('input','build_inputs','external_inputs'):
                if record[key+'_before']!=record[key+'_after']:raise ValueError('Changed check inputs')
        for source,files in record.get('build_inputs_before',{}).items():
            for filename,sha in files.items():need(name,'source:'+source,filename,sha)
        specs=file_specs(check)
        for filename,sha in record.get('input_before',{}).items():
            if filename not in specs:raise ValueError('Undeclared input')
            need(name,'input',filename,sha,specs[filename].get('archive',True))
        for output in record['outputs']:need(name,'output',output['path'],output['sha256'],output['archive'])
        if acceptable:
            if {x['path'] for x in record['outputs']}!={x['path'] for x in check.get('outputs',[])}:raise ValueError('Incomplete successful output declarations')
            if len(record['artifacts'])!=len(check.get('artifacts',[])):raise ValueError('Incomplete successful artifacts')
        for saved,spec in zip(record['artifacts'],check.get('artifacts',[]),strict=True) if acceptable else []:
            target='artifacts/'+name+'/'+Path(saved['path']).name
            if saved['path']!=spec['path'] or digest(safe_member(folder,target))!=saved['sha256']:raise ValueError('Artifact digest mismatch')
            envelope=json.loads((folder/target).read_text());context={k:v for k,v in context.items() if k not in ('root','declared_artifacts')}
            if any(envelope.get(k)!=v for k,v in context.items()) or envelope['kind']!=spec['kind']:raise ValueError('Artifact producer identity mismatch')
            if envelope['input_hashes']!={p:record['input_before'][p] for p in spec.get('inputs',[])}:raise ValueError('Artifact input mismatch')
            if envelope['kind']=='gtest-isolation':
                from gtest_isolation import verify_payload
                if verify_payload(envelope['payload'],envelope['input_hashes'])['failures']:raise ValueError('Isolated run failed')
            derived.append(target)
    if failed!={'hash-experiment':'failed','benchmarks':'blocked'}:raise ValueError('Unavailable scope mismatch')
    if {str(p.relative_to(folder/'capture')) for p in (folder/'capture').rglob('*') if p.is_file()}!=expected_files:raise ValueError('Capture inventory coverage mismatch')
    if actual_index!=expected:raise ValueError('Archive declaration coverage mismatch')
    if {str(p.relative_to(folder)) for p in (folder/'artifacts').rglob('*') if p.is_file()}!=set(derived):raise ValueError('Artifact inventory coverage mismatch')
    return dict(integrity_verified=True,eligible_current=False,complete=False,checks=len(manifest['checks']),unmet_required=failed,
                observations=observations,measurement_samples=0,limitation='This audits an incomplete attempt, not full campaign acceptance.')

if __name__=='__main__':
    print(json.dumps(verify(Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent),indent=2))
