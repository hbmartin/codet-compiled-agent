"""Explicit build inputs and tool resolution for local evidence campaigns."""

import hashlib
import os
import shutil
import subprocess
from pathlib import Path

BUILD_ENV = {
    "CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "SDKROOT",
    "MACOSX_DEPLOYMENT_TARGET", "CMAKE_PREFIX_PATH", "CMAKE_TOOLCHAIN_FILE",
    "MAKEFLAGS", "GNUMAKEFLAGS", "CMAKE_GENERATOR", "CLANG_TIDY", "CPPCHECK",
    "PYTHONPATH", "PYTHONHOME",
}
DIRECTORIES = ("src", "include", "cmake", "prompts", "tests", "scripts", "checks",
               "requirements", ".github/workflows")
ROOT_FILES = {"CMakeLists.txt", "Makefile", "build.mk", "toolchain.mk", ".clang-format",
              ".clang-tidy", ".go-version"}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def selected(name):
    path = Path(name)
    return ("__pycache__" not in path.parts and path.suffix != ".pyc" and
            (name in ROOT_FILES or (len(path.parts) == 1 and name.endswith("lock.json")) or
             any(name.startswith(d + "/") for d in DIRECTORIES) or
             (len(path.parts) == 3 and name.startswith("docs/validation/") and name.endswith(".py"))))


def build_hashes(root):
    paths = [root / name for name in ROOT_FILES]
    paths.extend(root.glob("*lock.json"))
    for directory in DIRECTORIES:
        paths.extend(p for p in (root / directory).rglob("*") if p.is_file())
    paths.extend((root / "docs/validation").glob("*.py"))
    return {str(p.relative_to(root)): sha256(p) for p in sorted(set(paths))
            if p.is_file() and selected(str(p.relative_to(root)))}


def source_inputs(root, specs):
    result = {}
    for spec in specs:
        name = spec["path"]
        source = root / name
        if not source.is_dir():
            raise ValueError("Source input missing: " + name)
        actual = build_hashes(source)
        if not actual:
            raise ValueError("Source input is empty: " + name)
        if spec.get("git_ref"):
            ref = spec["git_ref"]
            git = spec.get("git") or shutil.which("git", path=os.defpath)
            names = subprocess.check_output([git, "-C", str(root), "ls-tree", "-rz", "--name-only", ref], timeout=30).split(b"\0")
            names = [os.fsdecode(n) for n in names if n and selected(os.fsdecode(n))]
            data = subprocess.check_output([git, "-C", str(root), "cat-file", "--batch"],
                                           input="".join(ref + ":" + n + "\n" for n in names).encode(), timeout=30)
            expected = {}
            offset = 0
            for name in names:
                end = data.index(b"\n", offset)
                size = int(data[offset:end].split()[-1])
                expected[name] = hashlib.sha256(data[end + 1:end + 1 + size]).hexdigest()
                offset = end + 2 + size
            if actual != expected:
                raise ValueError("Source input does not match Git commit: " + name)
        result[name] = actual
    return result


def file_specs(check):
    specs = {s if isinstance(s, str) else s["path"]: (dict(path=s, archive=True) if isinstance(s, str) else s)
             for s in check.get("file_inputs", [])}
    for artifact in check.get("artifacts", []):
        for name in artifact.get("inputs", []):
            specs.setdefault(name, dict(path=name, archive=True))
    return specs


def file_hashes(root, names):
    result = {}
    for name in names:
        path = root / name
        if not path.is_file():
            raise ValueError("Artifact input not produced: " + name)
        result[name] = sha256(path)
    return result


def output_hashes(root, specs):
    result = []
    for spec in specs:
        if not (root / spec["path"]).is_file():
            raise ValueError("Artifact output not produced: " + spec["path"])
        result.append(dict(path=spec["path"], sha256=sha256(root / spec["path"]), archive=spec.get("archive", True)))
    return result


def directory_hashes(root, names):
    result = {}
    for name in names:
        directory = root / name
        if not directory.is_dir():
            raise ValueError("External source input missing: " + name)
        files = {str(p.relative_to(directory)): sha256(p) for p in sorted(directory.rglob("*"))
                 if p.is_file() and ".git" not in p.parts and "__pycache__" not in p.parts}
        serialized = "".join(n + "\0" + value + "\n" for n, value in files.items()).encode()
        result[name] = dict(sha256=hashlib.sha256(serialized).hexdigest(), files=len(files), archived=False)
    return result


def environment(manifest, check):
    env = {k: v for k, v in os.environ.items() if k not in BUILD_ENV and k != "PATH"}
    env.update(manifest.get("environment", {"PATH": os.defpath}))
    env.update(check.get("env", {}))
    return env


def executable(argv, env):
    path = shutil.which(argv[0], path=env.get("PATH", os.defpath))
    if path is None:
        raise FileNotFoundError("Executable not found: " + argv[0])
    # Keep the invocation path: resolving a virtualenv symlink changes Python's home.
    path = str(Path(path).absolute())
    return dict(path=path, realpath=os.path.realpath(path), sha256=sha256(Path(path)))


def verify_tools(manifest):
    for name, tool in manifest.get("tools", {}).items():
        if executable([tool["path"]], manifest.get("environment", {})) != {k: tool[k] for k in ("path", "realpath", "sha256")}:
            raise ValueError("Prepared tool changed: " + name)
