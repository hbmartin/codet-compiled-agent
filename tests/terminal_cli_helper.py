"""Fresh, single-threaded controlling-terminal supervisor for startup regressions."""
import fcntl
import json
import os
import pty
import select
import signal
import sys
import termios
import time


def foreground_child(slave,child):
    # The child waits on its launch gate: group setup cannot race another setpgid or exec.
    os.setpgid(child,child)
    os.tcsetpgrp(slave,child)


def active_job_control(binary):
    master,slave=pty.openpty()
    gate_read,gate_write=os.pipe();ready_read,ready_write=os.pipe()
    os.setsid();fcntl.ioctl(slave,termios.TIOCSCTTY,0)
    os.tcsetpgrp(slave,os.getpgrp())
    signal.signal(signal.SIGHUP,signal.SIG_IGN)
    signal.signal(signal.SIGTTOU,signal.SIG_IGN)
    before=termios.tcgetattr(slave)
    child=os.fork()
    if child==0:
        try:
            os.close(gate_write);os.close(ready_read);os.close(master)
            signal.signal(signal.SIGTTOU,signal.SIG_DFL)
            for descriptor in (gate_read,ready_write,slave):os.set_inheritable(descriptor,True)
            if os.read(gate_read,1)!=b'P':raise RuntimeError('active helper launch gate closed')
            os.execv(binary,[binary,'--codet-active-job-control-helper',str(gate_read),str(ready_write),str(slave)])
        except BaseException as error:
            os.write(2,('active helper setup/exec failed at line '+str(error.__traceback__.tb_lineno)+': '+str(error)+'; pid='+str(os.getpid())+' pgid='+str(os.getpgrp())+' sid='+str(os.getsid(0))+'\n').encode())
            os._exit(127)
    def token(expected):
        if not select.select([ready_read],[],[],5)[0]:raise TimeoutError('active helper gate timed out')
        if os.read(ready_read,1)!=expected:raise RuntimeError('unexpected active helper token')
    try:
        foreground_child(slave,child);os.write(gate_write,b'PG');token(b'R')
        os.tcsetpgrp(slave,os.getpgrp())
        foreign=termios.tcgetattr(slave);foreign[3]&=~termios.ECHO;foreign[6][termios.VMIN]=7
        termios.tcsetattr(slave,termios.TCSANOW,foreign)
        os.write(gate_write,b'E')
        deadline=time.monotonic()+5;status=None
        while time.monotonic()<deadline:
            collected,value=os.waitpid(child,os.WNOHANG|os.WUNTRACED)
            if collected:status=value;break
            select.select([],[],[],.01)
        stopped=status is not None and os.WIFSTOPPED(status) and os.WSTOPSIG(status)==signal.SIGTTOU
        unchanged=termios.tcgetattr(slave)==foreign
        if not stopped:raise RuntimeError('routine restoration did not stop with SIGTTOU')
        os.tcsetpgrp(slave,child);os.kill(child,signal.SIGCONT);token(b'E')
        after=termios.tcgetattr(slave)
        pending=getattr(termios,'PENDIN',0);before[3]&=~pending;after[3]&=~pending
        os.write(gate_write,b'X');_,status=os.waitpid(child,0);child=None
        print(json.dumps(dict(stopped_for_job_control=stopped,foreground_unchanged=unchanged,
            restored=before==after,exit=os.waitstatus_to_exitcode(status))))
    finally:
        if child is not None:os.kill(child,signal.SIGKILL);os.waitpid(child,0)
        os.tcsetpgrp(slave,os.getpgrp());termios.tcsetattr(slave,termios.TCSANOW,before)
        for descriptor in (master,slave,gate_read,gate_write,ready_read,ready_write):os.close(descriptor)
    return 0


