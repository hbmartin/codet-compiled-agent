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
            os.setpgid(0,0)
            signal.signal(signal.SIGTTOU,signal.SIG_DFL)
            for descriptor in (gate_read,ready_write,slave):os.set_inheritable(descriptor,True)
            os.execv(binary,[binary,'--codet-active-job-control-helper',str(gate_read),str(ready_write),str(slave)])
        except BaseException as error:
            os.write(2,('active helper exec failed: '+str(error)+'\n').encode())
            os._exit(127)
    def token(expected):
        if not select.select([ready_read],[],[],5)[0]:raise TimeoutError('active helper gate timed out')
        if os.read(ready_read,1)!=expected:raise RuntimeError('unexpected active helper token')
    try:
        os.setpgid(child,child);os.tcsetpgrp(slave,child);os.write(gate_write,b'G');token(b'R')
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


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("unknown-term", "background-term", "background-config", "background-partial", "active-job-control"):
        print("terminal helper expects MODE CODET", file=sys.stderr)
        return 2
    mode, binary = sys.argv[1:]
    if mode == "active-job-control":return active_job_control(binary)
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
