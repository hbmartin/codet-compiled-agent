#!/usr/bin/env python3
"""Real CLI + curl + SQLite; only a loopback fixture server, never a live provider."""
import contextlib,http.server,json,os,pathlib,signal,subprocess,sys,tempfile,threading,time,unittest

BIN=str(pathlib.Path(sys.argv.pop(1)).resolve())
FIXTURES=pathlib.Path(__file__).resolve().parent/'fixtures'

def normalized_events(events,workspace):
    """Only nondeterministic IDs and the temporary workspace are normalized."""
    events=json.loads(json.dumps(events))
    ids={}
    for event in events:
        if 'thread_id' in event:event['thread_id']='<thread>'
        item=event.get('item',{})
        if item.get('type')=='agent_message':
            ids.setdefault(item['id'],'message-'+str(len(ids)))
            item['id']=ids[item['id']]
        metadata=item.get('metadata',{})
        if 'paths' in metadata:
            metadata['paths']=[str(p).replace(str(workspace.resolve()),'<workspace>') for p in metadata['paths']]
    return events

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def handle(self):
        with contextlib.suppress(ConnectionResetError,BrokenPipeError):super().handle()
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.server.requests.append({'path':self.path,'body':body})
        if self.path == '/api/chat':
            data=(json.dumps({'message':{'role':'assistant','content':'fixture done'},'done':True})+'\n').encode()
            self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        messages=body['messages']
        prompt=next((m.get('content','') or '' for m in reversed(messages) if m['role']=='user'),'')
        if 'fixture-progress-failure' in prompt:
            frames='data: '+json.dumps({'choices':[{'delta':{'content':'partial-progress'},'finish_reason':None}]})+'\n\ndata: {broken\n\n'
            data=frames.encode();self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        if 'fixture-error' in prompt:
            data=b'{"error":{"message":"do-not-leak-fake-credential"}}'
            self.send_response(401);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        if 'fixture-wait' in prompt:time.sleep(2)
        last_user=max((i for i,m in enumerate(messages) if m['role']=='user'),default=0)
        completed=[m for m in messages[last_user:] if m['role']=='tool']
        if 'fixture-capture' in prompt:
            if 'final-error' in prompt and len(completed)>=2:
                data=b'{"error":{"message":"fixture failure after capture"}}'
                self.send_response(401);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
            if not completed:
                name='exec_command'
                command='cat output.txt'
                if 'wait' in prompt:command+='; printf ready > ready; while test ! -f release; do sleep .02; done'
                if 'background' in prompt:command='printf ready > ready; cat output.txt; while test ! -f release; do sleep .02; done'
                args={'cmd':command,'login':False,'yield_time_ms':250 if 'background' in prompt else 10000}
            elif 'background' in prompt:
                name=None
            elif len(completed)==1:
                path=json.loads(completed[0]['content'])['spill_path']
                import shlex
                name='exec_command';args={'cmd':'head -n 1 '+shlex.quote(path),'login':False}
            else:name=None
            if name:
                delta={'tool_calls':[{'index':0,'id':'capture-'+str(last_user)+'-'+str(len(completed)),'function':{'name':name,'arguments':json.dumps(args)}}]};reason='tool_calls'
            else:delta={'content':'fixture done'};reason='stop'
        elif any('fixture-async' in (m.get('content','') or '') for m in messages if m['role']=='user'):
            all_tools=[m for m in messages if m['role']=='tool']
            if 'libcurl' in prompt:
                delta={'content':'async reply received'};reason='stop'
            elif not all_tools:
                delta={'tool_calls':[{'index':0,'id':'async-ask','function':{'name':'request_user_input_async','arguments':json.dumps({'questions':[{'title':'Async library choice','options':['Catch2','libcurl']}]})}}]};reason='tool_calls'
            elif len(all_tools)==1:
                delta={'tool_calls':[{'index':0,'id':'async-sleep','function':{'name':'clock__sleep','arguments':json.dumps({'duration_ms':10000})}}]};reason='tool_calls'
            else:
                delta={'content':'async wait elapsed'};reason='stop'
        elif 'fixture-long-batch' in prompt and not completed:
            commands=['printf begun > begun; while test ! -f release; do sleep .02; done','printf completed > completed']
            delta={'tool_calls':[{'index':i,'id':'batch-'+str(i),'function':{'name':'exec_command','arguments':json.dumps({'cmd':command,'login':False})}} for i,command in enumerate(commands)]};reason='tool_calls'
        elif 'fixture-command' in messages[last_user].get('content','') and not completed:
            delta={'tool_calls':[{'index':0,'id':'shell-'+str(last_user),'function':{'name':'exec_command','arguments':json.dumps({'cmd':'printf approved','login':False})}}]};reason='tool_calls'
        elif 'fixture-edit' in prompt and not completed:
            args={'patch':'*** Begin Patch\n*** Update File: sample.txt\n@@\n-before\n+after\n*** End Patch\n'}
            delta={'tool_calls':[{'index':0,'id':'call-0','function':{'name':'apply_patch','arguments':json.dumps(args)}}]};reason='tool_calls'
        elif 'fixture-schema' in prompt and not completed:
            delta={'tool_calls':[{'index':0,'id':'structured','function':{'name':'StructuredOutput','arguments':'{"answer":42}'}}]};reason='tool_calls'
        elif 'Explicit memory consolidation:' in prompt and not completed:
            import re
            memory=re.search(r'Durable Markdown memory lives in ([^;]+);',messages[0]['content']).group(1)
            patch='*** Begin Patch\n*** Update File: '+str(pathlib.Path(memory)/'facts.md')+'\n@@\n-Old facts to consolidate.\n+Consolidated facts, from the explicit consolidation fixture.\n*** End Patch\n'
            delta={'tool_calls':[{'index':0,'id':'memory-0','function':{'name':'apply_patch','arguments':json.dumps({'patch':patch})}}]};reason='tool_calls'
        elif 'fixture-recall' in prompt:
            delta={'content':'memory recovered' if any('Consolidated facts' in m.get('content','') for m in messages if m['role']=='system') else 'memory missing'};reason='stop'
        elif 'fixture-question' in prompt and not completed:
            delta={'tool_calls':[{'index':0,'id':'question','function':{'name':'request_user_input','arguments':json.dumps({'questions':[{'id':'library','header':'Library','question':'Which library is available?','options':[{'label':'Catch2','description':'test library'},{'label':'libcurl','description':'HTTP library'}]}]})}}]};reason='tool_calls'
        elif 'fixture-question' in prompt:
            answer=completed[-1]['content']
            delta={'content':'question answered' if 'libcurl' in answer else 'answer missing'};reason='stop'
        else:delta={'content':'fixture done'};reason='stop'
        frame={'choices':[{'delta':delta,'finish_reason':reason}],'usage':{'prompt_tokens':10,'completion_tokens':2}}
        data=('data: '+json.dumps(frame)+'\n\ndata: [DONE]\n\n').encode()
        if 'fixture-active-stream' in prompt or 'fixture-idle-stream' in prompt:
            partial=('data: '+json.dumps({'choices':[{'delta':{'content':'.'},'finish_reason':None}]})+'\n\n').encode()
            frames=[partial]*5+[data]
            self.send_response(200);self.send_header('Content-Type','text/event-stream');self.send_header('Content-Length',str(sum(map(len,frames))));self.end_headers()
            with contextlib.suppress(BrokenPipeError,ConnectionResetError):
                for chunk in frames:
                    self.wfile.write(chunk);self.wfile.flush()
                    time.sleep(2 if 'fixture-idle-stream' in prompt else .3)
            return
        self.send_response(200);self.send_header('Content-Type','text/event-stream');self.send_header('Content-Length',str(len(data)));self.end_headers()
        with contextlib.suppress(BrokenPipeError,ConnectionResetError):
            for at in range(0,len(data),7):self.wfile.write(data[at:at+7]);self.wfile.flush()

class CliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
        cls.server.requests=[]
        threading.Thread(target=cls.server.serve_forever,daemon=True).start()
    @classmethod
    def tearDownClass(cls):cls.server.shutdown();cls.server.server_close()
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='codet-cli-');self.root=pathlib.Path(self.tmp.name);self.home=self.root/'home';self.home.mkdir();self.workspace=self.root/'workspace';self.workspace.mkdir()
        (self.home/'config.toml').write_text(f"provider='openai'\nmodel='fixture'\n[providers.openai]\napi_key='fake-key'\nbase_url='http://127.0.0.1:{self.server.server_port}/v1'\n")
        self.env=dict(os.environ,CODET_HOME=str(self.home));self.env.pop('SEARXNG_URL',None)
        self.server.requests=[]
    def tearDown(self):self.tmp.cleanup()
    def run_cli(self,*args,input=''):
        return subprocess.run([BIN,'-C',str(self.workspace),*args],input=input,text=True,capture_output=True,env=self.env,timeout=15)
    def test_prompt_subcommand_words_and_literal_flags(self):
        cases=[('exec','doctor'),('exec','memory'),('exec','login'),('exec','sessions'),
               ('exec','export'),('exec','fix','resume','handling'),('fix','doctor','handling'),
               ('exec','--','resume'),('exec','--','--help'),('exec','--','--version'),
               ('exec','--','--')]
        for args in cases:
            with self.subTest(args=args):
                r=self.run_cli(*args);self.assertEqual(r.returncode,0,r.stderr)
                self.assertEqual(r.stdout,'fixture done\n')
                words=list(args)
                if words[0]=='exec':words.pop(0)
                if words[0]=='--':words.pop(0)
                self.assertEqual(self.server.requests[-1]['body']['messages'][-1]['content'],' '.join(words))
        r=self.run_cli('exec','--model','--help','simple')
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(self.server.requests[-1]['body']['model'],'--help')

    def test_help_version_precede_errors_but_respect_values_and_literals(self):
        for flags in [('--bogus','--help'),('--help','--bogus'),('--output-format=bad','--help'),('--version','--bogus'),('--bogus','--version'),('--version','--help')]:
            r=self.run_cli(*flags)
            self.assertEqual(r.returncode,0,r.stderr)
            self.assertTrue(r.stdout.startswith('Codet') or r.stdout.startswith('codet'),r.stdout)
        self.assertEqual(self.run_cli('--bogus').returncode,2)

    def test_new_and_saved_selection_use_configured_pairs(self):
        base=f"[providers.openai]\napi_key='fake'\nbase_url='http://127.0.0.1:{self.server.server_port}/v1'\n[providers.ollama]\nbase_url='http://127.0.0.1:{self.server.server_port}'\nmodel='ollama-configured'\n"
        (self.home/'config.toml').write_text("provider='openai'\nmodel='configured-openai'\n"+base)
        r=self.run_cli('exec','--provider','ollama','--json','simple')
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(self.server.requests[-1]['body']['model'],'ollama-configured')
        session=json.loads(r.stdout.splitlines()[0])['thread_id']
        for action in ('resume','fork'):
            r=self.run_cli('exec',action,session,'--provider','openai','simple')
            self.assertEqual(r.returncode,0,r.stderr)
            self.assertEqual(self.server.requests[-1]['body']['model'],'configured-openai')
        (self.home/'config.toml').write_text("provider='openai'\nmodel_provider=42\nmodel='configured-openai'\n"+base)
        self.assertEqual(self.run_cli('doctor').returncode,0)

    def test_resume_selection_is_field_specific(self):
        base=f"[providers.openai]\napi_key='fake'\nbase_url='http://127.0.0.1:{self.server.server_port}/v1'\nmodel='openai-default'\n[providers.ollama]\nbase_url='http://127.0.0.1:{self.server.server_port}'\nmodel='ollama-default'\n"
        cases=[(['--max-turns','10'],'','openai','saved'),
               (['-c','max_turns=10'],'','openai','saved'),
               (['--profile','selected'],'max_turns=10\n','openai','saved'),
               (['--model','explicit'],'','openai','explicit'),
               (['-c',"model='explicit'"],'','openai','explicit'),
               (['--provider','ollama'],'','ollama','changed-global'),
               (['-c',"model_provider='ollama'"],'','ollama','changed-global'),
               (['--provider','openai'],'','openai','saved'),
               (['--profile','selected'],"provider='ollama'\n",'ollama','changed-global'),
               (['--profile','selected'],"model='profile-model'\n",'openai','profile-model'),
               (['--profile','selected','-m','cli-model'],"model='profile-model'\n",'openai','cli-model'),
               (['--provider','ollama','-m','explicit'],'','ollama','explicit')]
        for flags,profile,provider,model in cases:
            with self.subTest(flags=flags,profile=profile):
                (self.home/'config.toml').write_text("provider='openai'\nmodel='global'\n"+base)
                r=self.run_cli('exec','-m','saved','--json','simple');self.assertEqual(r.returncode,0,r.stderr)
                session=json.loads(r.stdout.splitlines()[0])['thread_id']
                (self.home/'config.toml').write_text("provider='ollama'\nmodel='changed-global'\n"+base)
                (self.home/'selected.config.toml').write_text(profile)
                r=self.run_cli('exec','resume',session,*flags,'continue');self.assertEqual(r.returncode,0,r.stderr)
                request=self.server.requests[-1]
                self.assertEqual(request['path'],'/api/chat' if provider=='ollama' else '/v1/chat/completions')
                self.assertEqual(request['body']['model'],model)
                import sqlite3
                with contextlib.closing(sqlite3.connect(self.home/'state.sqlite3')) as db:
                    selected=db.execute('SELECT provider,model FROM sessions WHERE id=?',(session,)).fetchone()
                self.assertEqual(selected,(provider,model))

    def configure_hooks(self,start='cat',end='cat'):
        marker=self.root/'hooks.jsonl'
        content=(self.home/'config.toml').read_text()
        content+='\n[hooks]\n'
        for event,command in [('SessionStart',start),('SessionEnd',end)]:
            content+=f'{event}=[{{command={json.dumps(command+" >> "+str(marker))}}}]\n'
        (self.home/'config.toml').write_text(content)
        return marker

    def test_end_hooks_cover_success_failure_and_empty_fork(self):
        marker=self.configure_hooks()
        for prompt,code in [('simple',0),('fixture-error',1)]:
            marker.unlink(missing_ok=True)
            r=self.run_cli('exec',prompt);self.assertEqual(r.returncode,code,r.stderr)
            events=[json.loads(x) for x in marker.read_text().splitlines()]
            self.assertEqual([x['hook_event_name'] for x in events],['SessionStart','SessionEnd'])
            self.assertEqual(events[0]['session_id'],events[1]['session_id'])
        marker.unlink()
        r=self.run_cli('exec','fork','--last');self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual([json.loads(x)['hook_event_name'] for x in marker.read_text().splitlines()],['SessionStart','SessionEnd'])

    def test_end_hook_failures_preserve_success_error_and_start_failure(self):
        self.configure_hooks(end='exit 1')
        for prompt,code in [('simple',0),('fixture-error',1)]:
            r=self.run_cli('exec','--output-format=json',prompt)
            self.assertEqual(r.returncode,code);self.assertIn('SessionEnd hook cleanup failed',r.stderr)
            self.assertEqual(json.loads(r.stdout)['status'],'success' if code==0 else 'error')
        (self.home/'config.toml').write_text((self.home/'config.toml').read_text().split('[hooks]')[0])
        marker=self.configure_hooks(start='exit 1')
        marker.unlink(missing_ok=True)
        r=self.run_cli('exec','simple');self.assertEqual(r.returncode,0)
        self.assertIn('SessionStart exited with code 1',r.stderr)
        self.assertEqual([json.loads(x)['hook_event_name'] for x in marker.read_text().splitlines()],['SessionEnd'])

    def test_end_hook_runs_after_interruption(self):
        marker=self.configure_hooks()
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'exec','--json','fixture-wait'],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=self.env)
        time.sleep(.3);p.send_signal(signal.SIGINT);out,err=p.communicate(timeout=8)
        self.assertEqual(p.returncode,130,err)
        self.assertEqual([json.loads(x)['hook_event_name'] for x in marker.read_text().splitlines()],['SessionStart','SessionEnd'])

    def test_compaction_requests_omit_empty_tools_through_real_adapter(self):
        r=self.run_cli('exec','--json','x'*20000);self.assertEqual(r.returncode,0,r.stderr)
        body=self.server.requests[-1]['body']
        fixed=len(body['messages'][0]['content'].encode())+len(json.dumps(body['tools'],separators=(',',':')).encode())
        config=self.home/'config.toml';config.write_text('context_tokens='+str((fixed+8000)//3)+'\n'+config.read_text())
        session=json.loads(r.stdout.splitlines()[0])['thread_id'];self.server.requests=[]
        r=self.run_cli('exec','resume',session,'continue');self.assertEqual(r.returncode,0,r.stderr)
        self.assertEqual(len(self.server.requests),2)
        self.assertNotIn('tools',self.server.requests[0]['body'])
        self.assertTrue(self.server.requests[1]['body']['tools'])

    def test_insufficient_fixed_context_fails_before_provider_request(self):
        config=self.home/'config.toml';config.write_text('context_tokens=512\n'+config.read_text())
        r=self.run_cli('exec','task')
        self.assertNotEqual(r.returncode,0)
        self.assertIn('Increase context_tokens',r.stderr)
        self.assertEqual(self.server.requests,[])

    def test_live_permission_modes_in_both_terminals(self):
        import pty,select
        for mode in ['line','curses']:
            master,slave=pty.openpty()
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_for(text):
                nonlocal received
                deadline=time.monotonic()+5
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.05)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
                received=b''
            try:
                wait_for(b'codet>')
                os.write(master,b'/permissions bypass\n');wait_for(b'Permissions: bypass')
                os.write(master,b'fixture-command\n');wait_for(b'fixture done')
                tool=[m for m in self.server.requests[-1]['body']['messages'] if m['role']=='tool'][-1]
                outcome=json.loads(tool['content'])
                self.assertEqual(outcome['exit_code'],0)
                self.assertEqual(outcome['output'],'approved')
                os.write(master,b'/permissions invalid\n');wait_for(b'Use /permissions')
                os.write(master,b'/permissions\n');wait_for(b'Permissions: bypass')
                os.write(master,b'/permissions plan\n');wait_for(b'Permissions: plan')
                os.write(master,b'fixture-command\n');wait_for(b'fixture done')
                self.assertIn('Permission denied for exec_command',json.dumps(self.server.requests[-1]['body']))
                os.write(master,b'/permissions default\n');wait_for(b'Permissions: default')
                os.write(master,b'/quit\n');deadline=time.monotonic()+5
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)

    def test_curses_progress_clears_after_success_failure_and_cancellation(self):
        import fcntl,pty,select,struct,termios
        config=self.home/'config.toml'
        config.write_text('stream_idle_timeout_seconds=5\n'+config.read_text())
        master,slave=pty.openpty()
        fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,120,0,0))
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui','curses'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
        received=b''
        def wait_for(text):
            nonlocal received
            deadline=time.monotonic()+7
            while text not in received and time.monotonic()<deadline:
                if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
            self.assertIn(text,received)
        try:
            wait_for(b'codet>')
            for index,(prompt,finish) in enumerate([('fixture-active-stream',b'fixture done'),('fixture-progress-failure',b'Error:'),('fixture-idle-stream',b'Error:')]):
                received=b'';os.write(master,prompt.encode()+b'\n')
                wait_for(b'Responding:')
                if index==2:p.send_signal(signal.SIGINT)
                wait_for(finish)
                # Streamed final text and older errors can precede turn completion.
                # Require an idle repaint, allowing bounded worker/terminal scheduling latency.
                deadline=time.monotonic()+7
                while time.monotonic()<deadline:
                    received=b''
                    fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',25+index,121+index,0,0))
                    p.send_signal(signal.SIGWINCH)
                    wait_for(b'Permissions: default')
                    if b'Responding:' not in received:break
                    time.sleep(.02)
                self.assertNotIn(b'Responding:',received)
            os.write(master,b'/quit\n');deadline=time.monotonic()+5
            while p.poll() is None and time.monotonic()<deadline:
                if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
            self.assertEqual(p.poll(),0,repr(received))
        finally:
            if p.poll() is None:p.kill();p.wait()
            os.close(master);os.close(slave)

    def test_cleanup_diagnostics_follow_curses_restoration(self):
        import pty,select,termios
        config=self.home/'config.toml'
        config.write_text(config.read_text()+'\n[hooks]\nSessionEnd=[{command="printf cleanup-detail >&2; exit 3"}]\n')
        master,slave=pty.openpty();original=termios.tcgetattr(slave)
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui','curses'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
        received=b''
        try:
            deadline=time.monotonic()+5
            while b'codet>' not in received and time.monotonic()<deadline:
                if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
            self.assertIn(b'codet>',received)
            os.write(master,b'/quit\n')
            while p.poll() is None or select.select([master],[],[],.1)[0]:
                if select.select([master],[],[],.05)[0]:received+=os.read(master,65536)
                if time.monotonic()>deadline+5:self.fail('terminal did not exit')
            self.assertEqual(p.wait(timeout=1),0)
            self.assertIn(b'cleanup-detail',received)
            self.assertIn(b'SessionEnd hook cleanup failed',received)
            self.assertGreater(received.index(b'cleanup-detail'),received.index(b'\x1b[?1049l'))
            restored=termios.tcgetattr(slave)
            # PENDIN is a kernel-maintained pending-input flag on Darwin.
            restored[3]&=~getattr(termios,'PENDIN',0);original[3]&=~getattr(termios,'PENDIN',0)
            self.assertEqual(restored,original)
        finally:
            if p.poll() is None:p.kill();p.wait()
            os.close(master);os.close(slave)

    def test_export_fork_and_resume_of_a_live_session(self):
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'exec','--json','--permission-mode','bypass','fixture-long-batch'],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=self.env)
        try:
            session=json.loads(p.stdout.readline())['thread_id']
            deadline=time.monotonic()+5
            while not (self.workspace/'begun').exists() and time.monotonic()<deadline:time.sleep(.01)
            self.assertTrue((self.workspace/'begun').exists())
            exported=self.run_cli('export',session);self.assertEqual(exported.returncode,0,exported.stderr)
            forked=self.run_cli('exec','fork',session);self.assertEqual(forked.returncode,0,forked.stderr)
            branch=json.loads(forked.stdout)['thread_id']
            self.assertNotEqual(branch,session)
            import sqlite3
            with contextlib.closing(sqlite3.connect(self.home/'state.sqlite3')) as db:
                messages=json.loads(db.execute('SELECT messages FROM sessions WHERE id=?',(branch,)).fetchone()[0])
            recovery=[json.loads(m['text'])['metadata']['recovery'] for m in messages if m['role']=='tool']
            self.assertEqual(recovery,['uncertain','not_started'])
            resumed=self.run_cli('exec','resume',session,'continue');self.assertEqual(resumed.returncode,1,resumed.stderr)
            self.assertIn('in use by another process',resumed.stderr)
            (self.workspace/'release').touch()
            out,err=p.communicate(timeout=10);self.assertEqual(p.returncode,0,err)
            self.assertTrue((self.workspace/'completed').exists())
        finally:
            (self.workspace/'release').touch()
            if p.poll() is None:
                p.send_signal(signal.SIGINT);p.communicate(timeout=10)

    def test_stream_idle_and_explicit_total_timeouts(self):
        config=self.home/'config.toml'
        base=config.read_text()
        config.write_text('stream_idle_timeout_seconds=1\n'+base)
        r=self.run_cli('exec','fixture-active-stream');self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('fixture done',r.stdout)
        config.write_text('stream_idle_timeout_seconds=1\nrequest_timeout_seconds=1\n'+base)
        self.server.requests=[]
        r=self.run_cli('exec','fixture-active-stream');self.assertEqual(r.returncode,1,r.stderr)
        self.assertEqual(len(self.server.requests),1)
        config.write_text('stream_idle_timeout_seconds=1\n'+base)
        self.server.requests=[]
        r=self.run_cli('exec','fixture-idle-stream');self.assertEqual(r.returncode,1,r.stderr)
        self.assertIn('idle timeout',r.stderr);self.assertEqual(len(self.server.requests),1)

    def test_start_and_prompt_plaintext_context_is_persisted(self):
        config=self.home/'config.toml'
        config.write_text(config.read_text()+'\n[hooks]\nSessionStart=[{command="printf startup-context"}]\nUserPromptSubmit=[{command="printf prompt-context"}]\n')
        r=self.run_cli('exec','simple');self.assertEqual(r.returncode,0,r.stderr)
        messages=self.server.requests[-1]['body']['messages']
        self.assertTrue(any('startup-context' in m.get('content','') for m in messages))
        self.assertTrue(any('prompt-context' in m.get('content','') for m in messages))

    def test_session_start_sources_append_identical_context_on_resume_and_fork(self):
        config=self.home/'config.toml';marker=self.root/'starts.jsonl'
        record='cat >> '+str(marker)+'; printf identical-context'
        config.write_text(config.read_text()+'\n[hooks]\nSessionStart=[{matcher="",command='+json.dumps(record)+'},{matcher="resume",command="printf resume-context"},{matcher="fork",command="printf fork-context"}]\n')
        first=self.run_cli('exec','--output-format','json','simple')
        self.assertEqual(first.returncode,0,first.stderr);sid=json.loads(first.stdout)['session_id']
        for command in ['resume','fork']:
            result=self.run_cli('exec',command,sid,'simple')
            self.assertEqual(result.returncode,0,result.stderr)
            messages=self.server.requests[-1]['body']['messages']
            contexts=[m['content'] for m in messages if m['role']=='system' and m.get('content','').startswith('SessionStart hook context:')]
            self.assertEqual(len(contexts),2 if command=='resume' else 3)
            self.assertTrue(all('identical-context' in m for m in contexts))
            self.assertIn(command+'-context',contexts[-1])
        self.assertEqual([json.loads(x)['source'] for x in marker.read_text().splitlines()],['startup','resume','fork'])

    def test_diagnostics_render_in_terminal_and_failed_expansion_recovers(self):
        import fcntl,pty,select,struct,termios
        config=self.home/'config.toml';base=config.read_text()
        for mode in ['line','curses']:
            config.write_text(base+'\n[hooks]\nUserPromptSubmit=[{command="printf prompt-context"}]\nPreToolUse=[{matcher="Bash",command="printf hook-visible >&2"}]\n')
            master,slave=pty.openpty()
            fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',40,140,0,0))
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'--permission-mode','bypass'],stdin=slave,stdout=slave,stderr=subprocess.PIPE,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_for(text):
                nonlocal received
                deadline=time.monotonic()+7
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
            try:
                wait_for(b'codet>');received=b''
                os.write(master,b'/unknown\n');wait_for(b'Error:')
                self.assertNotIn(b'recovery',received.lower());self.assertNotIn(b'concurrent',received.lower())
                received=b'';os.write(master,b'fixture-command\n');wait_for(b'fixture done')
                if mode=='curses':
                    time.sleep(.05);received=b''
                    fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',41,141,0,0));p.send_signal(signal.SIGWINCH)
                    wait_for(b'hook-visible')
                    self.assertFalse(select.select([p.stderr],[],[],0)[0], 'diagnostic escaped during curses operation')
                messages=self.server.requests[-1]['body']['messages']
                self.assertEqual(sum(m.get('content','').startswith('UserPromptSubmit hook context:') for m in messages),1)
                os.write(master,b'/quit\n')
                deadline=time.monotonic()+5
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
                stderr=p.stderr.read()
                if mode=='curses':self.assertEqual(stderr.count(b'hook-visible'),1)
                else:self.assertIn(b'hook-visible',stderr)
            finally:
                if p.poll() is None:p.kill();p.wait()
                p.stderr.close();os.close(master);os.close(slave)

    def test_curses_replay_and_cleanup_are_ordered_after_success_error_and_interrupt(self):
        import pty,select
        config=self.home/'config.toml';base=config.read_text()
        config.write_text(base+'\n[hooks]\nSessionStart=[{command="printf live-before >&2"}]\nSessionEnd=[{command="printf end-warning >&2"},{command="printf end-error >&2; exit 3"}]\n')
        for prompt,outcome in [('simple',b'fixture done'),('fixture-error',b'Error:'),('fixture-wait',b'Interrupted')]:
            with self.subTest(prompt=prompt):
                master,slave=pty.openpty();self.server.requests=[]
                p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui','curses'],stdin=slave,stdout=slave,stderr=subprocess.PIPE,env=dict(self.env,TERM='xterm'))
                received=b''
                def wait_for(text):
                    nonlocal received
                    deadline=time.monotonic()+7
                    while text not in received and time.monotonic()<deadline:
                        if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                    self.assertIn(text,received)
                try:
                    wait_for(b'codet>');self.assertFalse(select.select([p.stderr],[],[],0)[0])
                    received=b'';os.write(master,prompt.encode()+b'\n')
                    if prompt=='fixture-wait':
                        deadline=time.monotonic()+5
                        while not self.server.requests and time.monotonic()<deadline:time.sleep(.01)
                        self.assertTrue(self.server.requests);p.send_signal(signal.SIGINT)
                    wait_for(outcome);self.assertFalse(select.select([p.stderr],[],[],0)[0])
                    os.write(master,b'/quit\n');deadline=time.monotonic()+7
                    while p.poll() is None and time.monotonic()<deadline:
                        if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                    self.assertEqual(p.poll(),0,repr(received));stderr=p.stderr.read()
                    for marker in [b'live-before',b'end-warning',b'end-error']:self.assertEqual(stderr.count(marker),1)
                    self.assertLess(stderr.index(b'live-before'),stderr.index(b'end-warning'));self.assertLess(stderr.index(b'end-warning'),stderr.index(b'end-error'))
                    self.assertIn(b'SessionEnd hook warning:',stderr);self.assertIn(b'SessionEnd hook cleanup failed:',stderr)
                finally:
                    if p.poll() is None:p.kill();p.wait()
                    p.stderr.close();os.close(master);os.close(slave)

    def test_curses_replay_and_cleanup_have_independent_budgets_and_omission_notices(self):
        import pty,select
        config=self.home/'config.toml'
        start=[{'command':"printf 'live-"+str(i)+"-"+('x'*150)+"' >&2"} for i in range(12)]
        end=[{'command':"printf 'end-"+str(i)+"-"+('y'*150)+"' >&2"} for i in range(12)]
        def toml_hooks(items):return '['+','.join('{command='+json.dumps(x['command'])+'}' for x in items)+']'
        config.write_text(config.read_text()+'\n[hooks]\nSessionStart='+toml_hooks(start)+'\nSessionEnd='+toml_hooks(end)+'\n')
        master,slave=pty.openpty()
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui','curses','-c','tool_output_limit=512'],stdin=slave,stdout=slave,stderr=subprocess.PIPE,env=dict(self.env,TERM='xterm'))
        received=b''
        try:
            deadline=time.monotonic()+7
            while b'live-' not in received and time.monotonic()<deadline:
                if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
            self.assertIn(b'live-',received);self.assertFalse(select.select([p.stderr],[],[],0)[0])
            os.write(master,b'/quit\n')
            while p.poll() is None and time.monotonic()<deadline+7:
                if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
            self.assertEqual(p.poll(),0,repr(received));stderr=p.stderr.read()
            self.assertIn(b'live-0',stderr);self.assertIn(b'end-0',stderr)
            self.assertEqual(stderr.count(b'[Additional diagnostics omitted or truncated]'),2)
            self.assertLessEqual(len(stderr),1024)
        finally:
            if p.poll() is None:p.kill();p.wait()
            p.stderr.close();os.close(master);os.close(slave)

    def test_ephemeral_capture_is_pageable_and_removed_after_success_or_error(self):
        (self.workspace/'output.txt').write_text('雪-output\n'*20000)
        temporary=self.root/'temporary';temporary.mkdir();self.env['TMPDIR']=str(temporary)
        for prompt,code in [('fixture-capture',0),('fixture-capture-final-error',1),('fixture-capture-background',0)]:
            with self.subTest(prompt=prompt):
                self.server.requests=[]
                r=self.run_cli('exec','--ephemeral','--permission-mode','bypass',prompt)
                self.assertEqual(r.returncode,code,r.stderr)
                outcomes=[json.loads(m['content']) for m in self.server.requests[-1]['body']['messages'] if m['role']=='tool']
                capture=next(x for x in outcomes if 'spill_path' in x)
                self.assertEqual(capture.get('exit_code',0),0);self.assertTrue(str(capture['spill_path']).startswith(str(temporary)))
                self.assertFalse(pathlib.Path(capture['spill_path']).exists())
                self.assertFalse(list(temporary.glob('codet-ephemeral-*')))
                self.assertFalse((self.home/'tool-output').exists());self.assertFalse((self.home/'state.sqlite3').exists())
                if 'background' not in prompt:self.assertIn('雪-output',outcomes[-1]['output'])
                self.assertTrue((self.home/'config.toml').exists())

    def test_ephemeral_cancellation_closes_captures_before_removing_directory(self):
        (self.workspace/'output.txt').write_text('capture-data\n'*20000)
        temporary=self.root/'temporary';temporary.mkdir();self.env['TMPDIR']=str(temporary)
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'exec','--ephemeral','--permission-mode','bypass','fixture-capture-wait'],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=self.env)
        try:
            deadline=time.monotonic()+7
            while not (self.workspace/'ready').exists() and time.monotonic()<deadline:time.sleep(.01)
            self.assertTrue((self.workspace/'ready').exists())
            self.assertTrue(list(temporary.glob('codet-ephemeral-*/tool-output/capture-*.txt')))
            p.send_signal(signal.SIGINT);stdout,stderr=p.communicate(timeout=7)
            self.assertEqual(p.returncode,130,stderr)
            self.assertFalse(list(temporary.glob('codet-ephemeral-*')))
            self.assertFalse((self.home/'tool-output').exists())
        finally:
            if p.poll() is None:p.kill();p.wait()
            p.stdout.close();p.stderr.close()

    def test_new_session_lifecycle_and_invalid_hook_bytes_in_both_terminals(self):
        import pty,select
        marker=self.root/'lifecycle.jsonl'
        config=self.home/'config.toml';base=config.read_text()
        for mode in ['line','curses']:
            marker.unlink(missing_ok=True)
            record='cat >> '+str(marker)
            bad="printf '\\377' >&2; exit 2"
            config.write_text(base+'\n[hooks]\nSessionStart=[{command='+json.dumps(record)+'}]\nSessionEnd=[{command='+json.dumps(record)+'}]\nPreToolUse=[{matcher="Bash",command='+json.dumps(bad)+'}]\n')
            master,slave=pty.openpty()
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'--permission-mode','bypass'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_for(text):
                nonlocal received
                deadline=time.monotonic()+6
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.05)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
            try:
                wait_for(b'codet>');received=b''
                os.write(master,b'/new\n');wait_for(b'Started ')
                if mode=='line':wait_for(b'codet>')
                received=b'';os.write(master,b'fixture-command\n');wait_for(b'fixture done')
                tool=[m for m in self.server.requests[-1]['body']['messages'] if m['role']=='tool'][-1]
                outcome=tool['content'];self.assertIn('Tool error:',outcome);self.assertIn('\ufffd',outcome)
                received=b'';os.write(master,b'simple\n');wait_for(b'fixture done')
                os.write(master,b'/quit\n');deadline=time.monotonic()+6
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
                events=[json.loads(x) for x in marker.read_text().splitlines()]
                self.assertEqual([x['hook_event_name'] for x in events],['SessionStart','SessionEnd'])
                self.assertEqual(events[0]['session_id'],events[1]['session_id'])
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)

    def test_jsonl_real_tool_roundtrip_and_state(self):
        (self.workspace/'sample.txt').write_text('before\n')
        r=self.run_cli('exec','--json','--permission-mode','accept-edits','fixture-edit')
        self.assertEqual(r.returncode,0,r.stderr);events=[json.loads(x) for x in r.stdout.splitlines()]
        self.assertEqual(events[0]['type'],'thread.started');self.assertEqual(events[-1]['type'],'turn.completed');self.assertEqual((self.workspace/'sample.txt').read_text(),'after\n')
        self.assertTrue(any(x.get('item',{}).get('type')=='file_change' for x in events))
        expected=[json.loads(line) for line in (FIXTURES/'events-v1.jsonl').read_text().splitlines()]
        self.assertEqual(normalized_events(events,self.workspace),expected)
        session=events[0]['thread_id'];r=self.run_cli('exec','resume',session,'--json','continue')
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(json.loads(r.stdout.splitlines()[0])['thread_id'],session)
        r=self.run_cli('exec','fork',session);self.assertEqual(r.returncode,0,r.stderr);self.assertNotEqual(json.loads(r.stdout)['thread_id'],session)
    def test_text_final_only_and_output_file(self):
        output=self.root/'answer';r=self.run_cli('exec','-o',str(output),'simple')
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,'fixture done\n');self.assertEqual(output.read_text(),r.stdout)
    def test_stdin_and_single_json_result(self):
        r=self.run_cli('exec','--output-format=json',input='piped prompt');self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(json.loads(r.stdout)['result'],'fixture done')
    def test_schema_validates_final_answer(self):
        schema=self.root/'schema.json';schema.write_text(json.dumps({'type':'object','properties':{'answer':{'type':'integer'}},'required':['answer']}))
        r=self.run_cli('exec','--output-format=json','--output-schema',str(schema),'fixture-schema')
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(json.loads(r.stdout)['structured_output'],{'answer':42})
    def test_provider_errors_do_not_echo_body_or_credentials(self):
        r=self.run_cli('exec','--output-format=json','fixture-error');self.assertEqual(r.returncode,1);self.assertEqual(json.loads(r.stdout)['status'],'error');self.assertNotIn('do-not-leak',r.stdout+r.stderr)
    def test_ephemeral_session_not_saved(self):
        r=self.run_cli('exec','--ephemeral','simple');self.assertEqual(r.returncode,0,r.stderr)
        r=self.run_cli('sessions');self.assertEqual(json.loads(r.stdout),[])
    def test_explicit_memory_consolidation_is_readable_in_a_fresh_session(self):
        memory=self.home/'memory';memory.mkdir();(memory/'facts.md').write_text('Old facts to consolidate.\n')
        r=self.run_cli('memory','consolidate','--permission-mode','accept-edits')
        self.assertEqual(r.returncode,0,r.stderr)
        self.assertIn('Consolidated facts',(memory/'facts.md').read_text())
        r=self.run_cli('exec','fixture-recall')
        self.assertEqual(r.returncode,0,r.stderr);self.assertEqual(r.stdout,'memory recovered\n')
    def test_interrupt_has_exit_130_and_terminal_event(self):
        for sig in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signal=sig):
                p=subprocess.Popen([BIN,'-C',str(self.workspace),'exec','--json','fixture-wait'],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=self.env)
                time.sleep(.3);p.send_signal(sig);out,err=p.communicate(timeout=8)
                self.assertEqual(p.returncode,130,err);self.assertEqual(json.loads(out.splitlines()[-1])['type'],'turn.failed')
    def test_terminal_modes_restore_terminal(self):
        import pty,termios,select,fcntl,struct
        for mode in ['line','curses']:
            master,slave=pty.openpty();before=termios.tcgetattr(slave);fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,80,0,0))
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            try:
                received=b'';deadline=time.monotonic()+5
                while b'codet>' not in received and time.monotonic()<deadline:
                    ready,_,_=select.select([master],[],[],.2)
                    if ready:received+=os.read(master,65536)
                self.assertIn(b'codet>',received)
                if mode=='curses':p.send_signal(signal.SIGWINCH)
                os.write(master,b'/quit\n');deadline=time.monotonic()+5
                # Drain the PTY while waiting, so display output cannot block exit.
                while p.poll() is None and time.monotonic()<deadline:
                    ready,_,_=select.select([master],[],[],.02)
                    if ready:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
                after=termios.tcgetattr(slave)
                # Darwin may set PENDIN while restoring canonical mode; it is a
                # pending-input notification, not a changed editing mode.
                pending=getattr(termios,'PENDIN',0)
                self.assertEqual(before[3]&~pending,after[3]&~pending)
                self.assertEqual(before[:3],after[:3])
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)
    def test_interactive_interrupt_restores_terminal(self):
        import pty,termios,select
        for mode in ['line','curses']:
            master,slave=pty.openpty();before=termios.tcgetattr(slave)
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            try:
                received=b'';deadline=time.monotonic()+5
                while b'codet>' not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.1)[0]:received+=os.read(master,65536)
                self.assertIn(b'codet>',received);p.send_signal(signal.SIGINT);deadline=time.monotonic()+5
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),130,repr(received));after=termios.tcgetattr(slave);pending=getattr(termios,'PENDIN',0)
                self.assertEqual(before[3]&~pending,after[3]&~pending)
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)
    def test_both_terminal_modes_answer_questions(self):
        import pty,select
        for mode,answer in [('line','libcurl with OpenSSL'),('curses','2')]:
            master,slave=pty.openpty()
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'--permission-mode','plan','fixture-question'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_text(text):
                nonlocal received
                deadline=time.monotonic()+5
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.1)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
            try:
                wait_text(b'Which library is available?');os.write(master,(answer+'\n').encode())
                wait_text(b'question answered');os.write(master,b'/quit\n')
                deadline=time.monotonic()+5
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)

    def test_async_questions_and_sleep_wake_in_both_terminal_modes(self):
        import pty,select
        for mode,answer in [('line','libcurl'),('curses','libcurl')]:
            master,slave=pty.openpty()
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'fixture-async'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_text(text):
                nonlocal received
                deadline=time.monotonic()+5
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.1)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
            try:
                wait_text(b'Async library choice');os.write(master,(answer+'\n').encode())
                wait_text(b'async reply received');os.write(master,b'/quit\n')
                deadline=time.monotonic()+5
                while p.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(p.poll(),0,repr(received))
            finally:
                if p.poll() is None:p.kill();p.wait()
                os.close(master);os.close(slave)

    def test_late_steering_runs_before_a_deferred_new_session(self):
        import pty,select
        config=self.home/'config.toml';base=config.read_text()
        for mode in ['line','curses']:
            marker=self.workspace/('stop-'+mode);release=self.workspace/('release-'+mode)
            hook='if test ! -f '+str(marker)+'; then touch '+str(marker)+'; while test ! -f '+str(release)+'; do sleep .01; done; fi'
            config.write_text(base+'\n[hooks]\nStop=[{command='+json.dumps(hook)+'}]\n')
            master,slave=pty.openpty();self.server.requests=[]
            process=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'--permission-mode','bypass'],stdin=slave,stdout=slave,stderr=subprocess.PIPE,env=dict(self.env,TERM='xterm'))
            received=b''
            def wait_for(text):
                nonlocal received
                deadline=time.monotonic()+7
                while text not in received and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertIn(text,received)
            try:
                wait_for(b'codet>');os.write(master,b'first prompt\n')
                deadline=time.monotonic()+7
                while not marker.exists() and time.monotonic()<deadline:
                    if select.select([master],[],[],.01)[0]:received+=os.read(master,65536)
                self.assertTrue(marker.exists(),repr(received))
                os.write(master,b'late steering\n/new\n')
                wait_for(b'Starting a new session after pending messages are handled.')
                release.touch();wait_for(b'Started ')
                self.assertEqual(len(self.server.requests),2)
                users=[m['content'] for m in self.server.requests[1]['body']['messages'] if m['role']=='user']
                self.assertEqual(users,['first prompt','late steering'])
                received=b'';os.write(master,b'fresh session prompt\n');wait_for(b'fixture done')
                users=[m['content'] for m in self.server.requests[-1]['body']['messages'] if m['role']=='user']
                self.assertEqual(users,['fresh session prompt'])
                os.write(master,b'/quit\n');deadline=time.monotonic()+7
                while process.poll() is None and time.monotonic()<deadline:
                    if select.select([master],[],[],.02)[0]:received+=os.read(master,65536)
                self.assertEqual(process.poll(),0,repr(received))
            finally:
                release.touch()
                if process.poll() is None:process.kill();process.wait()
                process.stderr.close();os.close(master);os.close(slave)

if __name__=='__main__':unittest.main()
