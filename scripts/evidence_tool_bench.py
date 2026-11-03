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
from validation_evidence import production_hashes


def measure(root, ref, workload):
    calls = 0
    native = subprocess.check_output
    def counted(*args, **kwargs):
        nonlocal calls
        calls += 1
        return native(*args, **kwargs)
    subprocess.check_output = counted
    tracemalloc.start()
    start = time.perf_counter()
    try:
        if workload == "git-inspection":
            hashes = verify_git_source(root, root, ref)["production_hashes"]
        elif workload == "hashing":
            hashes = production_hashes(root)
        else:
            raise ValueError("Unknown tooling workload: " + workload)
        elapsed = (time.perf_counter() - start) * 1000
        stats = tracemalloc.take_snapshot().statistics("filename")
    finally:
        subprocess.check_output = native
        tracemalloc.stop()
    if not hashes or (workload == "git-inspection" and calls < 3):
        raise ValueError("Hash/Git work counter positive control failed")
    return dict(milliseconds=elapsed, files=len(hashes), git_commands=calls,
                allocations=sum(s.count for s in stats), allocated_bytes=sum(s.size for s in stats))


if __name__ == "__main__":
    root, ref, workload, output = sys.argv[1:]
    Path(output).write_text(json.dumps(measure(Path(root), ref, workload)) + "\n")
