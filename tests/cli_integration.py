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
        messages=body['messages']
        prompt=' '.join(m.get('content','') or '' for m in messages if m['role']=='user')
        if 'fixture-error' in prompt:
            data=b'{"error":{"message":"do-not-leak-fake-credential"}}'
            self.send_response(401);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        if 'fixture-wait' in prompt:time.sleep(2)
        completed=[m for m in messages if m['role']=='tool']
        if 'fixture-edit' in prompt and len(completed)<2:
            name='Read' if not completed else 'Edit'
            args={'file_path':'sample.txt'} if not completed else {'file_path':'sample.txt','old_string':'before','new_string':'after'}
            delta={'tool_calls':[{'index':0,'id':'call-'+str(len(completed)),'function':{'name':name,'arguments':json.dumps(args)}}]}
            reason='tool_calls'
        elif 'fixture-schema' in prompt and not completed:
            delta={'tool_calls':[{'index':0,'id':'structured','function':{'name':'StructuredOutput','arguments':'{"answer":42}'}}]};reason='tool_calls'
        elif 'Explicit memory consolidation:' in prompt and len(completed)<2:
            args={'action':'read','name':'facts.md'} if not completed else {'action':'write','name':'facts.md','content':'Consolidated facts, from the explicit consolidation fixture.\n'}
            delta={'tool_calls':[{'index':0,'id':'memory-'+str(len(completed)),'function':{'name':'Memory','arguments':json.dumps(args)}}]};reason='tool_calls'
        elif 'fixture-recall' in prompt:
            delta={'content':'memory recovered' if any('Consolidated facts' in m.get('content','') for m in messages if m['role']=='system') else 'memory missing'};reason='stop'
        elif 'fixture-question' in prompt and not completed:
            delta={'tool_calls':[{'index':0,'id':'question','function':{'name':'AskUserQuestion','arguments':json.dumps({'question':'Which library is available?','options':['Catch2','libcurl']})}}]};reason='tool_calls'
        elif 'fixture-question' in prompt:
            answer=json.loads(completed[-1]['content'])['content']
            delta={'content':'question answered' if 'libcurl' in answer else 'answer missing'};reason='stop'
        else:delta={'content':'fixture done'};reason='stop'
        frame={'choices':[{'delta':delta,'finish_reason':reason}],'usage':{'prompt_tokens':10,'completion_tokens':2}}
        data=('data: '+json.dumps(frame)+'\n\ndata: [DONE]\n\n').encode()
        self.send_response(200);self.send_header('Content-Type','text/event-stream');self.send_header('Content-Length',str(len(data)));self.end_headers()
        with contextlib.suppress(BrokenPipeError,ConnectionResetError):
            for at in range(0,len(data),7):self.wfile.write(data[at:at+7]);self.wfile.flush()

class CliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
        threading.Thread(target=cls.server.serve_forever,daemon=True).start()
    @classmethod
    def tearDownClass(cls):cls.server.shutdown();cls.server.server_close()
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='codet-cli-');self.root=pathlib.Path(self.tmp.name);self.home=self.root/'home';self.home.mkdir();self.workspace=self.root/'workspace';self.workspace.mkdir()
        (self.home/'config.toml').write_text(f"provider='openai'\nmodel='fixture'\n[providers.openai]\napi_key='fake-key'\nbase_url='http://127.0.0.1:{self.server.server_port}/v1'\n")
        self.env=dict(os.environ,CODET_HOME=str(self.home));self.env.pop('SEARXNG_URL',None)
    def tearDown(self):self.tmp.cleanup()
    def run_cli(self,*args,input=''):
        return subprocess.run([BIN,'-C',str(self.workspace),*args],input=input,text=True,capture_output=True,env=self.env,timeout=15)
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
        p=subprocess.Popen([BIN,'-C',str(self.workspace),'exec','--json','fixture-wait'],stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=self.env)
        time.sleep(.3);p.send_signal(signal.SIGINT);out,err=p.communicate(timeout=8)
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
            p=subprocess.Popen([BIN,'-C',str(self.workspace),'--ui',mode,'fixture-question'],stdin=slave,stdout=slave,stderr=slave,env=dict(self.env,TERM='xterm'))
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

if __name__=='__main__':unittest.main()
