"""Bounded host process inspection with real argument boundaries."""

import ctypes
import errno
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

from process_supervision import system_ps

NATIVE_BLOCKERS = {"ctest", "codet_tests", "codet_publication_fault_tests", "clang-tidy", "cppcheck", "ninja", "pytest"}
SCRIPT_BLOCKERS = {"analyze-modern.py", "cli_integration.py", "test_validation_evidence.py", "gtest_isolation.py"}
SNAPSHOT_TIMEOUT = 5
SNAPSHOT_REPORT_RESERVE_NS = 200_000_000
DARWIN_RETRY_DELAYS = (0.075, 0.150)
MIN_RETRY_SWEEP_SECONDS = 0.100


def matches_native(name):
    name = Path(name).name
    return (name in NATIVE_BLOCKERS or re.sub(r"-\d+(?:\.\d+)*$", "", name) in NATIVE_BLOCKERS or
            (len(name) in (15, 16) and any(len(n) > len(name) and n.startswith(name) for n in NATIVE_BLOCKERS)))


def script_identity(name):
    name = Path(name).name
    return name in SCRIPT_BLOCKERS or (len(name) == 15 and any(n.startswith(name) for n in SCRIPT_BLOCKERS))


def needs_arguments(name):
    base = Path(name).name
    return (base.lower().startswith("python") or matches_native(name) or script_identity(name) or
            (len(base) in (15, 16) and any(base.startswith(n + "-") for n in NATIVE_BLOCKERS)))


def python_program(arguments):
    values = list(arguments)
    while values:
        flag = values.pop(0)
        if flag == "--":
            return values
        if not flag.startswith("-") or flag == "-":
            return [flag] + values
        if flag.startswith("--"):
            if flag == "--check-hash-based-pycs":
                if not values:
                    raise ValueError("Missing Python switch operand")
                values.pop(0)
            elif flag.startswith("--check-hash-based-pycs="):
                if not flag.split("=", 1)[1]:
                    raise ValueError("Missing Python switch operand")
            elif flag in ("--help", "--version", "--help-env", "--help-xoptions", "--help-all"):
                return []
            else:
                raise ValueError("Ambiguous Python switch: " + flag)
            continue
        cluster = flag[1:]
        for position, option in enumerate(cluster):
            rest = cluster[position + 1:]
            if option == "c":
                return [] # Command text is deliberately opaque, never a script identity.
            if option in "mWX":
                operand = rest or (values.pop(0) if values else "")
                if not operand:
                    raise ValueError("Missing Python switch operand")
                if option == "m":
                    return ["-m", operand] + values
                break
            if option not in "bBdEhiIOPqRsSuvVx":
                raise ValueError("Ambiguous Python switch: " + flag)
    return []


def blocking_processes(processes):
    found = []
    for process in processes:
        executable, argv = Path(process["executable"]).name, process["argv"]
        if matches_native(executable) or (argv and matches_native(argv[0])) or script_identity(executable):
            found.append(process)
        elif executable.lower().startswith("python") or (argv and Path(argv[0]).name.lower().startswith("python")):
            arguments = python_program(argv[1:])
            if arguments and (Path(arguments[0]).name in SCRIPT_BLOCKERS or
                              arguments[:2] in (["-m", "unittest"], ["-m", "pytest"])):
                found.append(process)
    return found


def darwin_arguments(data):
    if len(data) < ctypes.sizeof(ctypes.c_int):
        raise ValueError("Malformed KERN_PROCARGS2 header")
    count = ctypes.c_int.from_buffer_copy(data).value
    if count <= 0:
        raise ValueError("Malformed KERN_PROCARGS2 argc")
    start = data.find(b"\0", ctypes.sizeof(ctypes.c_int))
    if start < 0:
        raise ValueError("Missing executable terminator")
    start += 1
    while start < len(data) and data[start] == 0:
        start += 1
    values = data[start:].split(b"\0")
    if len(values) <= count:
        raise ValueError("Truncated KERN_PROCARGS2 argv")
    return [os.fsdecode(s) for s in values[:count]]


