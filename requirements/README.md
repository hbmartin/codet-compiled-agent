# Development tool locks

These tools run on development/CI hosts, never on Tiger. The locks target Python 3.12,
with platform markers and distribution hashes for Linux and macOS. Provisioning uses
`pip --require-hashes --only-binary=:all:` and fails if no approved wheel is available.
The Semgrep environment additionally runs `pip check` and records its resolved environment.

`semgrep.in` pins the existing engine; `semgrep.lock` pins its full dependency closure.
`lint.in` and `lint.lock` pin Ruff, clang-format, zizmor and the PyYAML used by analyzer tests.
The Makefile separately pins actionlint and shfmt Go modules; ShellCheck is a host prerequisite
whose installed version is retained in lint evidence.

To deliberately refresh a lock with uv:

```sh
uv pip compile --universal --python-version 3.12 --generate-hashes --no-header --no-annotate \
  --output-file requirements/semgrep.lock requirements/semgrep.in
uv pip compile --universal --python-version 3.12 --generate-hashes --no-header --no-annotate \
  --output-file requirements/lint.lock requirements/lint.in
```

Review the resulting dependency changes and validate provisioning on both supported host
platforms. Run `make test-semgrep check-semgrep`, the enforcement-runner tests, and
`make check-lint` after relevant tool changes. Checks never resolve packages or fetch rules.
