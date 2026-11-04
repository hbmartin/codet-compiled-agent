"""Bounded supervision of a subprocess and the process group it owns."""
import os
import shutil
import signal
import subprocess
import threading
import time
from contextlib import contextmanager


@contextmanager
def interruption():
    # Only the owning main thread can change Python's handlers. Child groups never inherit
    # a different process-wide disposition; their exec restores Python's normal semantics.
    if threading.current_thread() is not threading.main_thread():
        yield
        return
    previous = signal.getsignal(signal.SIGTERM)
    def terminate(signum, frame):
        raise KeyboardInterrupt('Interrupted by SIGTERM')
    signal.signal(signal.SIGTERM, terminate)
    try:
        yield
    finally:
        signal.signal(signal.SIGTERM, previous)


def system_ps():
    path = shutil.which("ps", path=os.defpath)
    if path is None:
        raise ValueError("System process inspection unavailable")
    return path


def group_inactive(group, *, timeout=2):
    # Darwin can return EPERM for a group containing only unreaped zombies. Establish
    # that state explicitly; permission failure alone never proves successful cleanup.
    output = subprocess.check_output([system_ps(), "-axo", "pgid=,stat="], text=True, timeout=timeout)
    members = []
    for line in output.splitlines():
        values = line.split()
        if len(values) != 2 or not values[0].isdigit():
            raise ValueError("Malformed cleanup process-group inspection")
        if int(values[0]) == group:
            members.append(values[1])
    return all(state.startswith("Z") for state in members), members


@contextmanager
def cleanup_interruptions(observation):
    """Defer further signals during cleanup; never install ignored dispositions."""
    if threading.current_thread() is not threading.main_thread():
        yield
        return
    selected = (signal.SIGINT, signal.SIGTERM)
    previous = {}
    def defer(signum, frame):
        observation['interruptions'].append(dict(signal=signum))
    def mask():
        return signal.pthread_sigmask(signal.SIG_BLOCK, selected) if hasattr(signal, 'pthread_sigmask') else None
    def unmask(value):
        if value is not None:
            signal.pthread_sigmask(signal.SIG_SETMASK, value)
    blocked = mask()
    try:
        for signum in selected:
            previous[signum] = signal.getsignal(signum)
            signal.signal(signum, defer)
    finally:
        unmask(blocked)
    try:
        yield
    finally:
        blocked = mask()
        try:
            for signum, handler in previous.items():
                signal.signal(signum, handler)
        finally:
            unmask(blocked)


def stop_group(process, grace=2.0):
    if process.pid <= 0:
        raise ValueError('Cannot supervise a non-positive process group')
    observation = dict(group=process.pid, signals=[], reaped=False, interruptions=[], failures=[])
    process.group_cleanup = observation
    started = time.monotonic()
    term_deadline = started + max(0, grace)
    reap_deadline = term_deadline + 2

    def attempt(operation, deadline, once=False):
        while once or time.monotonic() < deadline:
            once = False
            try:
                return True, operation(max(0, deadline - time.monotonic()))
            except KeyboardInterrupt as error:
                observation['interruptions'].append(dict(exception=type(error).__name__))
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                observation['failures'].append(str(error))
                return False, None
        return False, None

    def inspect(deadline):
        success, result = attempt(lambda remaining: group_inactive(
            process.pid, timeout=min(.2, remaining)), deadline)
        if success:
            inactive, members = result
            observation['group_states'] = members
            return inactive
        return False

    def send(action, deadline):
        def operation(remaining):
            try:
                os.killpg(process.pid, action)
                observation['signals'].append(dict(signal=int(action), outcome='delivered'))
                return False
            except ProcessLookupError:
                observation['signals'].append(dict(signal=int(action), outcome='absent'))
                return True
            except PermissionError:
                if inspect(time.monotonic() + remaining):
                    observation['signals'].append(dict(signal=int(action), outcome='no-live-members'))
                    return True
                raise
        return attempt(operation, deadline, once=True)

    result = None
    with cleanup_interruptions(observation):
        sent, inactive = send(signal.SIGTERM, reap_deadline)
        inactive = bool(sent and inactive)
        while not inactive and time.monotonic() < term_deadline:
            inactive = inspect(term_deadline)
            if not inactive:
                attempt(lambda remaining: time.sleep(min(.05, remaining)), term_deadline)
        if not inactive:
            killed, _ = send(signal.SIGKILL, reap_deadline)
            if not killed:
                attempt(lambda remaining: process.kill(), reap_deadline)
        # No further group signal follows this point. Reaping now permits PID reuse safely.
        while time.monotonic() < reap_deadline:
            try:
                result = process.wait(timeout=min(.05, max(0, reap_deadline-time.monotonic())))
                observation.update(reaped=True, returncode=result)
                break
            except KeyboardInterrupt as error:
                observation['interruptions'].append(dict(exception=type(error).__name__))
            except subprocess.TimeoutExpired:
                continue
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                observation['failures'].append(str(error))
                break
    observation['elapsed_seconds'] = time.monotonic() - started
    if not observation['reaped']:
        observation['failures'].append('Owned process group was not reaped before cleanup deadline')
    if observation['failures']:
        failure = RuntimeError('; '.join(observation['failures']))
        failure.reaped_returncode = result
        raise failure
    return result


def run(argv, *, timeout, check=False, capture_output=False, input=None, cleanup_grace=2.0, **kwargs):
    if capture_output:
        if 'stdout' in kwargs or 'stderr' in kwargs:
            raise ValueError('capture_output conflicts with explicit streams')
        kwargs.update(stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    with interruption():
        try:
            process = subprocess.Popen(argv, start_new_session=True, **kwargs)
        except OSError as error:
            error.process_launch_failure = True
            raise
        try:
            stdout, stderr = process.communicate(input=input, timeout=timeout)
        except BaseException as error:
            try:
                error.returncode = stop_group(process, cleanup_grace)
            except BaseException as cleanup:
                error.returncode = getattr(cleanup, "reaped_returncode", process.returncode)
                error.cleanup_failure = str(cleanup)
            error.process_cleanup = getattr(process, "group_cleanup", {})
            raise
    result = subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)
    if check:
        result.check_returncode()
    return result
