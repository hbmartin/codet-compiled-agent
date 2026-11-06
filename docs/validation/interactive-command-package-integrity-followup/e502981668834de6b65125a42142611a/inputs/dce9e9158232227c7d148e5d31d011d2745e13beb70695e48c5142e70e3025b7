"""One dependency-root interpretation for explicit provisioning and consumers."""
import os
from pathlib import Path


def dependency_root(root):
    return Path(os.environ.get("CODET_DEPENDENCY_ROOT", root / ".deps")).resolve()
