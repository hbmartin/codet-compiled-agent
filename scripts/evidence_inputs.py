"""Explicit build inputs and tool resolution for local evidence campaigns."""

import csv
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

from verified_sources import file_hash

BUILD_ENV = {
    "CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "SDKROOT",
    "MACOSX_DEPLOYMENT_TARGET", "CMAKE_PREFIX_PATH", "CMAKE_TOOLCHAIN_FILE",
    "MAKEFLAGS", "GNUMAKEFLAGS", "CMAKE_GENERATOR", "CLANG_TIDY", "CPPCHECK",
    "PYTHONPATH", "PYTHONHOME",
}
DIRECTORIES = ("src", "include", "cmake", "prompts", "tests", "scripts", "checks",
               "requirements", ".github/workflows")
ROOT_FILES = {"CMakeLists.txt", "Makefile", "build.mk", "toolchain.mk", ".clang-format",
              ".clang-tidy", ".go-version", "ruff.toml"}


sha256 = file_hash


def write_csv(path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=sorted(set().union(*(r.keys() for r in rows))), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def selected(name):
    parts = name.split("/")
    return (".DS_Store" not in parts and "__pycache__" not in parts and not name.endswith(".pyc") and
            (name in ROOT_FILES or (len(parts) == 1 and name.endswith("lock.json")) or
             any(name.startswith(d + "/") for d in DIRECTORIES) or
             (len(parts) == 3 and name.startswith("docs/validation/") and name.endswith(".py"))))


def role_selected(name, role):
    parts = name.split("/")
    if ".DS_Store" in parts or "__pycache__" in parts or name.endswith(".pyc"):
        return False
    if role == "production":
        return name == "CMakeLists.txt" or name.startswith(("src/", "include/", "cmake/"))
    if role == "validation":
        return (name in {"Makefile", ".clang-format", ".clang-tidy", ".go-version", "ruff.toml"} or
                (len(parts) == 1 and name.endswith("lock.json")) or
                name.startswith(("tests/", "scripts/", "checks/", "requirements/", ".github/workflows/")) or
                (len(parts) == 3 and name.startswith("docs/validation/") and name.endswith(".py")))
    return selected(name)


def snapshot_files(directory):
    if not directory.exists():
        return
    with os.scandir(directory) as entries:
        for entry in entries:
            if entry.name in (".DS_Store", "__pycache__"):
                continue
            if entry.is_dir(follow_symlinks=False):
                yield from snapshot_files(Path(entry.path))
            elif entry.is_file():
                yield Path(entry.path)


def role_hashes(root, role):
    directories = (("src", "include", "cmake") if role == "production" else
                   ("tests", "scripts", "checks", "requirements", ".github/workflows") if role == "validation" else DIRECTORIES)
    paths = {root / name for name in ROOT_FILES if (root / name).is_file()}
    paths.update(p for p in root.glob("*lock.json") if p.is_file())
    for directory in directories:
        paths.update(snapshot_files(root / directory))
    if role != "production":
        paths.update(p for p in (root / "docs/validation").glob("*.py") if p.is_file())
    buffer = bytearray(64 * 1024) # Reused only inside this logical snapshot.
    return {str(p.relative_to(root)): sha256(p, buffer) for p in sorted(paths)
            if role_selected(str(p.relative_to(root)), role)}


def git_hashes(repository, ref, role, git="git"):
    names = subprocess.check_output([git, "-C", str(repository), "ls-tree", "-rz", "--name-only", ref], timeout=30).split(b"\0")
    names = [os.fsdecode(n) for n in names if n and role_selected(os.fsdecode(n), role)]
    if any("\n" in name for name in names):
        raise ValueError("Git batch input contains a newline")
    data = subprocess.check_output([git, "-C", str(repository), "cat-file", "--batch"],
                                   input="".join(ref + ":" + n + "\n" for n in names).encode(), timeout=30)
    result, offset = {}, 0
    for name in names:
        end = data.index(b"\n", offset)
        header = data[offset:end].split()
        if len(header) != 3 or header[1] != b"blob":
            raise ValueError("Invalid Git batch object header")
        size = int(header[2])
        if size < 0 or end + 1 + size >= len(data) or data[end + 1 + size] != 10:
            raise ValueError("Truncated Git batch object")
        result[name] = hashlib.sha256(data[end+1:end+1+size]).hexdigest()
        offset = end + 2 + size
    if offset != len(data):
        raise ValueError("Unexpected Git batch trailing bytes")
    return result


def build_hashes(root):
    return role_hashes(root, "build")


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
            expected = git_hashes(root, ref, "build", git)
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
                 if p.is_file() and ".DS_Store" not in p.parts and ".git" not in p.parts and "__pycache__" not in p.parts}
        serialized = "".join(n + "\0" + value + "\n" for n, value in files.items()).encode()
        result[name] = dict(sha256=hashlib.sha256(serialized).hexdigest(), files=len(files), archived=False)
    return result


def working_directory(manifest, check):
    return str((Path(manifest["root"]) / check.get("cwd", ".")).resolve())


def environment(manifest, check):
    # No inherited credentials, preload hooks, compiler overrides, or sanitizer settings.
    env = dict(manifest.get("environment", {"PATH": os.defpath}))
    env.update(check.get("env", {}))
    return env


def executable(argv, env, cwd=None):
    cwd = Path(cwd or Path.cwd())
    search = os.pathsep.join(str(cwd / p) for p in env.get("PATH", os.defpath).split(os.pathsep))
    candidate = str(cwd / argv[0]) if os.sep in argv[0] else argv[0]
    path = shutil.which(candidate, path=search)
    if path is None:
        raise FileNotFoundError("Executable not found: " + argv[0])
    # Normalize relative components while retaining the invocation symlink (virtualenv home).
    path = os.path.abspath(path)
    return dict(path=path, realpath=os.path.realpath(path), sha256=sha256(Path(path)))


def verify_tools(manifest):
    for name, tool in manifest.get("tools", {}).items():
        if executable([tool["path"]], manifest.get("environment", {})) != {k: tool[k] for k in ("path", "realpath", "sha256")}:
            raise ValueError("Prepared tool changed: " + name)
