"""Build one declared harness snapshot against each immutable production variant."""
import json
import shlex
import shutil
import subprocess
from pathlib import Path

from evidence_inputs import sha256
from process_supervision import run as supervised_run

PROGRAMS = dict(line="codet_line_input_bench", output="codet_output_bench", export="codet_export_bench")


def build_harnesses(root, folder, names, ninja):
    snapshot = folder / "harness-src"
    snapshot.mkdir()
    hashes = {}
    for name in names:
        target = snapshot / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / name, target)
        hashes[name] = sha256(target)
    variants = {}
    for variant in ("baseline", "changed"):
        build = folder / (variant + "-release")
        directory = folder / (variant + "-harness")
        directory.mkdir()
        database = json.loads((build / "compile_commands.json").read_text())
        records = {}
        for name, target in PROGRAMS.items():
            entry = next(e for e in database if e["file"].endswith("tests/" + {"line": "line_input", "output": "output", "export": "export"}[name] + "_bench.cpp"))
            compile_argv = list(entry.get("arguments") or shlex.split(entry["command"]))
            old_object = compile_argv[compile_argv.index("-o") + 1]
            obj = directory / (name + ".o")
            compile_argv[compile_argv.index("-o") + 1] = str(obj)
            compile_argv[compile_argv.index(entry["file"])] = str(snapshot / "tests" / Path(entry["file"]).name)
            compile_argv[1:1] = ["-I" + str(snapshot / "tests")]
            lines = subprocess.check_output([ninja, "-C", str(build), "-t", "commands", target], text=True, timeout=30).splitlines()
            link = shlex.split(lines[-1])
            if "&&" in link:
                link = link[link.index("&&") + 1:]
                if "&&" in link:
                    link = link[:link.index("&&")]
            binary = directory / target
            link[link.index(old_object)] = str(obj)
            link[link.index("-o") + 1] = str(binary)
            for argv in (compile_argv, link):
                supervised_run(argv, cwd=build, check=True, timeout=600)
            records[name] = dict(compile_argv=compile_argv, link_argv=link, library_sha256=sha256(build / "libcodet_core.a"),
                                 executable_sha256=sha256(binary), executable=str(binary))
        variants[variant] = records
    return dict(harness_hashes=hashes, variants=variants)
