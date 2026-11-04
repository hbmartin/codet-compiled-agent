"""Production-site mutations proving the pinned provider rules catch their regressions."""
import json

from evidence_benchmarks import replace_once
from process_supervision import run as supervised_run


def run_controls(root, folder, executable):
    source = (root / "src/modern/provider.cpp").read_text()
    cases = [
        ("reasoning", 'return c.provider == "openai" &&\n         (!c.effort.empty() || c.model == "gpt-6.1-sol" || c.model == "gpt-6-astra");',
         'return c.provider == "openai";', "codet-public-reasoning-helper-provider-only"),
        ("empty-replay", '      if (!result.message.replay.empty())\n        wire.push_back', '      wire.push_back', "codet-unguarded-epoch-replay"),
    ]
    rows = []
    for name, before, after, expected in cases:
        results = []
        for label, text in (("baseline", source), ("mutant", replace_once(source, before, after))):
            directory = folder / ("semgrep-control-" + name) / label
            directory.mkdir(parents=True)
            path = directory / "provider.cpp"
            path.write_text(text)
            argv = [executable, "scan", "--no-git-ignore", "--json", "--error", "--metrics=off", "--disable-version-check", "--no-rewrite-rule-ids", "--config", str(root / "checks/semgrep/provider.yaml"), str(path)]
            run = supervised_run(argv, text=True, capture_output=True, timeout=30)
            report = json.loads(run.stdout)
            ids = [r["check_id"] for r in report["results"]]
            if report.get("errors") or run.returncode != (0 if label == "baseline" else 1) or (label == "baseline" and ids) or (label == "mutant" and expected not in ids):
                raise ValueError("Semgrep production control failed: " + repr((name, label, run.returncode, ids, run.stderr)))
            results.append(dict(label=label, argv=argv, exit=run.returncode, ids=ids))
        rows.append(dict(control=name, results=results))
    return rows
