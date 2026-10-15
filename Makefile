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
test: host
	ctest --test-dir build/host --output-on-failure
.PHONY: analyze-modern
analyze-modern:
	python3 scripts/analyze-modern.py
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
	  'make analyze-modern Run Clang analysis using a separate Clang configuration.' \
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
