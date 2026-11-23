"""Hashing, Git, and supervision observations with scoped memory/command counters."""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import tracemalloc
from contextlib import contextmanager
from pathlib import Path

# Load the declared support snapshot before selecting a variant's implementation.
import evidence_tiger_context_bench

# Select each variant's tooling through the same harness, without modifying its tree.
if __name__ == "__main__":
    sys.path.insert(0, str(Path(sys.argv[1]) / "scripts"))
from evidence_benchmarks import verify_git_source
from process_supervision import run as supervised_run
from validation_evidence import production_hashes


@contextmanager
def archive_hash_work(root, carrier):
    """Shared instrumentation counts bytes actually streamed into archive hashes."""
    import evidence_carrier
    import evidence_historical
    archive = (root / carrier["archive"]["path"]).resolve()
    observed = dict(archive_bytes=archive.stat().st_size, archive_hash_passes=0, archive_hash_bytes=0)
    saved = []
    try:
        for module in (evidence_carrier, evidence_historical):
            native = module.sha256
            def tracked(path, fallback=native):
                if Path(path).resolve() != archive:return fallback(path)
                value = hashlib.sha256()
                with Path(path).open('rb') as stream:
                    while True:
                        block = stream.read(65536)
                        if not block:break
                        value.update(block)
                        observed['archive_hash_bytes'] += len(block)
                observed['archive_hash_passes'] += 1
                return value.hexdigest()
            saved.append((module,native))
            module.sha256 = tracked
        yield observed
    finally:
        for module,native in reversed(saved):module.sha256 = native


