SHELL := /bin/sh
.DEFAULT_GOAL := modern

include toolchain.mk

# Only this project is visible to the compiler. Source is read-only; build/
# is the sole writable bind mount. No original binary or recovery tree is needed.
DOCKER_MAKE = docker run --rm --pull=never --platform linux/amd64 \
	--network none --read-only --cap-drop ALL --security-opt no-new-privileges \
	--tmpfs /tmp:rw,noexec,nosuid,size=256m \
	--mount "type=bind,src=$(CURDIR),dst=/project,readonly" \
	--mount "type=bind,src=$(CURDIR)/build,dst=/project/build" \
	--workdir /project --entrypoint /usr/bin/make $(TOOLCHAIN_IMAGE) -f build.mk

.PHONY: all subset verify check-sources check-text-host pull clean help
all: modern

.PHONY: modern host test test-foundation test-sanitize fetch-deps fetch-tiger-deps toolchain-image tiger-sysroot tiger verify-tiger tiger-package legacy
modern: tiger
host:
	cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build build/host -j4
.PHONY: benchmark test-build-integrity developer-image
benchmark: host
	cmake --build build/host --target codet_persistence_bench -j4
	./build/host/codet_persistence_bench 512 durable
	./build/host/codet_persistence_bench 512 fast
	./build/host/codet_persistence_bench captures
	./build/host/codet_persistence_bench history
	./build/host/codet_persistence_bench provider
test-build-integrity:
	python3 -m unittest discover -s tests -p test_build_integrity.py -v
developer-image:
	docker build --no-cache --platform linux/amd64 -f cmake/Dockerfile.host-tests -t codet-host-tests:local .
test: host
	ctest --test-dir build/host --output-on-failure
MODERN_ANALYSIS_BUILD ?= build/modern-checks
MODERN_CC ?= clang
MODERN_CXX ?= $(or $(CODET_CLANGXX),clang++)
MODERN_ANALYSIS_JOBS ?= 4
MODERN_ANALYSIS_TIMEOUT ?= 300
ANALYSIS_PYTHON ?= python3
MODERN_ANALYSIS = $(ANALYSIS_PYTHON) scripts/analyze-modern.py --build-dir $(MODERN_ANALYSIS_BUILD) \
	--jobs $(MODERN_ANALYSIS_JOBS) --timeout $(MODERN_ANALYSIS_TIMEOUT)

.PHONY: modern-analysis-configure modern-analysis-host fetch-analysis-tools build-analysis-tools analyze-modern analyze-modern-clang-tidy analyze-modern-cppcheck \
	analyze-modern-clang-analyzer analyze-modern-selftest test-modern-analysis-runner
modern-analysis-host: modern-analysis-configure
	cmake --build $(MODERN_ANALYSIS_BUILD) --target all codet_persistence_bench codet_sanitization_bench -j$(MODERN_ANALYSIS_JOBS)
