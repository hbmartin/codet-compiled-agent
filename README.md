# Codet: a C++17 coding agent for PowerPC Tiger

Codet is an **unofficial C++17 adaptation of the official Codex CLI**, implemented as a single-agent terminal coding assistant targeting **Mac OS X 10.4 / PPC32**, with acceptance testing on a **G4 with 512 MB RAM**. It provides a minimal ncurses interface, an editable line REPL, and a Codex-shaped headless CLI.

The modern application retains Anthropic, OpenAI, Google, and Ollama providers and adds imported ChatGPT/Codex subscription credentials with refresh. It exposes Codex core tool contracts, yielding shell/PTY sessions, patches, image inputs, asynchronous terminal interaction, and native Tiger computer use through screenshots, accessibility, input events, clipboard and AppleScript. Sessions, memory previews/consolidation, hooks and text/JSON exports remain available.

Codex subscription access remains the default, using imported credentials and the private ChatGPT transport. This unofficial compatibility boundary can change independently of public APIs; API-key providers are available as fallbacks. See [reliability, migration and provider behavior](docs/RELIABILITY-MODERNIZATION.md).

**Verification boundary:** host fixtures and cross-build/static checks are separate from target execution. Tiger runtime and live provider acceptance must be established using the owner-run test bundle. No build or packaging target executes PPC code.

## Develop and test

```sh
make fetch-deps       # Explicit network step; locked sources and SHA-256 checks
make test-foundation  # GoogleTest 1.17.0 + GoogleMock, existing text-helper tests
make test             # Host unit, integration, CLI and terminal tests
make test-sanitize    # ASan + UBSan
make analyze-modern  # Separate Clang configuration; GCC host cache is preserved
make fetch-lint-tools # Explicit network step; Python 3.12.x, Go and ShellCheck required
make check-lint      # Offline C/C++ formatting, Ruff, shell and workflow checks
make fetch-semgrep    # Explicit network step; development-only Semgrep 1.177.0
make check-semgrep    # Every maintained production source/header; no downloads
./build/host/codet --help
```

Host dependencies: CMake 3.20+, C++17 compiler, Python 3.12+, curl 7.85+, OpenSSL, SQLite 3.24+, ncurses and libedit development packages. See [build and interface documentation](docs/MODERN-AGENT.md).

`make analyze-modern` runs Clang-Tidy, Cppcheck and Clang Static Analyzer against the modern
application and shared C text helpers in a separate Clang build. Ubuntu GitHub Actions runs the
same checks and host tests. See [analysis setup, reports and enforcement](docs/MODERN-ANALYSIS.md).

Semgrep uses an ignored `build/semgrep-venv` environment; provisioning requires Python with
`venv`/pip support (`python3-venv` on Ubuntu). Provisioning defaults to `python3.12`; locked tool environments require Python 3.12.x. Select a compatible interpreter with
`SEMGREP_PYTHON=/path/to/python3`, or an existing pinned tool with `SEMGREP=/path/to/semgrep`.
Checks require version 1.177.0, use only checked-in rules, disable metrics/version checks,
and fail on findings or analysis errors. They guard discovery, checkpoint inspection,
provider reasoning and terminal-event handling, provider-cache publication, catalog
descriptor lifetimes, and transaction/reference-write patterns.
Runtime regressions cover recovery semantics. Provisioning records `pip freeze --all` and
interpreter/platform details, with repository-relative interpreter paths or an
`<external>/basename` marker; interpreter symlinks retain their virtual-environment path.
`make semgrep-environment` refreshes that evidence for the
provisioned environment without installing anything. The complete Semgrep dependency closure is
version/hash locked in `requirements/semgrep.lock`; provisioning accepts wheels only and runs `pip check`.
Production policies run directly through the pinned Semgrep CLI with strict errors and findings-as-failure.
The scan covers native files under the production source/header roots using explicit extension filters.
There are no rule fixtures, custom scan validators, or per-rule coverage manifests. File-specific rules
still require their path filters to be maintained when files move; a successful scan does not prove
that every rule selected a production target. JSON and stderr reports are retained under
`build/semgrep/`, with pretty-printed failure diagnostics and the original failing exit status,
and uploaded by an independent Ubuntu policy job. Application builds and
Tiger binaries have no Semgrep dependency.

