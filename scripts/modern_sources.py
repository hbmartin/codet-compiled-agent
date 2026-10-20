"""The maintained native source inventory shared by offline checks."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}


def application_sources(root=ROOT):
    sources = {p for p in (root / "src/modern").rglob("*") if p.suffix in SOURCE_SUFFIXES}
    sources.add(root / "src/text_compat.c")
    return sorted(sources)


def maintained_native_files(root=ROOT):
    headers = {p for p in (root / "include/codet").rglob("*") if p.suffix in HEADER_SUFFIXES}
    headers.add(root / "include/text_compat.h")
    tests = {
        p for p in (root / "tests").rglob("*") if p.suffix in SOURCE_SUFFIXES | HEADER_SUFFIXES
    }
    return sorted(set(application_sources(root)) | headers | tests)


def policy_inputs(root=ROOT):
    # Every maintained production source/header is parsed, even when no narrow
    # regression rule applies. Fixtures and historical recovered C stay separate.
    return [p for p in maintained_native_files(root) if not p.is_relative_to(root / "tests")]