def background_close(binary,mode):
    master,slave=pty.openpty()
    gate_read,gate_write=os.pipe();ready_read,ready_write=os.pipe()
    os.setsid();fcntl.ioctl(slave,termios.TIOCSCTTY,0)
    os.tcsetpgrp(slave,os.getpgrp())
    signal.signal(signal.SIGHUP,signal.SIG_IGN)
    signal.signal(signal.SIGTTOU,signal.SIG_IGN)
    before=termios.tcgetattr(slave)
    child=os.fork()
    if child==0:
        try:
            os.close(gate_write);os.close(ready_read);os.close(master)
            signal.signal(signal.SIGTTOU,signal.SIG_DFL)
            for fd in (gate_read,ready_write,slave):os.set_inheritable(fd,True)
            if os.read(gate_read,1)!=b'P':raise RuntimeError('background helper launch gate closed')
            os.execv(binary,[binary,'--codet-background-close-helper',mode,str(gate_read),str(ready_write),str(slave)])
        except BaseException as error:
            os.write(2,('background helper setup/exec failed at line '+str(error.__traceback__.tb_lineno)+': '+str(error)+'\n').encode())
            os._exit(127)
    os.close(gate_read);os.close(ready_write)
    startup=bytearray()
    def token(expected):
        nonlocal child
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            ready=select.select([ready_read,master],[],[],.02)[0]
            if master in ready:startup.extend(os.read(master,65536))
            if ready_read in ready:
                if os.read(ready_read,1)!=expected:
                    raise RuntimeError('unexpected cleanup token: '+repr(bytes(startup)))
                return
            collected,status=os.waitpid(child,os.WNOHANG|os.WUNTRACED)
            if collected:
                if not os.WIFSTOPPED(status):child=None
                raise RuntimeError('background helper stopped/exited before ready: '+str(status)+' '+repr(bytes(startup)))
        raise TimeoutError('background cleanup gate: '+repr(bytes(startup)))
    try:
        foreground_child(slave,child);os.write(gate_write,b'PG');token(b'R')
        while select.select([master],[],[],0)[0]:os.read(master,65536)
        os.tcsetpgrp(slave,os.getpgrp())
        foreign=[*before[:6],list(before[6])];foreign[3]&=~termios.ECHO
        foreign[6][termios.VMIN]=7
        termios.tcsetattr(slave,termios.TCSANOW,foreign)
        foreign=termios.tcgetattr(slave)
        os.write(master,b'typed-ahead\n')
        os.write(gate_write,b'E')
        deadline=time.monotonic()+5;status=None;output=b''
        while time.monotonic()<deadline:
            collected,value=os.waitpid(child,os.WNOHANG|os.WUNTRACED)
            if collected:status=value;break
            if select.select([master],[],[],.02)[0]:output+=os.read(master,65536)
        if status is None:raise TimeoutError('background close did not exit')
        if os.WIFSTOPPED(status):raise RuntimeError('background cleanup stopped')
        child=None
        while select.select([master],[],[],0)[0]:output+=os.read(master,65536)
        after=termios.tcgetattr(slave)
        pending=getattr(termios,'PENDIN',0);foreign[3]&=~pending;after[3]&=~pending
        queued=os.read(slave,1024) if select.select([slave],[],[],1)[0] else b''
        print(json.dumps(dict(exit=os.waitstatus_to_exitcode(status),foreground_unchanged=after==foreign,
            queued_preserved=queued==b'typed-ahead\n',cleanup_bytes=output.decode(errors='backslashreplace'))))
    finally:
        if child is not None:os.kill(child,signal.SIGKILL);os.waitpid(child,0)
        os.tcsetpgrp(slave,os.getpgrp());termios.tcsetattr(slave,termios.TCSANOW,before)
        for fd in (master,slave,gate_write,ready_read):os.close(fd)
    return 0


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("unknown-term", "background-term", "background-config", "background-partial", "active-job-control", "close-line", "close-curses", "close-detach-failure", "close-detach-failure-curses", "close-partial", "close-query-failure"):
        print("terminal helper expects MODE CODET", file=sys.stderr)
        return 2
    mode, binary = sys.argv[1:]
    if mode == "active-job-control":return active_job_control(binary)
    if mode.startswith("close-"):return background_close(binary,mode.removeprefix("close-"))
    master, slave = pty.openpty()
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
    os.tcsetpgrp(slave, os.getpgrp())
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    before = termios.tcgetattr(slave)
    command = [binary, "--ephemeral"]
    if mode == "background-partial":
        command = [binary, "--codet-terminal-startup-helper", "failed", "0"]
    if mode == "background-config":
        command += ["-c", "keybindings.invalid=1"]
    environment = dict(os.environ, TERM="codet-invalid-terminal-33" if mode.endswith("term") else "xterm")
    child = os.fork()
    if child == 0:
        if mode.startswith("background"):
            os.setpgid(0, 0)
        signal.signal(signal.SIGTTOU, signal.SIG_DFL)
        for descriptor in range(3):
            os.dup2(slave, descriptor)
        os.close(master)
        os.execve(binary, command, environment)
    result = {}
    output = bytearray()
    deadline = time.monotonic() + 10
    try:
        while time.monotonic() < deadline:
            if select.select([master], [], [], .02)[0]:
                output.extend(os.read(master, 65536))
            collected, status = os.waitpid(child, os.WNOHANG | os.WUNTRACED)
            if collected:
                if os.WIFSTOPPED(status):
                    result["stopped"] = os.WSTOPSIG(status)
                    os.kill(child, signal.SIGKILL)
                    os.waitpid(child, 0)
                else:
                    result["exit"] = os.waitstatus_to_exitcode(status)
                child = None
                break
        else:
            result["timeout"] = True
        while select.select([master], [], [], .02)[0]:
            output.extend(os.read(master, 65536))
        after = termios.tcgetattr(slave)
        # Darwin may set PENDIN while restoring otherwise identical settings.
        before[3] &= ~getattr(termios, "PENDIN", 0)
        after[3] &= ~getattr(termios, "PENDIN", 0)
        result["restored"] = before == after
        result["output"] = output.decode(errors="backslashreplace")
        print(json.dumps(result))
    finally:
        if child is not None:
            os.kill(child, signal.SIGKILL)
            os.waitpid(child, 0)
        os.close(master)
        os.close(slave)
    return 0


if __name__ == "__main__":
    sys.exit(main())