`make check-lint` uses pinned clang-format 18.1.8, Ruff 0.17.1, actionlint 1.7.12,
zizmor 1.30.1 and shfmt 3.14.1, plus an installed ShellCheck. Install ShellCheck with
`apt install shellcheck` or `brew install shellcheck`; Go 1.27.2 is selected from `.go-version` during explicit provisioning, without automatic upgrades beyond that version. The selected version is checked before tool installation and recorded in `build/lint/go-version.txt`.
Individual offline targets are `check-format`, `check-python`, `check-shell`, and `check-workflows`.
Formatting covers maintained production code, headers, tests and benchmarks; recovered C
and dependencies retain their separate policies.
Ruff covers scripts and Python tests with undefined-name/import, selected correctness, and
Python 3.12 modernization checks. `LINT_PYTHON` defaults to `python3.12` for locked lint provisioning.
Checks, runner tests, and Semgrep failure summaries use `CHECK_PYTHON` (default `python3`)
and do not require the provisioning interpreter. Override it with `CHECK_PYTHON=/path/to/python3`.
Lint versions are parsed and compared exactly; ShellCheck remains an installed, unpinned prerequisite.
Requested reports are invalidated before discovery and atomically published, so interruptions cannot
leave previous passing evidence for unfinished groups. Detailed versions, commands and results live under `build/lint/`.
CI runs lint and policy checks independently of the native build; its `Required checks` job requires
all three mandatory jobs to succeed. Select that aggregate job in repository branch rules when enforcing merges.
To use the provisioned PyYAML for analyzer-runner tests, run
`make test-modern-analysis-runner ANALYSIS_PYTHON=build/lint-venv/bin/python`.

## Cross-build for Tiger

```sh
make fetch-tiger-deps # Fetch locked runtime sources and TigerPorts patches
make toolchain-image # Pinned GCC 14.2 / Tiger SDK + build utilities
make tiger-sysroot   # Offline dependency build
make                 # Default: modern PPC application and test executable
make verify-tiger    # Mach-O, architecture and dependency-path verification
make tiger-package   # build/dist/codet-tiger-ppc.tar.gz
```

The modern build statically links compiler support and dynamically links TigerPorts C libraries under `/opt/tigr` by default. Set `TIGERPORTS_PREFIX` consistently to target another installation. Runtime libraries and certificates must be installed on the target.

## Use

```sh
codet login --import /explicit/path/to/token-file.json
codet --ui line
codet exec --provider openai --model YOUR_MODEL --json 'explain this repository'
codet exec -i screenshot.png 'explain this image'
codet exec resume --last 'continue'
codet memory consolidate
```

State lives under `CODET_HOME` or `~/.codet`. No existing Codex or Claurst files are imported implicitly. Native sign-in is deferred; API-key providers are the explicit fallback if subscription transport cannot be validated. Application permission rules do not provide an OS shell sandbox. Content search uses system `grep` through exec_command. Select `--permission-mode bypass` for auto/no-approval operation, or change modes live with `/permissions MODE`. Explicit deny rules retain precedence.

Interactive workflows include `/model [MODEL [EFFORT]]`, `/status`, and `/compact`. Instructions use Codex filename precedence, and skills are discovered lazily with bounded metadata.

Preserved-file notices keep readable Unicode paths and selectively escape unsafe bytes and
terminal controls. This display policy does not guarantee visually unique Unicode paths.
Emergency output uses reversible ASCII byte escapes for the complete path.

## Documentation

- [Codex tool contracts and native Tiger computer use](docs/CODEX-TOOLS.md)
- [Captured Codex prompts and Tiger adaptations](docs/PROMPTS.md)
- [Modern build, configuration, CLI and tools](docs/MODERN-AGENT.md)
- [Modern static analysis and Ubuntu CI](docs/MODERN-ANALYSIS.md)
- [Feature/reference/test ledger](docs/FEATURE-PORT.md)
- [Tiger acceptance and resource measurements](docs/TIGER-ACCEPTANCE.md)
- [Recovered C build notes](docs/BUILD-NOTES.md)
- [Recovered C static-analysis evidence](docs/ANALYSIS.md)

The reconstructed C application remains available with `make legacy` / `make app`, including its original static verification. The modern production source list excludes its RAG, SQL tools, small-model routing, subagents and MCP paths. Historical source annotations remain intact. Existing recovered-C runtime limitations in the build/analysis notes still apply to that legacy target.

## License

[GPL v3](LICENSE.md). Dependency versions, source locations, hashes and patch provenance are recorded in the lock files; release bundles include notices for statically linked C++ dependencies.
