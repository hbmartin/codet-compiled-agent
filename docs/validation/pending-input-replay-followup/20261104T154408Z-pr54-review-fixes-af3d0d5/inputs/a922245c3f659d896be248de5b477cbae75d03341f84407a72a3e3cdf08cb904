# Direct cross-compilation recipes, used by the container entry point.
SHELL := /bin/sh
.DEFAULT_GOAL := app
.DELETE_ON_ERROR:

include toolchain.mk

CC = $(PPC_PREFIX)gcc
AR = $(PPC_PREFIX)ar
NM = $(PPC_PREFIX)nm
OTOOL = $(PPC_PREFIX)otool
CPPFLAGS += -Iinclude
CFLAGS ?= -O2 -fno-strict-aliasing
COMPILE_FLAGS = $(TARGET_FLAGS) -std=gnu11 $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Werror
SUBSYSTEMS := $(filter-out src/subsystems/abi-semantics.c src/subsystems/recovered-storage.c src/subsystems/runtime_startup.c src/subsystems/stdio_abi_stubs.c,$(wildcard src/subsystems/*.c))
APP_SOURCES := $(SUBSYSTEMS) src/storage.c src/text_compat.c
APP_OBJECTS := $(patsubst src/%.c,build/app/%.o,$(APP_SOURCES))
APP_FLAGS = $(TARGET_FLAGS) -std=gnu11 $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Wformat=2 -Werror
APP_LIBS = -lcurl -lsqlite3 -lncurses -lz -lm

# Machine-readable inputs for analysis; keep APP_SOURCES the single source list.
.PHONY: analysis-sources analysis-flags
analysis-sources:
	@printf '%s\n' $(APP_SOURCES)

analysis-flags:
	@printf '%s\n' '$(CC)' $(TARGET_FLAGS) $(CPPFLAGS)

.PHONY: app
app: build/ppccode.ppc build/layout.o

build/app/%.o: src/%.c $(wildcard include/*.h) build.mk toolchain.mk
	mkdir -p $(dir $@)
	$(CC) $(APP_FLAGS) -MMD -MP -c $< -o $@

build/ppccode.ppc: $(APP_OBJECTS)
	$(CC) $(TARGET_FLAGS) $(LDFLAGS) $(APP_OBJECTS) $(APP_LIBS) $(LDLIBS) -o $@
	chmod 644 $@

-include $(APP_OBJECTS:.o=.d)

.PHONY: all subset verify verify-subset check-sources
all: app
subset: build/librecovered-subset.a build/layout.o build/toolchain-link-probe.ppc build/subset.s

build:
	mkdir -p build

build/subset.o: src/subset.c include/recovered-types.h build.mk toolchain.mk | build
	$(CC) $(COMPILE_FLAGS) -c $< -o $@

build/layout.o: checks/layout.c include/recovered-types.h build.mk toolchain.mk | build
	$(CC) $(COMPILE_FLAGS) -c $< -o $@

build/subset.s: src/subset.c include/recovered-types.h build.mk toolchain.mk | build
	$(CC) $(COMPILE_FLAGS) -S $< -o $@

build/librecovered-subset.a: build/subset.o
	ZERO_AR_DATE=1 $(AR) rcs $@ $<

build/toolchain-link-probe.ppc: checks/link.c build/subset.o build.mk toolchain.mk
	$(CC) $(COMPILE_FLAGS) $(LDFLAGS) checks/link.c build/subset.o $(LDLIBS) -o $@
	chmod 644 $@

verify-subset: subset
	@$(CC) --version | head -1
	@test "$$($(CC) -dumpmachine)" = powerpc-apple-darwin8
	@$(CC) $(TARGET_FLAGS) -dM -E -x c /dev/null > build/target-macros.txt
	@grep -qx '#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ 1040' build/target-macros.txt
	@grep -qx '#define __SIZEOF_POINTER__ 4' build/target-macros.txt
	@grep -qx '#define __BIG_ENDIAN__ 1' build/target-macros.txt
	@$(NM) -g build/subset.o > build/subset-symbols.txt
	@awk '$$2 == "T" { count++ } END { exit count != 19 }' build/subset-symbols.txt
	@grep -Fq '_vsnprintf$$LDBL128' build/subset-symbols.txt
	@set -eu; for item in subset.o layout.o toolchain-link-probe.ppc; do \
	  header=$$(od -An -tx1 -N16 "build/$$item" | tr -d '[:space:]'); \
	  case "$$item" in *.o) expected=feedface000000120000000900000001 ;; \
	    *) expected=feedface000000120000000900000002 ;; esac; \
	  test "$$header" = "$$expected"; \
	done
	file build/subset.o build/librecovered-subset.a build/toolchain-link-probe.ppc
	$(OTOOL) -L build/toolchain-link-probe.ppc
	@printf '%s\n' 'Verified 19 functions, Tiger SDK layouts, PPC32 Mach-O headers and linking. Target code was not executed.'

verify: app
	@$(CC) --version | head -1
	@test "$$($(CC) -dumpmachine)" = powerpc-apple-darwin8
	@test "$$($(CC) -dumpfullversion)" = 14.2.0
	@$(CC) $(TARGET_FLAGS) -dM -E -x c /dev/null > build/target-macros.txt
	@grep -qx '#define __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ 1040' build/target-macros.txt
	@grep -qx '#define __SIZEOF_POINTER__ 4' build/target-macros.txt
	@grep -qx '#define __BIG_ENDIAN__ 1' build/target-macros.txt
	@test "$$(od -An -tx1 -N16 build/ppccode.ppc | tr -d '[:space:]')" = feedface000000120000000900000002
	@$(NM) -g build/ppccode.ppc > build/app-symbols.txt
	@$(NM) -m build/ppccode.ppc > build/app-symbols-detailed.txt
	@$(NM) -u build/ppccode.ppc > build/app-imports.txt
	@awk '$$2 ~ /^\(__TEXT,__text/ && $$3 == "external" { sub(/^_/, "", $$4); print $$4 }' build/app-symbols-detailed.txt | LC_ALL=C sort -u > build/app-functions.txt
	@LC_ALL=C comm -23 checks/app-symbols.txt build/app-functions.txt > build/missing-functions.txt
	@test ! -s build/missing-functions.txt || { cat build/missing-functions.txt; exit 1; }
	@$(OTOOL) -hv build/ppccode.ppc > build/macho-header.txt
	@$(OTOOL) -L build/ppccode.ppc > build/dependencies.txt
	@$(OTOOL) -l build/ppccode.ppc > build/load-commands.txt
	@awk 'NR > 1 && $$1 !~ /^\/usr\/lib\// { bad = 1 } END { exit bad }' build/dependencies.txt
	@# Inspect mode bits: emulated access(2) on Docker Desktop can report X_OK for 0644 files.
	@test "$$(LC_ALL=C ls -l build/ppccode.ppc | cut -c1-10)" = '-rw-r--r--'
	file build/ppccode.ppc
	@cat build/dependencies.txt
	@printf '%s\n' 'Verified all 362 recovered application functions plus 3 port helpers, SDK layouts, PPC32/G3 Mach-O, Tiger target and system-library links. Target code was not executed.'

# This target fails when any copied subsystem still has compiler errors.
# ABI semantic notation and raw storage are retained as source but are not
# application translation units; compiler/CRT code replaces runtime stubs.
check-sources: | build
	@mkdir -p build/diagnostics
	@rm -f build/diagnostics/*.log
	@failed=0; for source in $(APP_SOURCES); do \
	  name=$${source##*/}; \
	  if $(CC) $(APP_FLAGS) -fsyntax-only "$$source" > "build/diagnostics/$${name%.c}.log" 2>&1; then \
	    printf 'PASS: %s\n' "$$source"; \
	  else printf 'FAIL: %s (see build/diagnostics/%s.log)\n' "$$source" "$${name%.c}"; failed=1; fi; \
	done; exit "$$failed"