def measure(root, ref, workload, repository=None, carrier=None):
    calls = git_calls = process_inspections = cmake_calls = 0
    historical_sample_audits = historical_rows = 0
    context = None
    layout_checks = build_locations = extraction_files = extraction_bytes = 0
    carrier_metrics = {}
    if workload == "build-context":
        context = evidence_tiger_context_bench.ContextWorkload()
    native = subprocess.check_output
    def counted(*args, **kwargs):
        nonlocal calls, git_calls, process_inspections, cmake_calls
        calls += 1
        argv = args[0] if args else kwargs.get('args', [])
        name = Path(argv[0]).name if isinstance(argv, (list, tuple)) and argv else ''
        git_calls += name == 'git'
        process_inspections += name == 'ps'
        cmake_calls += name == 'cmake'
        return native(*args, **kwargs)
    tracing = False
    failure = None
    try:
        subprocess.check_output = counted
        if not tracemalloc.is_tracing():
            tracemalloc.start()
            tracing = True
        control = bytearray(4096)
        if tracemalloc.get_traced_memory()[0] < len(control):
            raise ValueError("Python allocation observation positive control failed")
        del control
        tracemalloc.reset_peak()
        if context:
            context.start()
        start = time.perf_counter()
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
        elif workload == "dependency-resolution":
            from dependency_paths import dependency_root
            hashes = {}
            for number in range(1000):
                hashes[str(number)] = str(dependency_root(root))
        elif workload == "configuration-selection":
            from tempfile import TemporaryDirectory
            hashes = {}
            cmake,ninja=shutil.which('cmake'),shutil.which('ninja')
            if not cmake or not ninja:
                raise ValueError('Configuration measurement requires recorded CMake/Ninja tools')
            with TemporaryDirectory(prefix='codet configuration measure ') as directory:
                source=Path(directory)/'source';source.mkdir();build=Path(directory)/'build'
                (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(probe LANGUAGES NONE)\n'
                    +'include("'+str(root/'cmake/compiler-identity.cmake')+'")\n'
                    +'include("'+str(root/'cmake/dependency-root.cmake')+'")\n')
                first,second=Path(directory)/'first',Path(directory)/'second'
                for number,selected,reset,expected in ((0,first,False,first),(1,second,False,first),(2,second,True,second)):
                    command=[cmake,'-S',str(source),'-B',str(build),'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+ninja]
                    if reset:command.append('-UCODET_DEPENDENCY_ROOT')
                    subprocess.check_output(command,env=dict(os.environ,CODET_DEPENDENCY_ROOT=str(selected)),
                                            stderr=subprocess.STDOUT,timeout=30)
                    actual=(build/'CMakeFiles/codet-dependency-root.txt').read_text()
                    if actual!=str(expected):
                        raise ValueError('Configuration selection positive control failed')
                    hashes[str(number)]=True
            if cmake_calls!=3:
                raise ValueError('Configuration command positive control failed')
        elif workload == "historical-samples":
            import statistics

            from evidence_benchmarks import verify_samples
            rows = [dict(harness="preparation", workload="allocation-positive", preparation_metric_version=1,
                         iterations=128, allocations=128, allocated_bytes=8388608, payload_sized_allocations=128,
                         rejections=0, prepared_submissions=0, distinct_command_wrappers=0, effective_content_bytes=0,
                         set=number, sample=sample, variant=variant, warmup=sample == 0, milliseconds=float(sample + 1))
                    for number in (1, 2) for variant in ("baseline", "changed") for sample in range(6)]
            summary = []
            for number in (1, 2):
                for variant in ("baseline", "changed"):
                    values = [v for v in rows if v["set"] == number and v["variant"] == variant and not v["warmup"]]
                    metrics = [k for k, v in values[0].items() if type(v) in (int, float) and k not in ("set", "sample")]
                    summary.append(dict(set=number, harness="preparation", workload="allocation-positive", variant=variant,
                                        **{k: statistics.median(v[k] for v in values) for k in metrics}))
            hashes = {}
            for number in range(32):
                audited = verify_samples(rows, summary, {"preparation": ["allocation-positive"]}, historical=True)
                if audited["samples"] != 24 or audited["measured_samples"] != 20:
                    raise ValueError("Historical sample audit positive control failed")
                historical_sample_audits += 1
                historical_rows += audited["samples"]
                hashes[str(number)] = True
        elif workload == "campaign-layout":
            from tempfile import TemporaryDirectory

            from evidence_campaign import resolve_build_layout
            hashes = {}
            with TemporaryDirectory(prefix="codet-layout-measure-") as directory:
                campaign = Path(directory) / "new campaign"
                settings = dict(debug="custom/debug", asan="asan", tsan="tsan", cross="cross", make="other space/make")
                for number in range(128):
                    locations = resolve_build_layout(campaign, settings)
                    if set(locations) != set(settings) or campaign.exists():
                        raise ValueError("Layout measurement positive control failed")
                    layout_checks += 1
                    build_locations += len(locations)
                    hashes[str(number)] = locations
        elif workload == "historical-extraction":
            from tempfile import TemporaryDirectory

            from evidence_carrier import check_carrier
            from evidence_historical import audit_package
            hashes = {}
            with TemporaryDirectory(prefix="codet-extraction-measure-") as directory:
                destination = Path(directory) / "verified"
                with archive_hash_work(repository or root, carrier) as hash_work:
                    package = check_carrier(repository or root, carrier)
                    audited = audit_package(package, destination)
                    check_carrier(repository or root, carrier)
                if audited.get("publication", {}).get("interruptions"):
                    print('Interrupted extraction publication: ' + json.dumps(audited['publication']), file=sys.stderr)
                    error = ValueError('Interrupted extraction measurement cannot produce a sample')
                    error.historical_publication = audited
                    raise error
                if audited["archive_sha256"] != carrier["archive"]["sha256"]:
                    raise ValueError("Extraction returned a different shared carrier")
                carrier_metrics = dict(carrier_path=carrier["path"], carrier_archive_sha256=audited["archive_sha256"],
                                       carrier_transport_sha256=carrier["transport"]["sha256"], timing_scope="verified-operation",
                                       publication_interruptions=0, **hash_work)
                if not audited["strict_rejected"] or not audited["historical_result"]["verified"] or audited["historical_result"]["eligible_current"]:
                    raise ValueError("Extraction measurement positive control failed")
                extraction_files, extraction_bytes = audited["expanded_files"], audited["expanded_bytes"]
                hashes["audited"] = audited["archive_sha256"]
            if Path(directory).exists():
                raise ValueError("Extraction measurement retained owned storage")
        elif workload == "measurement-cleanup":
            from tempfile import TemporaryDirectory
            hashes = {}
            for number in range(100):
                with TemporaryDirectory(prefix="codet-cleanup-measure-") as directory:
                    target = Path(directory) / "owned"
                    target.write_bytes(b"owned")
                if Path(directory).exists():
                    raise ValueError("Measurement resource cleanup positive control failed")
                hashes[str(number)] = True
        elif workload == "build-context":
            hashes = context.perform()
        else:
            raise ValueError("Unknown tooling workload: " + workload)
        elapsed = (time.perf_counter() - start) * 1000
        retained, peak = tracemalloc.get_traced_memory()
        stats = tracemalloc.take_snapshot().statistics("filename")
    except BaseException as error:
        failure = error
        raise
    finally:
        try:
            if context:
                context.close()
        except BaseException:
            if failure is None:
                raise
        finally:
            subprocess.check_output = native
            if tracing:
                tracemalloc.stop()
    if not hashes or (workload == "git-inspection" and calls < 3):
        raise ValueError("Hash/Git work counter positive control failed")
    return dict(**(dict(context_metric_version=1, **context.counts) if context else {}), case=workload, milliseconds=elapsed, files=(extraction_files if workload == "historical-extraction" else 0 if workload in ("supervision", "configuration-selection", "historical-samples", "campaign-layout") else len(hashes)),
                owned_processes=len(hashes) if workload == "supervision" else 0,
                check_output_calls=calls, git_commands=git_calls,
                process_inspection_commands=process_inspections,
                **(dict(configuration_metric_version=1,cmake_commands=cmake_calls,configuration_checks=len(hashes)) if workload=="configuration-selection" else {}),
                **(dict(historical_sample_audits=historical_sample_audits, historical_rows=historical_rows) if workload == "historical-samples" else {}),
                **(dict(layout_metric_version=2, layout_checks=layout_checks, build_locations=build_locations) if workload == "campaign-layout" else {}),
                **(dict(extraction_metric_version=3, historical_extractions=len(hashes), expanded_files=extraction_files, expanded_bytes=extraction_bytes) if workload == "historical-extraction" else {}),
                **carrier_metrics, metric_version=2, retained_traced_blocks=sum(s.count for s in stats), retained_traced_bytes=retained, peak_traced_bytes=peak)


if __name__ == "__main__":
    root, ref, workload, output = sys.argv[1:5]
    repository = Path(sys.argv[5]) if len(sys.argv) >= 6 else None
    carrier = json.loads(sys.argv[6]) if len(sys.argv) == 7 else None
    Path(output).write_text(json.dumps(measure(Path(root), ref, workload, repository, carrier)) + "\n")