def read_argv(pid, platform=None):
    platform = platform or sys.platform
    if platform.startswith("linux"):
        data = Path("/proc") / str(pid) / "cmdline"
        raw = data.read_bytes()
        if not raw or not raw.endswith(b"\0"):
            raise ValueError("Missing or malformed process argv: " + str(pid))
        return [os.fsdecode(s) for s in raw[:-1].split(b"\0")]
    if platform == "darwin":
        library = ctypes.CDLL(None, use_errno=True)
        function = library.sysctl
        function.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_uint, ctypes.c_void_p,
                             ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p, ctypes.c_size_t]
        function.restype = ctypes.c_int
        bound = os.sysconf("SC_ARG_MAX")
        if not 0 < bound <= 16 * 1024 * 1024:
            raise ValueError("Unsupported process argument bound")
        buffer = ctypes.create_string_buffer(bound)
        size = ctypes.c_size_t(bound)
        mib = (ctypes.c_int * 3)(1, 49, int(pid))  # CTL_KERN, KERN_PROCARGS2 (Darwin SDK)
        if function(mib, 3, buffer, ctypes.byref(size), None, 0):
            code = ctypes.get_errno()
            raise OSError(code, os.strerror(code))
        return darwin_arguments(buffer.raw[:size.value])
    raise ValueError("Process inspection unsupported on " + platform)


def _snapshot_now_ns():
    # Match the subprocess timeout's awake time using a clock shared across processes.
    clock = time.CLOCK_UPTIME_RAW if sys.platform == "darwin" else time.CLOCK_MONOTONIC
    return time.clock_gettime_ns(clock)


def _seconds_left(deadline_ns):
    return (deadline_ns - _snapshot_now_ns()) / 1_000_000_000


def _darwin_argv_failure(pid, state, executable, error):
    message = f"Cannot read argv for PID {pid} ({executable}, state {state}): {error.strerror}"
    failure = OSError(error.errno, message)
    failure.__cause__ = error
    pid_number = int(pid)
    if pid_number <= 0:
        raise failure
    try:
        os.kill(pid_number, 0)
    except ProcessLookupError:
        return None
    except OSError as probe_error:
        if probe_error.errno != errno.EPERM:
            raise OSError(probe_error.errno, f"{message}; signal-zero probe failed: {probe_error.strerror}") from failure
        failure = OSError(error.errno, f"{message}; signal-zero probe denied: {probe_error}")
        failure.__cause__ = error
    return failure


def _prepare_darwin_retry(failure, delay, deadline_ns):
    if delay is None:
        raise failure
    if _seconds_left(deadline_ns) < delay + MIN_RETRY_SWEEP_SECONDS:
        raise failure
    print(f"{failure}; retrying process snapshot after {delay:.3f}s", file=sys.stderr, flush=True)
    time.sleep(delay)


