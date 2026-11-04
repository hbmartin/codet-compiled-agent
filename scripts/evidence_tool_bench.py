"""Small replayable hashing/Git workload; reports Python allocation and subprocess work."""
import json
import subprocess
import sys
import time
import tracemalloc
from pathlib import Path

# Select each variant's tooling through the same harness, without modifying its tree.
if __name__ == "__main__":
    sys.path.insert(0, str(Path(sys.argv[1]) / "scripts"))
from evidence_benchmarks import verify_git_source
from process_supervision import run as supervised_run
from validation_evidence import production_hashes


def measure(root, ref, workload, repository=None):
    calls = 0
    native = subprocess.check_output
    def counted(*args, **kwargs):
        nonlocal calls
        calls += 1
        return native(*args, **kwargs)
    subprocess.check_output = counted
    tracemalloc.start()
    control = bytearray(4096)
    if tracemalloc.get_traced_memory()[0] < len(control):
        raise ValueError("Python allocation observation positive control failed")
    del control
    tracemalloc.reset_peak()
    start = time.perf_counter()
    try:
        if workload == "git-inspection":
            hashes = verify_git_source(repository or root, root, ref)["production_hashes"]
        elif workload == "hashing":
            hashes = production_hashes(root)
        elif workload == "supervision":
            hashes = {}
            for number in range(5):
                try:
                    supervised_run([sys.executable, "-c", "import time;time.sleep(2)"], timeout=.02, cleanup_grace=.2)
                except subprocess.TimeoutExpired as error:
                    if not error.process_cleanup.get("reaped"):
                        raise ValueError("Supervision positive control did not reap its leader") from error
                    hashes[str(number)] = error.process_cleanup["reaped"]
                else:
                    raise ValueError("Supervision positive control did not time out")
        else:
            raise ValueError("Unknown tooling workload: " + workload)
        elapsed = (time.perf_counter() - start) * 1000
        retained, peak = tracemalloc.get_traced_memory()
        stats = tracemalloc.take_snapshot().statistics("filename")
    finally:
        subprocess.check_output = native
        tracemalloc.stop()
    if not hashes or (workload == "git-inspection" and calls < 3):
        raise ValueError("Hash/Git work counter positive control failed")
    return dict(case=workload, milliseconds=elapsed, files=len(hashes), git_commands=calls,
                metric_version=2, retained_traced_blocks=sum(s.count for s in stats), retained_traced_bytes=retained, peak_traced_bytes=peak)


if __name__ == "__main__":
    root, ref, workload, output = sys.argv[1:5]
    repository = Path(sys.argv[5]) if len(sys.argv) == 6 else None
    Path(output).write_text(json.dumps(measure(Path(root), ref, workload, repository)) + "\n")