modern-analysis-configure:
	cmake -S . -B $(MODERN_ANALYSIS_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=Debug \
	  -DCMAKE_C_COMPILER=$(MODERN_CC) -DCMAKE_CXX_COMPILER=$(MODERN_CXX) \
	  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
analyze-modern: modern-analysis-configure
	$(MODERN_ANALYSIS) --tool all
analyze-modern-clang-tidy: modern-analysis-configure
	$(MODERN_ANALYSIS) --tool clang-tidy
analyze-modern-cppcheck: modern-analysis-configure
	$(MODERN_ANALYSIS) --tool cppcheck
analyze-modern-clang-analyzer: modern-analysis-configure
	$(MODERN_ANALYSIS) --tool clang-analyzer
analyze-modern-selftest: modern-analysis-configure
	$(MODERN_ANALYSIS) --selftest
fetch-analysis-tools:
	$(ANALYSIS_PYTHON) scripts/analysis-tools.py fetch
build-analysis-tools:
	$(ANALYSIS_PYTHON) scripts/analysis-tools.py build --jobs $(MODERN_ANALYSIS_JOBS)
test-modern-analysis-runner:
	$(ANALYSIS_PYTHON) -m unittest discover -s tests -p test_analyze_modern.py -v
SEMGREP_VERSION := 1.177.0
SEMGREP_PYTHON ?= python3.12
SEMGREP ?= $(CURDIR)/build/semgrep-venv/bin/semgrep
SEMGREP_RUN = SEMGREP_ENABLE_VERSION_CHECK=0 SEMGREP_SEND_METRICS=off $(SEMGREP)
SEMGREP_FLAGS = --metrics=off --disable-version-check --strict --error --no-rewrite-rule-ids
.PHONY: fetch-semgrep semgrep-environment semgrep-ready check-semgrep
# Provisioning is explicit; neither check target downloads tools or rules.
fetch-semgrep:
	$(SEMGREP_PYTHON) -c 'import sys; sys.exit("Provisioning requires Python 3.12; set SEMGREP_PYTHON to a compatible interpreter.") if sys.version_info[:2] != (3, 12) else None'
	$(SEMGREP_PYTHON) -m venv build/semgrep-venv
	build/semgrep-venv/bin/python -m pip install --disable-pip-version-check --only-binary=:all: \
	  --require-hashes -r requirements/semgrep.lock
	build/semgrep-venv/bin/python -m pip check
	$(MAKE) semgrep-environment
semgrep-environment:
	@mkdir -p build/semgrep
	build/semgrep-venv/bin/python -m pip freeze --all > build/semgrep/requirements-resolved.txt
	build/semgrep-venv/bin/python scripts/semgrep-report.py environment > build/semgrep/environment.json
semgrep-ready:
	@test -x '$(SEMGREP)' || { echo 'Run make fetch-semgrep first (or set SEMGREP to the pinned executable).'; exit 1; }
	@codet_semgrep_version=$$($(SEMGREP_RUN) --version) && test "$$codet_semgrep_version" = '$(SEMGREP_VERSION)' || { echo 'Semgrep $(SEMGREP_VERSION) is required.'; exit 1; }
	@mkdir -p build/semgrep
check-semgrep: semgrep-ready
	@rm -f build/semgrep/tests.json build/semgrep/test.stderr.log build/semgrep/test.manifest.json build/semgrep/scan.manifest.json
	@$(SEMGREP_RUN) scan $(SEMGREP_FLAGS) --config checks/semgrep --json \
	  --include '*.c' --include '*.cc' --include '*.cpp' --include '*.cxx' \
	  --include '*.h' --include '*.hh' --include '*.hpp' --include '*.hxx' \
	  src/modern src/text_compat.c include/codet include/text_compat.h \
	  > build/semgrep/scan.json 2> build/semgrep/scan.stderr.log || \
	  { codet_status=$$?; $(SEMGREP_PYTHON) scripts/semgrep-report.py failure build/semgrep/scan.json || :; \
	    cat build/semgrep/scan.stderr.log >&2; exit $$codet_status; }

LINT_PYTHON ?= python3.12
LINT_GO_VERSION := $(shell cat .go-version)
LINT_GO = GOTOOLCHAIN=go$(LINT_GO_VERSION) go
.PHONY: fetch-lint-tools check-lint check-format check-python check-shell check-workflows test-lint-runners
fetch-lint-tools:
	$(LINT_PYTHON) -c 'import sys; sys.exit("Provisioning requires Python 3.12; set LINT_PYTHON to a compatible interpreter.") if sys.version_info[:2] != (3, 12) else None'
	@mkdir -p build/lint-tools/bin build/lint
	$(LINT_GO) version > build/lint/go-version.txt
	@read codet_banner codet_command codet_version codet_platform < build/lint/go-version.txt; \
	  test "$$codet_banner $$codet_command $$codet_version" = 'go version go$(LINT_GO_VERSION)' && test -n "$$codet_platform" || \
	  { echo 'Go $(LINT_GO_VERSION) is required; install Go 1.21+ with toolchain selection support.' >&2; exit 1; }
	$(LINT_PYTHON) -m venv build/lint-venv
	build/lint-venv/bin/python -m pip install --disable-pip-version-check --only-binary=:all: \
	  --require-hashes -r requirements/lint.lock
	GOCACHE='$(CURDIR)/build/lint-tools/go-cache' GOBIN='$(CURDIR)/build/lint-tools/bin' $(LINT_GO) install github.com/rhysd/actionlint/cmd/actionlint@v1.7.12
	GOCACHE='$(CURDIR)/build/lint-tools/go-cache' GOBIN='$(CURDIR)/build/lint-tools/bin' $(LINT_GO) install mvdan.cc/sh/v3/cmd/shfmt@v3.14.1
	@command -v shellcheck >/dev/null || { echo 'Install ShellCheck (apt install shellcheck or brew install shellcheck).'; exit 1; }
check-lint:
	$(LINT_PYTHON) scripts/lint.py all
check-format:
	$(LINT_PYTHON) scripts/lint.py format
check-python:
	$(LINT_PYTHON) scripts/lint.py python
check-shell:
	$(LINT_PYTHON) scripts/lint.py shell
check-workflows:
	$(LINT_PYTHON) scripts/lint.py workflows
test-lint-runners:
	$(LINT_PYTHON) -m unittest discover -s tests -p test_semgrep_report.py -v
	$(LINT_PYTHON) -m unittest discover -s tests -p test_lint.py -v

test-foundation:
	cmake -S . -B build/foundation -DCODET_FOUNDATION_ONLY=ON
	cmake --build build/foundation -j4
	ctest --test-dir build/foundation --output-on-failure
test-sanitize:
	cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DCODET_SANITIZERS=ON
	cmake --build build/sanitize -j4
	ctest --test-dir build/sanitize --output-on-failure
fetch-deps:
	python3 scripts/fetch-deps.py
fetch-tiger-deps:
	python3 scripts/fetch-deps.py --tiger
toolchain-image:
	python3 scripts/tiger.py image
tiger-sysroot:
	python3 scripts/tiger.py sysroot
tiger:
	python3 scripts/tiger.py build
verify-tiger: tiger
	python3 scripts/tiger.py verify
tiger-package: verify-tiger
	python3 scripts/tiger.py package
legacy: app

.PHONY: app
app: | build
	$(DOCKER_MAKE) -j4 verify

subset: | build
	$(DOCKER_MAKE) verify-subset

verify: app

check-sources: | build
	$(DOCKER_MAKE) check-sources

# Explicit host-only unit check: no application sources, hooks or PPC code.
HOST_CC ?= cc
check-text-host: | build
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Wformat=2 -Werror -Iinclude \
	  src/text_compat.c checks/text_compat.c -o build/text-compat-host-check
	./build/text-compat-host-check

pull:
	docker pull --platform linux/amd64 $(TOOLCHAIN_IMAGE)

build:
	mkdir -p build

clean:
	rm -rf -- build

help:
	@printf '%s\n' 'make [modern]     Cross-build the modern C++ application and GoogleTest executable.' \
	  'make host / test   Build the modern host application / run hermetic tests.' \
	  'make test-sanitize Run host tests with ASan and UBSan.' \
	  'make fetch-lint-tools Provision pinned formatter, Ruff, workflow and shell-format tools (network).' \
	  'make check-lint    Run offline formatting, Python, shell and workflow gates independently of builds.' \
	  'make fetch-analysis-tools / build-analysis-tools Provision checksum-pinned Cppcheck 2.22.0.' \
	  'make analyze-modern Configure a dedicated Clang database and run all three analyzers without building.' \
	  'make analyze-modern-{clang-tidy,cppcheck,clang-analyzer} Run one modern analyzer.' \
	  'make analyze-modern-selftest / test-modern-analysis-runner Verify detection and runner failure handling.' \
	  'make fetch-semgrep Explicitly provision development-only Semgrep 1.177.0.' \
	  'make check-semgrep Run pinned local policies on production sources and headers offline.' \
	  'make fetch-tiger-deps / toolchain-image Fetch pinned sources / prepare build utilities (network).' \
	  'make tiger-sysroot Build pinned target dependencies offline.' \
	  'make verify-tiger / tiger-package Statically verify / package the modern PPC artifacts.' \
	  'make legacy / app Compile and verify the historical recovered C application.' \
	  'make pull         Download the pinned GCC 14.2 / Tiger 10.4u SDK image.' \
	  'make verify       Build as needed and inspect the PowerPC outputs.' \
	  'make subset       Build and verify the separate 19-function library and toolchain probe.' \
	  'make check-sources Check all application C syntax and save compiler diagnostics.' \
	  'make check-text-host Test only the two new text helpers against host libc; no app code.' \
	  'make clean        Remove only generated build outputs.'