def process_snapshot(deadline_ns=None):
    if sys.platform != "darwin" and not sys.platform.startswith("linux"):
        raise ValueError("Process inspection unsupported on " + sys.platform)
    if deadline_ns is None:
        deadline_ns = _snapshot_now_ns() + SNAPSHOT_TIMEOUT * 1_000_000_000
    retries = 0
    retry_failure = None
    while True:
        remaining = _seconds_left(deadline_ns)
        if retry_failure is not None and remaining < MIN_RETRY_SWEEP_SECONDS:
            raise retry_failure
        if remaining <= 0:
            raise TimeoutError("Process inspection deadline expired")
        try:
            identities = subprocess.check_output([system_ps(), "-axo", "pid=,stat=,comm="],
                                                 text=True, timeout=remaining)
        except subprocess.TimeoutExpired as error:
            if retry_failure is not None:
                raise error from retry_failure
            raise
        processes = []
        for line in identities.splitlines():
            fields = line.strip().split(None, 2)
            if len(fields) != 3 or not fields[0].isdigit():
                raise ValueError("Malformed process identity: " + line)
            pid, state, executable = fields
            if state.startswith("Z"):
                continue
            arguments = [executable]
            if needs_arguments(executable):
                try:
                    arguments = read_argv(pid)
                except OSError as error:
                    if error.errno in (errno.ENOENT, errno.ESRCH):
                        continue
                    # Darwin uses EINVAL for both exit races and denied argv reads.
                    # A retry always discards the partial sweep.
                    if sys.platform == "darwin" and error.errno == errno.EINVAL:
                        failure = _darwin_argv_failure(pid, state, executable, error)
                        if failure is None:
                            continue
                        delay = DARWIN_RETRY_DELAYS[retries] if retries < len(DARWIN_RETRY_DELAYS) else None
                        _prepare_darwin_retry(failure, delay, deadline_ns)
                        retry_failure = failure
                        retries += 1
                        break
                    raise
                if sys.platform.startswith("linux"):
                    try:
                        executable = os.readlink("/proc/" + pid + "/exe")
                    except OSError as error:
                        if error.errno in (errno.ENOENT, errno.ESRCH):
                            continue
                        raise
            processes.append(dict(pid=pid, executable=executable, argv=arguments))
        else:
            if not processes:
                raise ValueError("Process inspection returned no usable records")
            if _seconds_left(deadline_ns) <= 0:
                raise TimeoutError("Process inspection deadline expired") from retry_failure
            return processes


def require_quiet_processes():
    # Use a host-wide clock so interpreter startup also spends the worker budget.
    deadline_ns = _snapshot_now_ns() + SNAPSHOT_TIMEOUT * 1_000_000_000 - SNAPSHOT_REPORT_RESERVE_NS
    try:
        processes = json.loads(subprocess.check_output(
            [sys.executable, str(Path(__file__).resolve()), "--snapshot", "--deadline-ns", str(deadline_ns)],
            text=True, stderr=subprocess.PIPE, timeout=SNAPSHOT_TIMEOUT))
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        stderr = error.stderr or ''
        if isinstance(stderr, bytes):
            stderr = stderr.decode('utf-8', 'replace')
        observation = dict(inspector_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                           deadline_seconds=SNAPSHOT_TIMEOUT, reason=type(error).__name__,
                           returncode=getattr(error, 'returncode', None), stderr=stderr[-16384:])
        print('Measurement inspection unavailable: ' + json.dumps(observation, sort_keys=True),
              file=sys.stderr)
        raise
    if not isinstance(processes, list) or not processes:
        raise ValueError("Process inspection returned no usable records")
    blocked = blocking_processes(processes)
    if blocked:
        raise ValueError("Concurrent validation/build/analyzer processes: " + repr(blocked))


def validate_inspection_failure(value, inspector_digest):
    if (not isinstance(value, dict) or set(value) != {'inspector_sha256', 'deadline_seconds', 'reason', 'returncode', 'stderr'}
            or value['inspector_sha256'] != inspector_digest
            or type(value['deadline_seconds']) is not int or value['deadline_seconds'] != SNAPSHOT_TIMEOUT
            or not isinstance(value['stderr'], str) or len(value['stderr']) > 16384):
        raise ValueError('Unavailable process inspection is not source-bound')
    if value['reason'] == 'CalledProcessError':
        if type(value['returncode']) is not int or value['returncode'] == 0:
            raise ValueError('Unavailable process inspection has no failed outcome')
    elif value['reason'] == 'TimeoutExpired':
        if value['returncode'] is not None:
            raise ValueError('Timed-out process inspection has an exit status')
    else:
        raise ValueError('Unsupported unavailable process inspection outcome')


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", action="store_true", help="print a process snapshot (default)")
    parser.add_argument("--deadline-ns", type=int)
    print(json.dumps(process_snapshot(parser.parse_args().deadline_ns)))
