# GCC 14.2.0, cctools, Tiger CRT and MacOSX10.4u.sdk; pinned for reproducibility.
TOOLCHAIN_IMAGE := ghcr.io/variantxyz/gcc-powerpc-apple-darwin8@sha256:a9013745ae4a696dc3a047675a85e7c43b9453cdb1e26d9a7ac9738587c1e198
PPC_PREFIX ?= /usr/local/bin/powerpc-apple-darwin8-
TIGER_SDK ?= /usr/local/MacOSX10.4u.sdk
TARGET_FLAGS := -m32 -mcpu=750 -mmacosx-version-min=10.4 -isysroot $(TIGER_SDK)
